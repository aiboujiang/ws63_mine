/**
 * @file wk_zw101_test.c
 * @brief WK2114 子串口1挂载 ZW101 的串口命令测试实现。
 *
 * 串口命令（UART0 输入）：
 * - ZW101 ENROLL <id> [times]
 * - ZW101 VERIFY [score] [id]
 * - ZW101 DEL <id> [count]
 *
 * 设计说明：
 * 1) 模块通过 WK2114 子串口1与 ZW101 通信；
 * 2) 命令执行采用“发送后轮询收包”的同步模型，避免因等待 ACK 期间漏喂解析器；
 * 3) 自动录入/识别等长耗时命令统一走超时与错误码日志，便于现场排障。
 */

#include "wk_zw101_test.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "osal_debug.h"
#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"

#include "mine_wk2114_uart2_ext.h"
#include "ZW101/zw101_protocol.h"

/* ZW101 挂接的 WK2114 子串口号。 */
#define WK_ZW101_SUB_PORT 1U
/* UART0 作为命令输入调试口。 */
#define WK_ZW101_DEBUG_UART_BUS UART_BUS_0
#define WK_ZW101_DEBUG_UART_TX_PIN 17
#define WK_ZW101_DEBUG_UART_RX_PIN 18
#define WK_ZW101_DEBUG_UART_PIN_MODE 1
#define WK_ZW101_DEBUG_UART_BAUD 115200

/* 收包轮询参数：单次最多读取 16 字节（WK2114 FIFO 命令限制）。 */
#define WK_ZW101_RX_CHUNK_MAX 16U
/* 命令口单次读取缓冲长度。 */
#define WK_ZW101_DEBUG_READ_CHUNK_MAX 32U
/* 命令行最大长度。 */
#define WK_ZW101_DEBUG_LINE_MAX 96U
/* 命令口 UART RX 缓冲长度。 */
#define WK_ZW101_DEBUG_UART_RX_BUFFER_SIZE 256U

/* 设备探测重试参数。 */
#define WK_ZW101_PROBE_RETRY_MAX 3U
#define WK_ZW101_PROBE_RETRY_GAP_MS 40U
/* 未就绪时的后台重探测间隔。 */
#define WK_ZW101_REPROBE_INTERVAL_MS 1500U

/* 命令超时参数。 */
#define WK_ZW101_TIMEOUT_COMMON_MS ZW101_COMMON_TIMEOUT
#define WK_ZW101_TIMEOUT_AUTO_MS ZW101_AUTO_TIMEOUT

/* 自动录入/识别默认参数。 */
#define WK_ZW101_AUTO_ENROLL_TIMES_DEFAULT 3U
#define WK_ZW101_AUTO_ENROLL_PARAM_DEFAULT ((uint16_t)(1U << 4))
#define WK_ZW101_AUTO_VERIFY_SCORE_DEFAULT 3U
#define WK_ZW101_AUTO_VERIFY_TARGET_DEFAULT 0xFFFFU
#define WK_ZW101_AUTO_VERIFY_PARAM_DEFAULT 0x0000U

#define WK_ZW101_MATCH_ID_INVALID 0xFFFFU

/**
 * @brief 命令执行结果快照。
 */
typedef struct {
    uint8_t ack_code;
    uint8_t payload[ZW101_PROTOCOL_RCV_BUFFER_SIZE];
    uint16_t payload_len;
} wk_zw101_ack_result_t;

static zw101_context_t g_wk_zw101_ctx;
static uint8_t g_wk_zw101_ready = 0U;
static uint32_t g_wk_zw101_next_probe_ms = 0U;

/* UART0 命令行缓存。 */
static char g_wk_zw101_debug_line[WK_ZW101_DEBUG_LINE_MAX] = {0};
static uint16_t g_wk_zw101_debug_line_len = 0U;
static uint8_t g_wk_zw101_debug_uart_rx_buf[WK_ZW101_DEBUG_UART_RX_BUFFER_SIZE] = {0};

/**
 * @brief 统一返回 ACK 码可读文本。
 */
static const char *wk_zw101_ack_desc(uint8_t ack_code)
{
    switch (ack_code) {
        case ZW101_PS_OK:
            return "OK";
        case ZW101_PS_NO_FINGER:
            return "NO_FINGER";
        case ZW101_PS_NOT_MATCH:
            return "NOT_MATCH";
        case ZW101_PS_NOT_SEARCHED:
            return "NOT_FOUND";
        case ZW101_PS_ADDRESS_OVER:
            return "ADDRESS_OVER";
        case ZW101_PS_ENROLL_ERR:
            return "ENROLL_ERR";
        case ZW101_PS_LIB_FULL_ERR:
            return "LIB_FULL";
        case ZW101_PS_TMPL_NOT_EMPTY:
            return "TMPL_NOT_EMPTY";
        case ZW101_PS_TMPL_EMPTY:
            return "TMPL_EMPTY";
        case ZW101_PS_TIME_OUT:
            return "TIMEOUT";
        case ZW101_PS_FP_DUPLICATION:
            return "DUPLICATION";
        case ZW101_PS_ENROLL_CANCEL:
            return "ENROLL_CANCEL";
        case ZW101_PS_IMAGE_SMALL:
            return "IMAGE_SMALL";
        case ZW101_PS_IMAGE_UNAVAILABLE:
            return "IMAGE_UNAVAILABLE";
        case ZW101_PS_ENROLL_TIMES_NOT_ENOUGH:
            return "ENROLL_TIMES_NOT_ENOUGH";
        case ZW101_PS_COMMUNICATE_TIMEOUT:
            return "COMM_TIMEOUT";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 将字符串原地转大写，便于命令匹配忽略大小写。
 */
static void wk_zw101_to_upper(char *text)
{
    uint16_t i;

    if (text == NULL) {
        return;
    }

    for (i = 0U; text[i] != '\0'; i++) {
        text[i] = (char)toupper((unsigned char)text[i]);
    }
}

/**
 * @brief 提取下一个空白分隔 token。
 */
static char *wk_zw101_next_token(char **cursor)
{
    char *start;

    if ((cursor == NULL) || (*cursor == NULL)) {
        return NULL;
    }

    while ((**cursor == ' ') || (**cursor == '\t')) {
        (*cursor)++;
    }

    if (**cursor == '\0') {
        return NULL;
    }

    start = *cursor;
    while ((**cursor != '\0') && (**cursor != ' ') && (**cursor != '\t')) {
        (*cursor)++;
    }

    if (**cursor != '\0') {
        **cursor = '\0';
        (*cursor)++;
    }

    return start;
}

/**
 * @brief 解析 u16 参数（支持十进制与 0x 前缀）。
 */
static uint8_t wk_zw101_parse_u16(const char *token, uint16_t *value)
{
    char *end = NULL;
    unsigned long tmp;

    if ((token == NULL) || (value == NULL)) {
        return 0U;
    }

    tmp = strtoul(token, &end, 0);
    if ((end == token) || (*end != '\0') || (tmp > 0xFFFFUL)) {
        return 0U;
    }

    *value = (uint16_t)tmp;
    return 1U;
}

/**
 * @brief 解析 u8 参数。
 */
static uint8_t wk_zw101_parse_u8(const char *token, uint8_t *value)
{
    uint16_t tmp = 0U;

    if (!wk_zw101_parse_u16(token, &tmp)) {
        return 0U;
    }

    if (tmp > 0xFFU) {
        return 0U;
    }

    *value = (uint8_t)tmp;
    return 1U;
}

/**
 * @brief 子串口1发送适配：按 16 字节块写入 WK2114 FIFO。
 */
static int wk_zw101_uart_send_adapter(const uint8_t *data, uint16_t size)
{
    uint16_t offset = 0U;
    uint16_t remain;
    uint8_t chunk;

    if ((data == NULL) || (size == 0U)) {
        return -1;
    }

    while (offset < size) {
        remain = (uint16_t)(size - offset);
        chunk = (remain > WK_ZW101_RX_CHUNK_MAX) ? WK_ZW101_RX_CHUNK_MAX : (uint8_t)remain;
        WkWriteSFifo(WK_ZW101_SUB_PORT, (uint8_t *)&data[offset], chunk);
        offset = (uint16_t)(offset + chunk);
    }

    return 0;
}

/**
 * @brief 获取毫秒 tick 的 HAL 适配。
 */
static uint32_t wk_zw101_get_tick_ms_adapter(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

/**
 * @brief 延时 HAL 适配。
 */
static void wk_zw101_delay_ms_adapter(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 轮询子串口1接收 FIFO 并喂入协议解析器。
 */
static void wk_zw101_poll_subport_rx_once(void)
{
    uint8_t rx_cnt;
    uint8_t chunk;
    uint8_t rx_buf[WK_ZW101_RX_CHUNK_MAX] = {0};

    rx_cnt = WkReadSReg(WK_ZW101_SUB_PORT, WK2XXX_RFCNT);
    while (rx_cnt > 0U) {
        chunk = (uint8_t)((rx_cnt > WK_ZW101_RX_CHUNK_MAX) ? WK_ZW101_RX_CHUNK_MAX : rx_cnt);
        WkReadSFifo(WK_ZW101_SUB_PORT, rx_buf, chunk);
        zw101_protocol_parse(&g_wk_zw101_ctx, rx_buf, chunk);
        rx_cnt = (uint8_t)(rx_cnt - chunk);
    }
}

/**
 * @brief 发送命令并在本地轮询收包直到 ACK 或超时。
 *
 * 关键点：
 * 1) 先调用 zw101_send_command 建立等待状态；
 * 2) 在等待循环中持续轮询子串口并喂解析器；
 * 3) 统一在超时时回填 TIMEOUT 确认码，便于上层日志释义。
 */
static int wk_zw101_send_cmd_wait(uint8_t cmd,
    const uint8_t *params,
    uint16_t params_len,
    uint32_t timeout_ms,
    uint8_t skip_progress_ack,
    wk_zw101_ack_result_t *out_result)
{
    uint32_t start_ms;

    if (out_result != NULL) {
        out_result->ack_code = 0xFFU;
        out_result->payload_len = 0U;
        (void)memset_s(out_result->payload, sizeof(out_result->payload), 0, sizeof(out_result->payload));
    }

    g_wk_zw101_ctx.skip_autologin_progress = (skip_progress_ack != 0U);
    if (zw101_send_command(&g_wk_zw101_ctx, cmd, params, params_len) != 0) {
        g_wk_zw101_ctx.skip_autologin_progress = false;
        return -1;
    }

    start_ms = (uint32_t)uapi_systick_get_ms();
    while (!g_wk_zw101_ctx.ack_done) {
        wk_zw101_poll_subport_rx_once();
        if ((uint32_t)(uapi_systick_get_ms() - start_ms) >= timeout_ms) {
            g_wk_zw101_ctx.waiting_ack = false;
            g_wk_zw101_ctx.skip_autologin_progress = false;
            g_wk_zw101_ctx.ack_code = ZW101_PS_TIME_OUT;
            if (out_result != NULL) {
                out_result->ack_code = ZW101_PS_TIME_OUT;
            }
            return -1;
        }
        osal_msleep(1U);
    }

    g_wk_zw101_ctx.waiting_ack = false;
    g_wk_zw101_ctx.skip_autologin_progress = false;

    if (out_result != NULL) {
        out_result->ack_code = g_wk_zw101_ctx.ack_code;
        out_result->payload_len = g_wk_zw101_ctx.ack_payload_len;
        if (out_result->payload_len > sizeof(out_result->payload)) {
            out_result->payload_len = sizeof(out_result->payload);
        }
        if (out_result->payload_len > 0U) {
            (void)memcpy_s(out_result->payload,
                sizeof(out_result->payload),
                g_wk_zw101_ctx.ack_payload,
                out_result->payload_len);
        }
    }

    return (g_wk_zw101_ctx.ack_code == ZW101_PS_OK) ? 0 : -1;
}

/**
 * @brief 从 ACK 载荷读取 16 位大端字段。
 */
static uint16_t wk_zw101_payload_u16(const wk_zw101_ack_result_t *result, uint16_t offset)
{
    if ((result == NULL) || ((uint16_t)(offset + 1U) >= result->payload_len)) {
        return 0U;
    }

    return (uint16_t)(((uint16_t)result->payload[offset] << 8) | result->payload[offset + 1U]);
}

/**
 * @brief 探测设备可用性：握手 + 传感器检查。
 */
static errcode_t wk_zw101_probe_ready(void)
{
    uint8_t retry;
    wk_zw101_ack_result_t ack = {0};

    for (retry = 0U; retry < WK_ZW101_PROBE_RETRY_MAX; retry++) {
        if (wk_zw101_send_cmd_wait(ZW101_CMD_HANDSHAKE, NULL, 0U,
            WK_ZW101_TIMEOUT_COMMON_MS, 0U, &ack) == 0) {
            if (wk_zw101_send_cmd_wait(ZW101_CMD_CHECK_SENSOR, NULL, 0U,
                WK_ZW101_TIMEOUT_COMMON_MS, 0U, &ack) == 0) {
                g_wk_zw101_ready = 1U;
                osal_printk("[wk2114][zw101] ready on subport%u\r\n", (unsigned int)WK_ZW101_SUB_PORT);
                return ERRCODE_SUCC;
            }
        }

        osal_printk("[wk2114][zw101] probe retry %u/%u ack=0x%02X(%s)\r\n",
            (unsigned int)(retry + 1U),
            (unsigned int)WK_ZW101_PROBE_RETRY_MAX,
            (unsigned int)ack.ack_code,
            wk_zw101_ack_desc(ack.ack_code));
        osal_msleep(WK_ZW101_PROBE_RETRY_GAP_MS);
    }

    g_wk_zw101_ready = 0U;
    return ERRCODE_FAIL;
}

/**
 * @brief 执行录入命令。
 */
static void wk_zw101_cmd_enroll(uint16_t page_id, uint8_t enroll_times)
{
    uint8_t params[5] = {0};
    wk_zw101_ack_result_t ack = {0};

    if (g_wk_zw101_ready == 0U) {
        osal_printk("[wk2114][zw101] ENROLL reject: not ready\r\n");
        return;
    }

    if ((enroll_times < 2U) || (enroll_times > 6U)) {
        enroll_times = WK_ZW101_AUTO_ENROLL_TIMES_DEFAULT;
    }

    params[0] = (uint8_t)(page_id >> 8);
    params[1] = (uint8_t)page_id;
    params[2] = enroll_times;
    params[3] = (uint8_t)(WK_ZW101_AUTO_ENROLL_PARAM_DEFAULT >> 8);
    params[4] = (uint8_t)WK_ZW101_AUTO_ENROLL_PARAM_DEFAULT;

    osal_printk("[wk2114][zw101] ENROLL start id=%u times=%u\r\n",
        (unsigned int)page_id,
        (unsigned int)enroll_times);

    if (wk_zw101_send_cmd_wait(ZW101_CMD_AUTO_ENROLL,
        params,
        sizeof(params),
        WK_ZW101_TIMEOUT_AUTO_MS,
        1U,
        &ack) == 0) {
        osal_printk("[wk2114][zw101] ENROLL success id=%u\r\n", (unsigned int)page_id);
        return;
    }

    osal_printk("[wk2114][zw101] ENROLL fail ack=0x%02X(%s)\r\n",
        (unsigned int)ack.ack_code,
        wk_zw101_ack_desc(ack.ack_code));
}

/**
 * @brief 执行识别命令。
 */
static void wk_zw101_cmd_verify(uint8_t score_level, uint16_t target_id)
{
    uint8_t params[5] = {0};
    wk_zw101_ack_result_t ack = {0};
    uint16_t match_id;
    uint16_t score;

    if (g_wk_zw101_ready == 0U) {
        osal_printk("[wk2114][zw101] VERIFY reject: not ready\r\n");
        return;
    }

    if ((score_level < 1U) || (score_level > 5U)) {
        score_level = WK_ZW101_AUTO_VERIFY_SCORE_DEFAULT;
    }

    params[0] = score_level;
    params[1] = (uint8_t)(target_id >> 8);
    params[2] = (uint8_t)target_id;
    params[3] = (uint8_t)(WK_ZW101_AUTO_VERIFY_PARAM_DEFAULT >> 8);
    params[4] = (uint8_t)WK_ZW101_AUTO_VERIFY_PARAM_DEFAULT;

    osal_printk("[wk2114][zw101] VERIFY start level=%u id=0x%04X\r\n",
        (unsigned int)score_level,
        (unsigned int)target_id);

    if (wk_zw101_send_cmd_wait(ZW101_CMD_AUTO_MATCH,
        params,
        sizeof(params),
        WK_ZW101_TIMEOUT_AUTO_MS,
        0U,
        &ack) == 0) {
        match_id = wk_zw101_payload_u16(&ack, 1U);
        score = wk_zw101_payload_u16(&ack, 3U);
        osal_printk("[wk2114][zw101] VERIFY success id=%u score=%u\r\n",
            (unsigned int)match_id,
            (unsigned int)score);
        return;
    }

    if ((ack.ack_code == ZW101_PS_NOT_SEARCHED) || (ack.ack_code == ZW101_PS_NOT_MATCH)) {
        osal_printk("[wk2114][zw101] VERIFY no match ack=0x%02X(%s)\r\n",
            (unsigned int)ack.ack_code,
            wk_zw101_ack_desc(ack.ack_code));
        return;
    }

    osal_printk("[wk2114][zw101] VERIFY fail ack=0x%02X(%s)\r\n",
        (unsigned int)ack.ack_code,
        wk_zw101_ack_desc(ack.ack_code));
}

/**
 * @brief 执行删除命令。
 */
static void wk_zw101_cmd_delete(uint16_t page_id, uint16_t count)
{
    uint8_t params[4] = {0};
    wk_zw101_ack_result_t ack = {0};

    if (g_wk_zw101_ready == 0U) {
        osal_printk("[wk2114][zw101] DEL reject: not ready\r\n");
        return;
    }

    if (count == 0U) {
        count = 1U;
    }

    params[0] = (uint8_t)(page_id >> 8);
    params[1] = (uint8_t)page_id;
    params[2] = (uint8_t)(count >> 8);
    params[3] = (uint8_t)count;

    osal_printk("[wk2114][zw101] DEL start id=%u count=%u\r\n",
        (unsigned int)page_id,
        (unsigned int)count);

    if (wk_zw101_send_cmd_wait(ZW101_CMD_DEL_TEMPLATE,
        params,
        sizeof(params),
        WK_ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack) == 0) {
        osal_printk("[wk2114][zw101] DEL success id=%u count=%u\r\n",
            (unsigned int)page_id,
            (unsigned int)count);
        return;
    }

    osal_printk("[wk2114][zw101] DEL fail ack=0x%02X(%s)\r\n",
        (unsigned int)ack.ack_code,
        wk_zw101_ack_desc(ack.ack_code));
}

/**
 * @brief 打印可用命令帮助。
 */
static void wk_zw101_print_help(void)
{
    osal_printk("[wk2114][zw101] cmd help:\r\n");
    osal_printk("[wk2114][zw101]   ZW101 ENROLL <id> [times]\r\n");
    osal_printk("[wk2114][zw101]   ZW101 VERIFY [score] [id]\r\n");
    osal_printk("[wk2114][zw101]   ZW101 DEL <id> [count]\r\n");
}

/**
 * @brief 处理一行命令文本。
 */
static void wk_zw101_handle_line(const char *line)
{
    char cmd_line[WK_ZW101_DEBUG_LINE_MAX] = {0};
    char *cursor;
    char *token0;
    char *token1;
    char *token2;
    char *token3;
    uint16_t id = 0U;
    uint16_t count = 1U;
    uint16_t value = 0U;
    uint8_t times = WK_ZW101_AUTO_ENROLL_TIMES_DEFAULT;
    uint8_t score = WK_ZW101_AUTO_VERIFY_SCORE_DEFAULT;

    if (line == NULL) {
        return;
    }

    if (strncpy_s(cmd_line, sizeof(cmd_line), line, sizeof(cmd_line) - 1U) != EOK) {
        return;
    }

    wk_zw101_to_upper(cmd_line);
    cursor = cmd_line;
    token0 = wk_zw101_next_token(&cursor);
    token1 = wk_zw101_next_token(&cursor);
    token2 = wk_zw101_next_token(&cursor);
    token3 = wk_zw101_next_token(&cursor);

    if ((token0 == NULL) || (strcmp(token0, "ZW101") != 0)) {
        return;
    }

    if ((token1 == NULL) || (strcmp(token1, "HELP") == 0)) {
        wk_zw101_print_help();
        return;
    }

    if (strcmp(token1, "ENROLL") == 0) {
        if (!wk_zw101_parse_u16(token2, &id)) {
            osal_printk("[wk2114][zw101] usage: ZW101 ENROLL <id> [times]\r\n");
            return;
        }
        if ((token3 != NULL) && (!wk_zw101_parse_u8(token3, &times))) {
            osal_printk("[wk2114][zw101] invalid enroll times\r\n");
            return;
        }
        wk_zw101_cmd_enroll(id, times);
        return;
    }

    if (strcmp(token1, "VERIFY") == 0) {
        id = WK_ZW101_AUTO_VERIFY_TARGET_DEFAULT;

        if ((token2 != NULL) && (!wk_zw101_parse_u16(token2, &value))) {
            osal_printk("[wk2114][zw101] usage: ZW101 VERIFY [score] [id]\r\n");
            return;
        }

        if (token2 != NULL) {
            /* 单参数优先按 score(1~5) 解释，否则按 target id 解释。 */
            if ((value >= 1U) && (value <= 5U)) {
                score = (uint8_t)value;
            } else {
                id = value;
            }
        }

        if ((token3 != NULL) && (!wk_zw101_parse_u16(token3, &id))) {
            osal_printk("[wk2114][zw101] usage: ZW101 VERIFY [score] [id]\r\n");
            return;
        }

        wk_zw101_cmd_verify(score, id);
        return;
    }

    if (strcmp(token1, "DEL") == 0) {
        if (!wk_zw101_parse_u16(token2, &id)) {
            osal_printk("[wk2114][zw101] usage: ZW101 DEL <id> [count]\r\n");
            return;
        }
        if ((token3 != NULL) && (!wk_zw101_parse_u16(token3, &count))) {
            osal_printk("[wk2114][zw101] invalid del count\r\n");
            return;
        }
        wk_zw101_cmd_delete(id, count);
        return;
    }

    osal_printk("[wk2114][zw101] unknown cmd, send 'ZW101 HELP'\r\n");
}

/**
 * @brief 初始化 UART0 命令输入口。
 */
static errcode_t wk_zw101_debug_uart_init(void)
{
    uart_attr_t attr = {
        .baud_rate = WK_ZW101_DEBUG_UART_BAUD,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE,
    };
    uart_pin_config_t pin_cfg = {
        .tx_pin = WK_ZW101_DEBUG_UART_TX_PIN,
        .rx_pin = WK_ZW101_DEBUG_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE,
    };
    uart_buffer_config_t rx_buf_cfg = {
        .rx_buffer = g_wk_zw101_debug_uart_rx_buf,
        .rx_buffer_size = sizeof(g_wk_zw101_debug_uart_rx_buf),
    };

    (void)uapi_pin_set_mode(WK_ZW101_DEBUG_UART_TX_PIN, WK_ZW101_DEBUG_UART_PIN_MODE);
    (void)uapi_pin_set_mode(WK_ZW101_DEBUG_UART_RX_PIN, WK_ZW101_DEBUG_UART_PIN_MODE);

    (void)uapi_uart_deinit(WK_ZW101_DEBUG_UART_BUS);
    return uapi_uart_init(WK_ZW101_DEBUG_UART_BUS, &pin_cfg, &attr, NULL, &rx_buf_cfg);
}

/**
 * @brief 轮询读取 UART0 命令并按行执行。
 */
static void wk_zw101_process_debug_uart(void)
{
    uint8_t read_buf[WK_ZW101_DEBUG_READ_CHUNK_MAX] = {0};
    int read_len;
    int idx;

    read_len = uapi_uart_read(WK_ZW101_DEBUG_UART_BUS, (const uint8_t *)read_buf, sizeof(read_buf), 1U);
    if (read_len <= 0) {
        return;
    }

    for (idx = 0; idx < read_len; idx++) {
        uint8_t ch = read_buf[idx];

        if ((ch == '\r') || (ch == '\n')) {
            if (g_wk_zw101_debug_line_len > 0U) {
                g_wk_zw101_debug_line[g_wk_zw101_debug_line_len] = '\0';
                wk_zw101_handle_line(g_wk_zw101_debug_line);
                g_wk_zw101_debug_line_len = 0U;
                (void)memset_s(g_wk_zw101_debug_line,
                    sizeof(g_wk_zw101_debug_line),
                    0,
                    sizeof(g_wk_zw101_debug_line));
            }
            continue;
        }

        if ((!isprint((int)ch)) && (ch != ' ') && (ch != '\t')) {
            g_wk_zw101_debug_line_len = 0U;
            (void)memset_s(g_wk_zw101_debug_line,
                sizeof(g_wk_zw101_debug_line),
                0,
                sizeof(g_wk_zw101_debug_line));
            osal_printk("[wk2114][zw101] cmd dropped: invalid char\r\n");
            continue;
        }

        if (g_wk_zw101_debug_line_len >= (WK_ZW101_DEBUG_LINE_MAX - 1U)) {
            g_wk_zw101_debug_line_len = 0U;
            (void)memset_s(g_wk_zw101_debug_line,
                sizeof(g_wk_zw101_debug_line),
                0,
                sizeof(g_wk_zw101_debug_line));
            osal_printk("[wk2114][zw101] cmd dropped: line too long\r\n");
            continue;
        }

        g_wk_zw101_debug_line[g_wk_zw101_debug_line_len++] = (char)ch;
    }
}

errcode_t wk_zw101_test_init(void)
{
    zw101_hal_t hal = {0};
    errcode_t ret;

    ret = wk_zw101_debug_uart_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114][zw101] debug uart init fail ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    hal.uart_send = wk_zw101_uart_send_adapter;
    hal.get_tick_ms = wk_zw101_get_tick_ms_adapter;
    hal.delay_ms = wk_zw101_delay_ms_adapter;

    zw101_init(&g_wk_zw101_ctx, &hal);
    zw101_set_callbacks(&g_wk_zw101_ctx, NULL, NULL);
    zw101_reset_protocol_parse(&g_wk_zw101_ctx);

    /* 上电稳定后先探测一次，失败则由后台重探测兜底。 */
    osal_msleep(ZW101_PWRON_WAIT_PERIOD);
    if (wk_zw101_probe_ready() != ERRCODE_SUCC) {
        osal_printk("[wk2114][zw101] initial probe fail, enter reprobe mode\r\n");
    }

    g_wk_zw101_next_probe_ms = (uint32_t)uapi_systick_get_ms() + WK_ZW101_REPROBE_INTERVAL_MS;
    wk_zw101_print_help();
    return ERRCODE_SUCC;
}

void wk_zw101_test_process(void)
{
    uint32_t now_ms;

    wk_zw101_poll_subport_rx_once();
    wk_zw101_process_debug_uart();

    if (g_wk_zw101_ready != 0U) {
        return;
    }

    now_ms = (uint32_t)uapi_systick_get_ms();
    if ((int32_t)(now_ms - g_wk_zw101_next_probe_ms) < 0) {
        return;
    }

    if (wk_zw101_probe_ready() == ERRCODE_SUCC) {
        osal_printk("[wk2114][zw101] reprobe success\r\n");
    }
    g_wk_zw101_next_probe_ms = now_ms + WK_ZW101_REPROBE_INTERVAL_MS;
}
