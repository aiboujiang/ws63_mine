/**
 * @file ld2402.c
 * @brief LD2402 雷达模块驱动逻辑。
 *
 * 根据《HLK-LD2402用户手册 V1.08》:
 * - 波特率 115200
 * - 使能配置命令 (0x00FF), 结束配置命令 (0x00FE) 来确认通信正常。
 */
#include "ld2402.h"
#include "wk2114.h"
#include "ws63_final_bsp.h"
#include "ws63_final_config.h"
#include "osal_debug.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* LD2402 命令/数据帧公共格式参数。 */
#define LD2402_HEADER_LEN                 4U
#define LD2402_LEN_FIELD_LEN              2U
#define LD2402_TAIL_LEN                   4U
#define LD2402_FRAME_FIXED_OVERHEAD       (LD2402_HEADER_LEN + LD2402_LEN_FIELD_LEN + LD2402_TAIL_LEN)
#define LD2402_ACK_EXPECT_CMD_ENABLE      0x01FFU
#define LD2402_ACK_EXPECT_STATUS_OK       0x0000U

/* 初始化阶段的接收窗口参数：分段轮询，兼容“ACK 与数据帧交织/分段到达”的场景。 */
#define LD2402_INIT_RETRY_MAX             3U
#define LD2402_INIT_POLL_ROUND_MAX        8U
#define LD2402_INIT_POLL_GAP_MS           20U
/* 清空接收缓存时的最大轮询次数，防止 RFCNT 读回异常导致死循环。 */
#define LD2402_DRAIN_MAX_ROUND            32U

// 使能配置命令: 帧头(4)+帧内长(2)+字(2)+值(2)+帧尾(4)
static const uint8_t g_ld2402_cmd_enable[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 
    0x04, 0x00, 
    0xFF, 0x00, 
    0x01, 0x00, 
    0x04, 0x03, 0x02, 0x01
};

// 结束配置命令: 帧头(4)+帧内长(2)+字(2)+帧尾(4)
static const uint8_t g_ld2402_cmd_disable[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 
    0x02, 0x00, 
    0xFE, 0x00, 
    0x04, 0x03, 0x02, 0x01
};

/* 命令帧尾与数据帧尾。 */
static const uint8_t g_ld2402_cmd_tail[LD2402_TAIL_LEN] = {0x04, 0x03, 0x02, 0x01};
static const uint8_t g_ld2402_data_tail[LD2402_TAIL_LEN] = {0xF8, 0xF7, 0xF6, 0xF5};

/* 运行态日志控制：默认“开启但按间隔节流”，避免上电后高频刷屏。 */
static uint8_t g_ld2402_data_log_enable = WS63_LD2402_DATA_LOG_ENABLE_DEFAULT;
static uint32_t g_ld2402_data_log_gap_ms = WS63_LD2402_DATA_LOG_GAP_MS_DEFAULT;
static uint32_t g_ld2402_data_last_log_ms = 0U;
/* 最近一次解析到的 distance:xxx 输出，供门锁编排层轮询使用。 */
static int32_t g_ld2402_last_distance_mm = -1;
static uint32_t g_ld2402_last_distance_tick_ms = 0U;

/**
 * @brief 从串口文本中提取 `distance:xxx` 数值。
 *
 * LD2402 这一路在门锁场景下只接受明确的距离字段，避免把其他调试文本
 * 误识别为有效接近事件。
 */
static bool ld2402_parse_distance_text(const uint8_t *data, uint16_t len, int32_t *distance_mm_out)
{
    const char prefix[] = "distance:";
    char text_buf[128] = {0};
    const char *value_ptr;
    char value_buf[16] = {0};
    size_t copy_len;
    size_t i;
    long distance_value;
    char *end_ptr = NULL;

    if ((data == NULL) || (distance_mm_out == NULL) || (len == 0U)) {
        return false;
    }

    copy_len = (len < (uint16_t)(sizeof(text_buf) - 1U)) ? (size_t)len : (sizeof(text_buf) - 1U);
    (void)memcpy(text_buf, data, copy_len);
    text_buf[copy_len] = '\0';

    value_ptr = strstr(text_buf, prefix);
    if (value_ptr == NULL) {
        return false;
    }

    value_ptr += (sizeof(prefix) - 1U);
    for (i = 0U; (i + 1U < sizeof(value_buf)) && (value_ptr[i] != '\0'); i++) {
        char ch = value_ptr[i];

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '\t')) {
            break;
        }
        if ((i == 0U) && ((ch == '+') || (ch == '-'))) {
            value_buf[i] = ch;
            continue;
        }
        if ((ch < '0') || (ch > '9')) {
            break;
        }
        value_buf[i] = ch;
    }

    value_buf[i] = '\0';

    if (value_buf[0] == '\0') {
        return false;
    }

    distance_value = strtol(value_buf, &end_ptr, 10);
    if ((end_ptr == value_buf) || (end_ptr == NULL) || (*end_ptr != '\0')) {
        return false;
    }

    *distance_mm_out = (int32_t)distance_value;
    return true;
}

/**
 * @brief 检查缓冲区中的某个完整 LD2402 帧。
 *
 * @param data 缓冲区。
 * @param len 缓冲区有效长度。
 * @param has_enable_ack_out 输出：是否命中“使能配置 ACK 成功”。
 * @return true  发现至少一个完整帧（命令帧或数据帧）。
 * @return false 未发现完整帧。
 */
static bool ld2402_find_valid_frame(const uint8_t *data, uint16_t len,
    bool *has_enable_ack_out)
{
    uint16_t pos;

    if ((data == NULL) || (len < LD2402_FRAME_FIXED_OVERHEAD) || (has_enable_ack_out == NULL)) {
        return false;
    }

    *has_enable_ack_out = false;

    for (pos = 0U; pos + LD2402_FRAME_FIXED_OVERHEAD <= len; pos++) {
        bool is_cmd_header;
        bool is_data_header;
        uint16_t payload_len;
        uint16_t frame_total_len;
        uint16_t tail_pos;
        const uint8_t *expect_tail;

        is_cmd_header = (data[pos] == 0xFDU) && (data[pos + 1U] == 0xFCU) &&
            (data[pos + 2U] == 0xFBU) && (data[pos + 3U] == 0xFAU);
        is_data_header = (data[pos] == 0xF4U) && (data[pos + 1U] == 0xF3U) &&
            (data[pos + 2U] == 0xF2U) && (data[pos + 3U] == 0xF1U);
        if ((!is_cmd_header) && (!is_data_header)) {
            continue;
        }

        payload_len = (uint16_t)((uint16_t)data[pos + 4U] |
            ((uint16_t)data[pos + 5U] << 8));
        frame_total_len = (uint16_t)(LD2402_FRAME_FIXED_OVERHEAD + payload_len);
        if ((frame_total_len < LD2402_FRAME_FIXED_OVERHEAD) ||
            ((uint32_t)pos + frame_total_len > (uint32_t)len)) {
            continue;
        }

        tail_pos = (uint16_t)(pos + frame_total_len - LD2402_TAIL_LEN);
        expect_tail = is_cmd_header ? g_ld2402_cmd_tail : g_ld2402_data_tail;
        if ((data[tail_pos] != expect_tail[0]) ||
            (data[tail_pos + 1U] != expect_tail[1]) ||
            (data[tail_pos + 2U] != expect_tail[2]) ||
            (data[tail_pos + 3U] != expect_tail[3])) {
            continue;
        }

        /* 命中“使能配置 ACK + 成功状态”则直接标记。 */
        if (is_cmd_header && (payload_len >= 4U)) {
            uint16_t ack_cmd;
            uint16_t ack_status;

            ack_cmd = (uint16_t)((uint16_t)data[pos + 6U] |
                ((uint16_t)data[pos + 7U] << 8));
            ack_status = (uint16_t)((uint16_t)data[pos + 8U] |
                ((uint16_t)data[pos + 9U] << 8));
            if ((ack_cmd == LD2402_ACK_EXPECT_CMD_ENABLE) &&
                (ack_status == LD2402_ACK_EXPECT_STATUS_OK)) {
                *has_enable_ack_out = true;
            }
        }

        return true;
    }

    return false;
}

/**
 * @brief 初始化 ld2402 模块并校验通信。
 * 通过发送"使能配置命令"并读取响应来测试握手。
 */
errcode_t ld2402_init(uint8_t sub_port)
{
    uint8_t rx_buf[128] = {0};
    uint16_t rx_total = 0U;
    uint8_t len = 0;
    uint8_t retry = LD2402_INIT_RETRY_MAX;
    errcode_t ret = ERRCODE_FAIL;

    osal_printk("[ld2402] init on port %u\r\n", sub_port);

    while (retry-- > 0) {
        uint8_t round;
        uint8_t drain_round;
        uint8_t retry_index;
        bool has_valid_frame = false;
        bool has_enable_ack = false;

        retry_index = (uint8_t)(LD2402_INIT_RETRY_MAX - retry);

        // 清空接收缓存（带上限，避免读回异常时卡死）
        for (drain_round = 0U; drain_round < LD2402_DRAIN_MAX_ROUND; drain_round++) {
            if (wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf)) == 0U) {
                break;
            }
            ws63_bsp_sleep_ms(2);
        }

        if (drain_round >= LD2402_DRAIN_MAX_ROUND) {
            osal_printk("[ld2402] drain rx hit limit, continue init.\r\n");
        }

        // 发送使能配置
        wk2114_subport_write(sub_port, g_ld2402_cmd_enable, sizeof(g_ld2402_cmd_enable));

        /*
         * 分段轮询接收窗口：
         * 1) ACK 可能不是首字节对齐；
         * 2) ACK 与数据上报可能交织到达。
         */
        rx_total = 0U;
        for (round = 0U; round < LD2402_INIT_POLL_ROUND_MAX; round++) {
            len = wk2114_subport_read(sub_port, &rx_buf[rx_total],
                (uint8_t)(sizeof(rx_buf) - rx_total));
            if (len > 0U) {
                rx_total = (uint16_t)(rx_total + len);
                has_valid_frame = ld2402_find_valid_frame(rx_buf, rx_total, &has_enable_ack);
                if (has_valid_frame) {
                    break;
                }
                if (rx_total >= sizeof(rx_buf)) {
                    break;
                }
            }

            ws63_bsp_sleep_ms(LD2402_INIT_POLL_GAP_MS);
        }

        osal_printk("[ld2402] init try%u rx_total=%u valid_frame=%u enable_ack=%u\r\n",
            (unsigned int)retry_index,
            (unsigned int)rx_total,
            (unsigned int)has_valid_frame,
            (unsigned int)has_enable_ack);

        if (has_valid_frame) {
            if (has_enable_ack) {
                osal_printk("[ld2402] Enable Config ACK received, communication OK.\r\n");
            } else {
                osal_printk("[ld2402] valid frame received, communication OK.\r\n");
            }

            // 通信正常后发送结束配置，恢复工作模式
            wk2114_subport_write(sub_port, g_ld2402_cmd_disable, sizeof(g_ld2402_cmd_disable));
            ws63_bsp_sleep_ms(20);

            ret = ERRCODE_SUCC;
            break;
        }

        osal_printk("[ld2402] init retry...\r\n");
        ws63_bsp_sleep_ms(100);
    }

    if (ret != ERRCODE_SUCC) {
        osal_printk("[ld2402] init test failed.\r\n");
    }

    return ret;
}

/**
 * @brief 处理 ld2402 雷达上报数据
 */
void ld2402_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint32_t now_ms;
    int32_t distance_mm;

    (void)sub_port;

    if (len == 0U) {
        return;
    }

    /* 门锁编排层只关心明确的距离值，其他调试文本仍保留原始日志路径。 */
    if (ld2402_parse_distance_text(data, len, &distance_mm)) {
        g_ld2402_last_distance_mm = distance_mm;
        g_ld2402_last_distance_tick_ms = ws63_bsp_get_tick_ms();
        osal_printk("LD2402 processing distance:%ld\r\n", (long)distance_mm);
        return;
    }

    if (g_ld2402_data_log_enable == 0U) {
        return;
    }

    now_ms = ws63_bsp_get_tick_ms();
    /*
     * 仅限制日志输出频率，不改变日志文案语义，便于兼容现场既有关键字检索。
     * 当间隔配置为 0 时，保持每包都输出。
     */
    if ((g_ld2402_data_log_gap_ms > 0U) &&
        ((uint32_t)(now_ms - g_ld2402_data_last_log_ms) < g_ld2402_data_log_gap_ms)) {
        return;
    }

    g_ld2402_data_last_log_ms = now_ms;
    osal_printk("LD2402 processing %u bytes.\r\n", (unsigned int)len);
    /* logic to parse ld2402 radar packet */
}

/**
 * @brief 设置 LD2402 运行态日志开关。
 */
errcode_t ld2402_set_data_log_enable(uint8_t enable)
{
    g_ld2402_data_log_enable = (enable != 0U) ? 1U : 0U;
    return ERRCODE_SUCC;
}

/**
 * @brief 获取 LD2402 运行态日志开关状态。
 */
uint8_t ld2402_get_data_log_enable(void)
{
    return g_ld2402_data_log_enable;
}

/**
 * @brief 设置 LD2402 运行态日志最小输出间隔。
 */
errcode_t ld2402_set_data_log_gap_ms(uint32_t gap_ms)
{
    g_ld2402_data_log_gap_ms = gap_ms;
    return ERRCODE_SUCC;
}

/**
 * @brief 获取 LD2402 运行态日志最小输出间隔。
 */
uint32_t ld2402_get_data_log_gap_ms(void)
{
    return g_ld2402_data_log_gap_ms;
}

/**
 * @brief 获取最近一次解析到的距离值。
 */
int32_t ld2402_get_last_distance_mm(void)
{
    return g_ld2402_last_distance_mm;
}

/**
 * @brief 获取最近一次有效距离值的更新时间。
 */
uint32_t ld2402_get_last_distance_tick_ms(void)
{
    return g_ld2402_last_distance_tick_ms;
}
