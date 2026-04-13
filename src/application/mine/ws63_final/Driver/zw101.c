/**
 * @file zw101.c
 * @brief ZW101 指纹模组驱动层实现。
 *
 * 依据文档：src/application/mine/lib/指纹模组产品用户手册_V1.5.1.pdf
 * - 第 4.1 节：基本通信流程（包头/包标识/包长度/校验和）；
 * - 第 5 节：ZA 协议兼容命令（0x53/0x54/0x55/0x56/0x57/0x58/0xAA）。
 */

#include "zw101.h"

#include <stdbool.h>
#include <string.h>

#include "osal_debug.h"

#include "securec.h"

#include "wk2114.h"
#include "ws63_final_bsp.h"

/* ----------------------------- 协议常量 ----------------------------- */
#define ZW101_FRAME_HEAD_HI 0xEFU
#define ZW101_FRAME_HEAD_LO 0x01U

#define ZW101_PACKET_CMD 0x01U
#define ZW101_PACKET_ACK 0x07U

#define ZW101_ACK_OK 0x00U
#define ZW101_ACK_TIMEOUT 0x26U
#define ZW101_ACK_AUTOLOGIN_OK1 0x56U
#define ZW101_ACK_AUTOLOGIN_OK2 0x57U
#define ZW101_ACK_PROCESS_TERMINATED 0x58U
#define ZW101_ACK_GET_ECHO_READY 0x55U

#define ZW101_CMD_GET_IMAGE 0x01U
#define ZW101_CMD_GEN_CHAR 0x02U
#define ZW101_CMD_MATCH 0x03U
#define ZW101_CMD_SEARCH 0x04U
#define ZW101_CMD_REG_MODEL 0x05U
#define ZW101_CMD_STORE_CHAR 0x06U
#define ZW101_CMD_LOAD_CHAR 0x07U
#define ZW101_CMD_UP_CHAR 0x08U
#define ZW101_CMD_DOWN_CHAR 0x09U
#define ZW101_CMD_UP_IMAGE_4BIT 0x0AU
#define ZW101_CMD_DOWN_IMAGE_4BIT 0x0BU
#define ZW101_CMD_DELETE_CHAR 0x0CU
#define ZW101_CMD_EMPTY 0x0DU
#define ZW101_CMD_WRITE_REG 0x0EU
#define ZW101_CMD_READ_SYS_PARA 0x0FU
#define ZW101_CMD_SET_PWD 0x12U
#define ZW101_CMD_VFY_PWD 0x13U
#define ZW101_CMD_SET_CHIP_ADDR 0x15U
#define ZW101_CMD_READ_INF_PAGE 0x16U
#define ZW101_CMD_WRITE_NOTEPAD 0x18U
#define ZW101_CMD_READ_NOTEPAD 0x19U
#define ZW101_CMD_READ_VALID_TEMPLATE_NUM 0x1DU
#define ZW101_CMD_READ_INDEX_TABLE 0x1FU
#define ZW101_CMD_GET_ENROLL_IMAGE 0x29U
#define ZW101_CMD_CANCEL 0x30U
#define ZW101_CMD_AUTO_ENROLL 0x31U
#define ZW101_CMD_AUTO_IDENTIFY 0x32U
#define ZW101_CMD_SLEEP 0x33U
#define ZW101_CMD_GET_CHIP_SN 0x34U
#define ZW101_CMD_HANDSHAKE 0x35U
#define ZW101_CMD_CHECK_SENSOR 0x36U
#define ZW101_CMD_REST_SETTING 0x3BU
#define ZW101_CMD_CONTROL_BLN 0x3CU
#define ZW101_CMD_GET_IMAGE_INFO 0x3DU
#define ZW101_CMD_SEARCH_NOW 0x3EU
#define ZW101_CMD_BLN_AM_SW 0x60U
#define ZW101_CMD_READ_ADD_PARA 0x62U
#define ZW101_CMD_UP_IMAGE_8BIT 0x6AU
#define ZW101_CMD_DOWN_IMAGE_8BIT 0x6BU

/* 手册目录中写注册比对参数信息给出 33H；该值与休眠命令重号，按手册原文保留。 */
#define ZW101_CMD_WRITE_EMPARA 0x33U

#define ZW101_CMD_ZA_GET_ECHO 0x53U
#define ZW101_CMD_ZA_AUTO_LOGIN 0x54U
#define ZW101_CMD_ZA_AUTO_SEARCH 0x55U
#define ZW101_CMD_ZA_SEARCH_RES_BACK 0x56U
#define ZW101_CMD_ZA_AUTO_LOGIN_STAB 0x57U
#define ZW101_CMD_ZA_AUTO_SEARCH_ECHO 0x58U
#define ZW101_CMD_ZA_PROCESS_TERMINATE 0xAAU

/* ----------------------------- 缓冲与超时 ----------------------------- */
#define ZW101_RX_TMP_BUF_SIZE 64U
#define ZW101_RX_CACHE_SIZE 320U
#define ZW101_FRAME_BUF_SIZE 256U
#define ZW101_CMD_FRAME_MAX_SIZE 96U
#define ZW101_ACK_PAYLOAD_MAX_LEN 64U

#define ZW101_DRAIN_MAX_ROUND 32U

#define ZW101_WAIT_POLL_MS 5U
#define ZW101_TIMEOUT_COMMON_MS 1000U
#define ZW101_TIMEOUT_CAPTURE_MS 1500U
#define ZW101_TIMEOUT_AUTO_MS 30000U

/* ----------------------------- 驱动上下文 ----------------------------- */
static uint8_t g_zw101_sub_port = 0U;
static uint8_t g_zw101_ready = 0U;
static uint8_t g_zw101_rx_cache[ZW101_RX_CACHE_SIZE] = {0};
static uint16_t g_zw101_rx_cache_len = 0U;

/* ----------------------------- 工具函数 ----------------------------- */

/**
 * @brief 计算协议校验和（从包标识到数据末尾，排除最后 2 字节校验和）。
 */
static uint16_t zw101_calc_checksum(const uint8_t *frame, uint16_t frame_len)
{
    uint16_t i;
    uint16_t sum = 0U;

    if ((frame == NULL) || (frame_len < 12U)) {
        return 0U;
    }

    for (i = 6U; i < (uint16_t)(frame_len - 2U); i++) {
        sum = (uint16_t)(sum + frame[i]);
    }

    return sum;
}

/**
 * @brief 校验完整帧格式与校验和。
 */
static uint8_t zw101_is_valid_frame(const uint8_t *frame, uint16_t frame_len)
{
    uint16_t data_len;
    uint16_t sum;

    if ((frame == NULL) || (frame_len < 12U)) {
        return 0U;
    }

    if ((frame[0] != ZW101_FRAME_HEAD_HI) || (frame[1] != ZW101_FRAME_HEAD_LO)) {
        return 0U;
    }

    data_len = (uint16_t)(((uint16_t)frame[7] << 8) | frame[8]);
    if ((uint16_t)(9U + data_len) != frame_len) {
        return 0U;
    }

    sum = zw101_calc_checksum(frame, frame_len);
    return (uint8_t)((frame[frame_len - 2U] == (uint8_t)(sum >> 8)) &&
        (frame[frame_len - 1U] == (uint8_t)sum));
}

/**
 * @brief 向接收缓存追加数据，溢出时丢弃最旧数据。
 */
static void zw101_rx_cache_push(const uint8_t *data, uint16_t len)
{
    uint16_t drop_len;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    if (len >= ZW101_RX_CACHE_SIZE) {
        if (memcpy_s(g_zw101_rx_cache, sizeof(g_zw101_rx_cache),
            &data[len - ZW101_RX_CACHE_SIZE], ZW101_RX_CACHE_SIZE) == EOK) {
            g_zw101_rx_cache_len = ZW101_RX_CACHE_SIZE;
        }
        return;
    }

    if ((uint32_t)g_zw101_rx_cache_len + len > ZW101_RX_CACHE_SIZE) {
        drop_len = (uint16_t)((uint32_t)g_zw101_rx_cache_len + len - ZW101_RX_CACHE_SIZE);
        if (drop_len >= g_zw101_rx_cache_len) {
            g_zw101_rx_cache_len = 0U;
        } else {
            if (memmove_s(g_zw101_rx_cache,
                sizeof(g_zw101_rx_cache),
                &g_zw101_rx_cache[drop_len],
                g_zw101_rx_cache_len - drop_len) == EOK) {
                g_zw101_rx_cache_len = (uint16_t)(g_zw101_rx_cache_len - drop_len);
            }
        }
    }

    if (memcpy_s(&g_zw101_rx_cache[g_zw101_rx_cache_len],
        sizeof(g_zw101_rx_cache) - g_zw101_rx_cache_len,
        data,
        len) == EOK) {
        g_zw101_rx_cache_len = (uint16_t)(g_zw101_rx_cache_len + len);
    }
}

/**
 * @brief 清空接收缓存。
 */
static void zw101_rx_cache_clear(void)
{
    g_zw101_rx_cache_len = 0U;
}

/**
 * @brief 从缓存中提取一帧完整协议包。
 *
 * @return uint8_t 1=提取成功，0=当前缓存还不够组成完整帧。
 */
static uint8_t zw101_rx_cache_pop_frame(uint8_t *frame, uint16_t *frame_len)
{
    uint16_t data_len;
    uint16_t total_len;

    if ((frame == NULL) || (frame_len == NULL)) {
        return 0U;
    }

    while (1) {
        if (g_zw101_rx_cache_len < 2U) {
            return 0U;
        }

        if ((g_zw101_rx_cache[0] != ZW101_FRAME_HEAD_HI) ||
            (g_zw101_rx_cache[1] != ZW101_FRAME_HEAD_LO)) {
            (void)memmove_s(g_zw101_rx_cache,
                sizeof(g_zw101_rx_cache),
                &g_zw101_rx_cache[1],
                g_zw101_rx_cache_len - 1U);
            g_zw101_rx_cache_len = (uint16_t)(g_zw101_rx_cache_len - 1U);
            continue;
        }

        if (g_zw101_rx_cache_len < 9U) {
            return 0U;
        }

        data_len = (uint16_t)(((uint16_t)g_zw101_rx_cache[7] << 8) | g_zw101_rx_cache[8]);
        total_len = (uint16_t)(9U + data_len);

        if ((total_len < 12U) || (total_len > ZW101_FRAME_BUF_SIZE)) {
            (void)memmove_s(g_zw101_rx_cache,
                sizeof(g_zw101_rx_cache),
                &g_zw101_rx_cache[1],
                g_zw101_rx_cache_len - 1U);
            g_zw101_rx_cache_len = (uint16_t)(g_zw101_rx_cache_len - 1U);
            continue;
        }

        if (g_zw101_rx_cache_len < total_len) {
            return 0U;
        }

        if (!zw101_is_valid_frame(g_zw101_rx_cache, total_len)) {
            (void)memmove_s(g_zw101_rx_cache,
                sizeof(g_zw101_rx_cache),
                &g_zw101_rx_cache[1],
                g_zw101_rx_cache_len - 1U);
            g_zw101_rx_cache_len = (uint16_t)(g_zw101_rx_cache_len - 1U);
            continue;
        }

        if (memcpy_s(frame, ZW101_FRAME_BUF_SIZE, g_zw101_rx_cache, total_len) != EOK) {
            return 0U;
        }

        if (g_zw101_rx_cache_len > total_len) {
            (void)memmove_s(g_zw101_rx_cache,
                sizeof(g_zw101_rx_cache),
                &g_zw101_rx_cache[total_len],
                g_zw101_rx_cache_len - total_len);
        }
        g_zw101_rx_cache_len = (uint16_t)(g_zw101_rx_cache_len - total_len);
        *frame_len = total_len;
        return 1U;
    }
}

/**
 * @brief 非阻塞读取一次子串口并写入缓存。
 */
static void zw101_pump_uart_once(void)
{
    uint8_t rx_tmp[ZW101_RX_TMP_BUF_SIZE] = {0};
    uint8_t len;

    if (g_zw101_sub_port == 0U) {
        return;
    }

    len = wk2114_subport_read(g_zw101_sub_port, rx_tmp, sizeof(rx_tmp));
    if (len > 0U) {
        zw101_rx_cache_push(rx_tmp, len);
    }
}

/**
 * @brief 清空子串口接收残留数据。
 */
static void zw101_drain_uart(void)
{
    uint8_t i;
    uint8_t dummy[ZW101_RX_TMP_BUF_SIZE] = {0};

    for (i = 0U; i < ZW101_DRAIN_MAX_ROUND; i++) {
        if (wk2114_subport_read(g_zw101_sub_port, dummy, sizeof(dummy)) == 0U) {
            break;
        }
        ws63_bsp_sleep_ms(2U);
    }

    zw101_rx_cache_clear();
}

/**
 * @brief 组包发送命令帧。
 */
static errcode_t zw101_send_command(uint8_t cmd, const uint8_t *params, uint16_t params_len)
{
    uint8_t frame[ZW101_CMD_FRAME_MAX_SIZE] = {0};
    uint16_t frame_len;
    uint16_t data_len;
    uint16_t sum;

    if (g_zw101_sub_port == 0U) {
        return ERRCODE_FAIL;
    }

    data_len = (uint16_t)(params_len + 3U);
    frame_len = (uint16_t)(9U + data_len);
    if (frame_len > sizeof(frame)) {
        return ERRCODE_INVALID_PARAM;
    }

    frame[0] = ZW101_FRAME_HEAD_HI;
    frame[1] = ZW101_FRAME_HEAD_LO;
    frame[2] = 0xFFU;
    frame[3] = 0xFFU;
    frame[4] = 0xFFU;
    frame[5] = 0xFFU;
    frame[6] = ZW101_PACKET_CMD;
    frame[7] = (uint8_t)(data_len >> 8);
    frame[8] = (uint8_t)data_len;
    frame[9] = cmd;

    if ((params_len > 0U) && (params != NULL)) {
        if (memcpy_s(&frame[10], sizeof(frame) - 10U, params, params_len) != EOK) {
            return ERRCODE_FAIL;
        }
    }

    sum = zw101_calc_checksum(frame, frame_len);
    frame[frame_len - 2U] = (uint8_t)(sum >> 8);
    frame[frame_len - 1U] = (uint8_t)sum;

    return wk2114_subport_write(g_zw101_sub_port, frame, (uint8_t)frame_len);
}

/**
 * @brief 是否命中自动登记中间态确认码（需要继续等待）。
 */
static uint8_t zw101_is_autologin_progress_ack(uint8_t ack_code)
{
    return (uint8_t)((ack_code == ZW101_ACK_AUTOLOGIN_OK1) ||
        (ack_code == ZW101_ACK_AUTOLOGIN_OK2));
}

/**
 * @brief 等待 ACK 帧。
 *
 * @param timeout_ms 超时时间。
 * @param filter_autologin_progress 1=忽略 0x56/0x57 中间态并继续等待。
 * @param out_result 输出应答。
 * @return errcode_t ERRCODE_SUCC 收到 ACK，ERRCODE_FAIL 超时或异常。
 */
static errcode_t zw101_wait_ack(uint32_t timeout_ms,
    uint8_t filter_autologin_progress,
    zw101_ack_result_t *out_result)
{
    uint32_t start_ms;
    uint8_t frame[ZW101_FRAME_BUF_SIZE] = {0};
    uint16_t frame_len;
    uint16_t data_len;
    uint16_t payload_len;

    if (out_result != NULL) {
        out_result->ack_code = 0xFFU;
        out_result->payload_len = 0U;
    }

    start_ms = ws63_bsp_get_tick_ms();
    while ((uint32_t)(ws63_bsp_get_tick_ms() - start_ms) < timeout_ms) {
        zw101_pump_uart_once();

        while (zw101_rx_cache_pop_frame(frame, &frame_len) == 1U) {
            if (frame[6] != ZW101_PACKET_ACK) {
                continue;
            }

            data_len = (uint16_t)(((uint16_t)frame[7] << 8) | frame[8]);
            if (data_len < 3U) {
                continue;
            }

            payload_len = (uint16_t)(data_len - 2U);
            if (payload_len == 0U) {
                continue;
            }

            if ((filter_autologin_progress == 1U) && zw101_is_autologin_progress_ack(frame[9])) {
                continue;
            }

            if (out_result != NULL) {
                out_result->ack_code = frame[9];
                if (payload_len > ZW101_ACK_PAYLOAD_MAX_LEN) {
                    payload_len = ZW101_ACK_PAYLOAD_MAX_LEN;
                }
                if (memcpy_s(out_result->payload,
                    sizeof(out_result->payload),
                    &frame[9],
                    payload_len) == EOK) {
                    out_result->payload_len = payload_len;
                }
            }
            return ERRCODE_SUCC;
        }

        ws63_bsp_sleep_ms(ZW101_WAIT_POLL_MS);
    }

    if (out_result != NULL) {
        out_result->ack_code = ZW101_ACK_TIMEOUT;
        out_result->payload_len = 0U;
    }
    return ERRCODE_FAIL;
}

/**
 * @brief 发送命令并等待 ACK。
 */
static errcode_t zw101_send_cmd_wait(uint8_t cmd,
    const uint8_t *params,
    uint16_t params_len,
    uint32_t timeout_ms,
    uint8_t filter_autologin_progress,
    zw101_ack_result_t *out_result)
{
    errcode_t ret;

    zw101_drain_uart();

    ret = zw101_send_command(cmd, params, params_len);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = zw101_wait_ack(timeout_ms, filter_autologin_progress, out_result);
    if ((ret != ERRCODE_SUCC) && (out_result != NULL) && (out_result->ack_code == ZW101_ACK_TIMEOUT)) {
        osal_printk("[zw101] cmd 0x%02X wait ack timeout\r\n", (unsigned int)cmd);
    }
    return ret;
}

/**
 * @brief 统一处理“命令发送成功且 ACK=0x00 才判成功”。
 */
static errcode_t zw101_expect_ack_ok(errcode_t ret, const zw101_ack_result_t *ack)
{
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if ((ack == NULL) || (ack->ack_code != ZW101_ACK_OK)) {
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief 组装 16 位数值为高字节在前。
 */
static void zw101_pack_u16(uint16_t value, uint8_t *out_hi, uint8_t *out_lo)
{
    if ((out_hi == NULL) || (out_lo == NULL)) {
        return;
    }

    *out_hi = (uint8_t)(value >> 8);
    *out_lo = (uint8_t)value;
}

/**
 * @brief 解析 ACK 载荷中的 16 位字段（从 offset 开始，载荷下标基于 payload[0]=ack）。
 */
static uint16_t zw101_unpack_payload_u16(const zw101_ack_result_t *ack, uint16_t offset)
{
    if ((ack == NULL) || ((uint16_t)(offset + 1U) >= ack->payload_len)) {
        return 0U;
    }

    return (uint16_t)(((uint16_t)ack->payload[offset] << 8) | ack->payload[offset + 1U]);
}

/**
 * @brief 检查驱动是否可发送命令。
 */
static errcode_t zw101_check_ready(void)
{
    if ((g_zw101_sub_port == 0U) || (g_zw101_ready == 0U)) {
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

/* ----------------------------- 对外基础接口 ----------------------------- */

errcode_t zw101_init(uint8_t sub_port)
{
    errcode_t ret;
    uint8_t retry;
    uint8_t ack;

    if (sub_port == 0U) {
        return ERRCODE_INVALID_PARAM;
    }

    g_zw101_sub_port = sub_port;
    g_zw101_ready = 0U;

    osal_printk("[zw101] init on port %u\r\n", (unsigned int)sub_port);

    zw101_drain_uart();

    for (retry = 0U; retry < 3U; retry++) {
        ack = 0xFFU;
        ret = zw101_za_get_echo(&ack);
        osal_printk("[zw101] init try%u echo ret=0x%x ack=0x%02X\r\n",
            (unsigned int)(retry + 1U),
            (unsigned int)ret,
            (unsigned int)ack);
        if (ret == ERRCODE_SUCC) {
            if ((ack == ZW101_ACK_GET_ECHO_READY) || (ack == ZW101_ACK_OK)) {
                ack = 0xFFU;
                ret = zw101_maint_check_sensor(&ack);
                osal_printk("[zw101] init try%u check_sensor ret=0x%x ack=0x%02X\r\n",
                    (unsigned int)(retry + 1U),
                    (unsigned int)ret,
                    (unsigned int)ack);
                if (ret == ERRCODE_SUCC) {
                    if (ack == ZW101_ACK_OK) {
                        g_zw101_ready = 1U;
                        osal_printk("[zw101] init ok (echo=0x%02X sensor=0x%02X)\r\n",
                            (unsigned int)ZW101_ACK_GET_ECHO_READY,
                            (unsigned int)ack);
                        return ERRCODE_SUCC;
                    }
                }
            }
        }

        /* 兼容路径：若 ZA 握手不通，退回标准握手 0x35 再做传感器检查。 */
        ack = 0xFFU;
        ret = zw101_maint_handshake(&ack);
        osal_printk("[zw101] init try%u handshake ret=0x%x ack=0x%02X\r\n",
            (unsigned int)(retry + 1U),
            (unsigned int)ret,
            (unsigned int)ack);
        if (ret == ERRCODE_SUCC) {
            if (ack == ZW101_ACK_OK) {
                ack = 0xFFU;
                ret = zw101_maint_check_sensor(&ack);
                osal_printk("[zw101] init try%u check_sensor ret=0x%x ack=0x%02X\r\n",
                    (unsigned int)(retry + 1U),
                    (unsigned int)ret,
                    (unsigned int)ack);
                if (ret == ERRCODE_SUCC) {
                    if (ack == ZW101_ACK_OK) {
                        g_zw101_ready = 1U;
                        osal_printk("[zw101] init ok (handshake+sensor)\r\n");
                        return ERRCODE_SUCC;
                    }
                }
            }
        }

        ws63_bsp_sleep_ms(60U);
    }

    osal_printk("[zw101] init failed\r\n");
    return ERRCODE_FAIL;
}

/**
 * @brief 查询 ZW101 驱动是否已进入可用状态。
 */
uint8_t zw101_is_ready(void)
{
    return g_zw101_ready;
}

void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint8_t frame[ZW101_FRAME_BUF_SIZE] = {0};
    uint16_t frame_len;

    if ((sub_port != g_zw101_sub_port) || (data == NULL) || (len == 0U)) {
        return;
    }

    /*
     * 主循环轮询读到的包在这里做流式缓存。
     * 同步命令执行时会复用同一套帧提取逻辑，保证通信流程一致。
     */
    zw101_rx_cache_push(data, len);

    while (zw101_rx_cache_pop_frame(frame, &frame_len) == 1U) {
        if ((frame[6] == ZW101_PACKET_ACK) && (frame_len >= 12U)) {
            osal_printk("[zw101] async ack=0x%02X\r\n", (unsigned int)frame[9]);
        }
    }
}

errcode_t zw101_send_raw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U) || (g_zw101_sub_port == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    return wk2114_subport_write(g_zw101_sub_port, data, len);
}

/* ----------------------------- ZA 协议兼容命令 ----------------------------- */

errcode_t zw101_za_get_echo(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_GET_ECHO,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ack_out != NULL) {
        *ack_out = ack.ack_code;
    }

    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    if ((ack.ack_code == ZW101_ACK_GET_ECHO_READY) || (ack.ack_code == ZW101_ACK_OK)) {
        return ERRCODE_SUCC;
    }

    return ERRCODE_FAIL;
}

errcode_t zw101_za_auto_login(uint8_t wait_time,
    uint8_t sample_interval_code,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if ((press_times != 2U) && (press_times != 3U)) {
        return ERRCODE_INVALID_PARAM;
    }

    params[0] = wait_time;
    params[1] = (uint8_t)(((sample_interval_code & 0x0FU) << 4U) | (press_times & 0x0FU));
    zw101_pack_u16(page_id, &params[2], &params[3]);
    params[4] = (allow_dup == 0U) ? 0U : 1U;

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_AUTO_LOGIN,
        params,
        sizeof(params),
        ZW101_TIMEOUT_AUTO_MS,
        1U,
        &ack);
    if (ack_out != NULL) {
        *ack_out = ack.ack_code;
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_za_auto_search(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    zw101_ack_result_t *result_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    params[0] = wait_time;
    zw101_pack_u16(start_page, &params[1], &params[2]);
    zw101_pack_u16(page_num, &params[3], &params[4]);

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_AUTO_SEARCH,
        params,
        sizeof(params),
        ZW101_TIMEOUT_AUTO_MS,
        0U,
        &ack);
    if (result_out != NULL) {
        *result_out = ack;
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_za_search_res_back(uint8_t buffer_id,
    uint16_t start_page,
    uint16_t page_num,
    zw101_ack_result_t *result_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if ((buffer_id != 1U) && (buffer_id != 2U)) {
        return ERRCODE_INVALID_PARAM;
    }

    params[0] = buffer_id;
    zw101_pack_u16(start_page, &params[1], &params[2]);
    zw101_pack_u16(page_num, &params[3], &params[4]);

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_SEARCH_RES_BACK,
        params,
        sizeof(params),
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (result_out != NULL) {
        *result_out = ack;
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_za_auto_login_stab_light(uint8_t wait_time,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if ((press_times != 2U) && (press_times != 3U)) {
        return ERRCODE_INVALID_PARAM;
    }

    params[0] = wait_time;
    params[1] = press_times;
    zw101_pack_u16(page_id, &params[2], &params[3]);
    params[4] = (allow_dup == 0U) ? 0U : 1U;

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_AUTO_LOGIN_STAB,
        params,
        sizeof(params),
        ZW101_TIMEOUT_AUTO_MS,
        1U,
        &ack);
    if (ack_out != NULL) {
        *ack_out = ack.ack_code;
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_za_auto_search_with_echo(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    zw101_ack_result_t *result_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    params[0] = wait_time;
    zw101_pack_u16(start_page, &params[1], &params[2]);
    zw101_pack_u16(page_num, &params[3], &params[4]);

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_AUTO_SEARCH_ECHO,
        params,
        sizeof(params),
        ZW101_TIMEOUT_AUTO_MS,
        0U,
        &ack);
    if (result_out != NULL) {
        *result_out = ack;
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_za_process_terminate(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    ret = zw101_send_cmd_wait(ZW101_CMD_ZA_PROCESS_TERMINATE,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ack_out != NULL) {
        *ack_out = ack.ack_code;
    }

    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return (ack.ack_code == ZW101_ACK_PROCESS_TERMINATED) ? ERRCODE_SUCC : ERRCODE_FAIL;
}

/* ----------------------------- 业务类指令集 ----------------------------- */

errcode_t zw101_business_get_image(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_GET_IMAGE, NULL, 0U, ZW101_TIMEOUT_CAPTURE_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_gen_char(uint8_t buffer_id)
{
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (buffer_id == 0U)) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_GEN_CHAR, &buffer_id, 1U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_match(uint16_t *score_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_MATCH, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack);
    if ((ret == ERRCODE_SUCC) && (score_out != NULL) && (ack.payload_len >= 3U)) {
        *score_out = zw101_unpack_payload_u16(&ack, 1U);
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_business_search(uint8_t buffer_id,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (buffer_id == 0U)) {
        return ERRCODE_FAIL;
    }

    params[0] = buffer_id;
    zw101_pack_u16(start_page, &params[1], &params[2]);
    zw101_pack_u16(page_num, &params[3], &params[4]);

    ret = zw101_send_cmd_wait(ZW101_CMD_SEARCH,
        params,
        sizeof(params),
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);

    if ((ret == ERRCODE_SUCC) && (ack.payload_len >= 5U)) {
        if (page_id_out != NULL) {
            *page_id_out = zw101_unpack_payload_u16(&ack, 1U);
        }
        if (score_out != NULL) {
            *score_out = zw101_unpack_payload_u16(&ack, 3U);
        }
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_business_reg_model(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_REG_MODEL, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_store_char(uint8_t buffer_id, uint16_t page_id)
{
    uint8_t params[3] = {0};
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (buffer_id == 0U)) {
        return ERRCODE_FAIL;
    }

    params[0] = buffer_id;
    zw101_pack_u16(page_id, &params[1], &params[2]);

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_STORE_CHAR, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_load_char(uint8_t buffer_id, uint16_t page_id)
{
    uint8_t params[3] = {0};
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (buffer_id == 0U)) {
        return ERRCODE_FAIL;
    }

    params[0] = buffer_id;
    zw101_pack_u16(page_id, &params[1], &params[2]);

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_LOAD_CHAR, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_up_char(uint8_t buffer_id)
{
    zw101_ack_result_t ack;

    /* 说明：本函数当前完成“命令 + ACK”阶段，后续数据包上行由上层按场景接入。 */
    if ((zw101_check_ready() != ERRCODE_SUCC) || (buffer_id == 0U)) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_UP_CHAR, &buffer_id, 1U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_down_char(uint8_t buffer_id)
{
    zw101_ack_result_t ack;

    /* 说明：本函数当前完成“命令 + ACK”阶段，后续数据包下行由上层按场景接入。 */
    if ((zw101_check_ready() != ERRCODE_SUCC) || (buffer_id == 0U)) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_DOWN_CHAR, &buffer_id, 1U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_delete_char(uint16_t page_id, uint16_t count)
{
    uint8_t params[4] = {0};
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (count == 0U)) {
        return ERRCODE_FAIL;
    }

    zw101_pack_u16(page_id, &params[0], &params[1]);
    zw101_pack_u16(count, &params[2], &params[3]);

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_DELETE_CHAR, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_empty(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_EMPTY, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_write_reg(uint8_t reg_index, uint8_t reg_value)
{
    uint8_t params[2] = {reg_index, reg_value};
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_WRITE_REG, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_read_syspara(uint8_t *syspara_out, uint16_t *out_len)
{
    errcode_t ret;
    zw101_ack_result_t ack;
    uint16_t copy_len;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (syspara_out == NULL) || (out_len == NULL)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_READ_SYS_PARA,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (ack.ack_code != ZW101_ACK_OK) {
        return ERRCODE_FAIL;
    }

    if (ack.payload_len <= 1U) {
        *out_len = 0U;
        return ERRCODE_FAIL;
    }

    copy_len = (uint16_t)(ack.payload_len - 1U);
    if (copy_len > *out_len) {
        copy_len = *out_len;
    }

    if (memcpy_s(syspara_out, *out_len, &ack.payload[1], copy_len) != EOK) {
        return ERRCODE_FAIL;
    }

    *out_len = copy_len;
    return ERRCODE_SUCC;
}

errcode_t zw101_business_read_infpage(void)
{
    zw101_ack_result_t ack;

    /* 说明：本函数当前完成“命令 + ACK”阶段，信息页数据包上传后续按需求接入。 */
    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_READ_INF_PAGE, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_read_valid_template_num(uint16_t *valid_num_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_READ_VALID_TEMPLATE_NUM,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (ack.ack_code != ZW101_ACK_OK) {
        return ERRCODE_FAIL;
    }

    if ((valid_num_out != NULL) && (ack.payload_len >= 3U)) {
        *valid_num_out = zw101_unpack_payload_u16(&ack, 1U);
    }

    return ERRCODE_SUCC;
}

errcode_t zw101_business_read_index_table(uint8_t table_index)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_READ_INDEX_TABLE,
            &table_index,
            1U,
            ZW101_TIMEOUT_COMMON_MS,
            0U,
            &ack),
        &ack);
}

errcode_t zw101_business_get_enroll_image(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_GET_ENROLL_IMAGE, NULL, 0U, ZW101_TIMEOUT_CAPTURE_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_read_add_para(uint8_t *add_para_out, uint16_t *out_len)
{
    errcode_t ret;
    zw101_ack_result_t ack;
    uint16_t copy_len;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (add_para_out == NULL) || (out_len == NULL)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_READ_ADD_PARA,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (ack.ack_code != ZW101_ACK_OK) {
        return ERRCODE_FAIL;
    }

    if (ack.payload_len <= 1U) {
        *out_len = 0U;
        return ERRCODE_FAIL;
    }

    copy_len = (uint16_t)(ack.payload_len - 1U);
    if (copy_len > *out_len) {
        copy_len = *out_len;
    }
    if (memcpy_s(add_para_out, *out_len, &ack.payload[1], copy_len) != EOK) {
        return ERRCODE_FAIL;
    }

    *out_len = copy_len;
    return ERRCODE_SUCC;
}

errcode_t zw101_business_sleep(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_SLEEP, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_write_empara(uint16_t em_para)
{
    uint8_t params[2] = {0};
    zw101_ack_result_t ack;

    /* 手册目录中该命令码与休眠同为 33H，本实现按原文保留。 */
    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    zw101_pack_u16(em_para, &params[0], &params[1]);
    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_WRITE_EMPARA, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_cancel(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_CANCEL, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_business_auto_enroll(uint16_t page_id, uint8_t enroll_times, uint16_t param_flags)
{
    uint8_t params[5] = {0};
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    zw101_pack_u16(page_id, &params[0], &params[1]);
    params[2] = enroll_times;
    zw101_pack_u16(param_flags, &params[3], &params[4]);

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_AUTO_ENROLL, params, sizeof(params), ZW101_TIMEOUT_AUTO_MS, 1U, &ack),
        &ack);
}

errcode_t zw101_business_auto_identify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    params[0] = score_level;
    zw101_pack_u16(target_id, &params[1], &params[2]);
    zw101_pack_u16(param_flags, &params[3], &params[4]);

    ret = zw101_send_cmd_wait(ZW101_CMD_AUTO_IDENTIFY,
        params,
        sizeof(params),
        ZW101_TIMEOUT_AUTO_MS,
        0U,
        &ack);

    if ((ret == ERRCODE_SUCC) && (ack.payload_len >= 5U)) {
        if (match_id_out != NULL) {
            *match_id_out = zw101_unpack_payload_u16(&ack, 1U);
        }
        if (score_out != NULL) {
            *score_out = zw101_unpack_payload_u16(&ack, 3U);
        }
    }

    return zw101_expect_ack_ok(ret, &ack);
}

/* ----------------------------- 维护类指令集 ----------------------------- */

errcode_t zw101_maint_up_image_4bit(void)
{
    zw101_ack_result_t ack;

    /* 仅实现命令阶段，图像数据包上传在后续链路接入。 */
    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_UP_IMAGE_4BIT, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_maint_up_image_8bit(void)
{
    zw101_ack_result_t ack;

    /* 仅实现命令阶段，图像数据包上传在后续链路接入。 */
    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_UP_IMAGE_8BIT, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_maint_down_image_4bit(void)
{
    zw101_ack_result_t ack;

    /* 仅实现命令阶段，图像数据包下发在后续链路接入。 */
    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_DOWN_IMAGE_4BIT, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_maint_down_image_8bit(void)
{
    zw101_ack_result_t ack;

    /* 仅实现命令阶段，图像数据包下发在后续链路接入。 */
    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_DOWN_IMAGE_8BIT, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_maint_get_chip_sn(uint8_t *chip_sn_out, uint16_t *out_len)
{
    errcode_t ret;
    zw101_ack_result_t ack;
    uint16_t copy_len;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (chip_sn_out == NULL) || (out_len == NULL)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_GET_CHIP_SN,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (ack.ack_code != ZW101_ACK_OK) {
        return ERRCODE_FAIL;
    }

    if (ack.payload_len <= 1U) {
        *out_len = 0U;
        return ERRCODE_FAIL;
    }

    copy_len = (uint16_t)(ack.payload_len - 1U);
    if (copy_len > *out_len) {
        copy_len = *out_len;
    }
    if (memcpy_s(chip_sn_out, *out_len, &ack.payload[1], copy_len) != EOK) {
        return ERRCODE_FAIL;
    }

    *out_len = copy_len;
    return ERRCODE_SUCC;
}

errcode_t zw101_maint_handshake(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    ret = zw101_send_cmd_wait(ZW101_CMD_HANDSHAKE,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);

    if (ack_out != NULL) {
        *ack_out = ack.ack_code;
    }

    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return (ack.ack_code == ZW101_ACK_OK) ? ERRCODE_SUCC : ERRCODE_FAIL;
}

errcode_t zw101_maint_check_sensor(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    ret = zw101_send_cmd_wait(ZW101_CMD_CHECK_SENSOR,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);

    if (ack_out != NULL) {
        *ack_out = ack.ack_code;
    }

    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return (ack.ack_code == ZW101_ACK_OK) ? ERRCODE_SUCC : ERRCODE_FAIL;
}

errcode_t zw101_maint_reset_setting(void)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_REST_SETTING, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

/* ----------------------------- 定制类指令集 ----------------------------- */

errcode_t zw101_custom_set_pwd(uint32_t pwd)
{
    uint8_t params[4] = {0};
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    params[0] = (uint8_t)(pwd >> 24);
    params[1] = (uint8_t)(pwd >> 16);
    params[2] = (uint8_t)(pwd >> 8);
    params[3] = (uint8_t)pwd;

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_SET_PWD, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_custom_verify_pwd(uint32_t pwd)
{
    uint8_t params[4] = {0};
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    params[0] = (uint8_t)(pwd >> 24);
    params[1] = (uint8_t)(pwd >> 16);
    params[2] = (uint8_t)(pwd >> 8);
    params[3] = (uint8_t)pwd;

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_VFY_PWD, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_custom_set_chip_addr(uint32_t chip_addr)
{
    uint8_t params[4] = {0};
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    params[0] = (uint8_t)(chip_addr >> 24);
    params[1] = (uint8_t)(chip_addr >> 16);
    params[2] = (uint8_t)(chip_addr >> 8);
    params[3] = (uint8_t)chip_addr;

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_SET_CHIP_ADDR, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_custom_write_notepad(uint8_t page_id, const uint8_t *data, uint8_t data_len)
{
    uint8_t params[33] = {0};
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (data == NULL) || (data_len == 0U) || (data_len > 32U)) {
        return ERRCODE_FAIL;
    }

    params[0] = page_id;
    if (memcpy_s(&params[1], sizeof(params) - 1U, data, data_len) != EOK) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_WRITE_NOTEPAD, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_custom_read_notepad(uint8_t page_id, uint8_t *data_out, uint16_t *out_len)
{
    errcode_t ret;
    zw101_ack_result_t ack;
    uint16_t copy_len;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (data_out == NULL) || (out_len == NULL)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_READ_NOTEPAD,
        &page_id,
        1U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (ack.ack_code != ZW101_ACK_OK) {
        return ERRCODE_FAIL;
    }

    if (ack.payload_len <= 1U) {
        *out_len = 0U;
        return ERRCODE_FAIL;
    }

    copy_len = (uint16_t)(ack.payload_len - 1U);
    if (copy_len > *out_len) {
        copy_len = *out_len;
    }

    if (memcpy_s(data_out, *out_len, &ack.payload[1], copy_len) != EOK) {
        return ERRCODE_FAIL;
    }

    *out_len = copy_len;
    return ERRCODE_SUCC;
}

errcode_t zw101_custom_bln_auto_manual_switch(uint8_t mode)
{
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_BLN_AM_SW, &mode, 1U, ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_custom_control_bln(uint8_t func_code,
    uint8_t start_color,
    uint8_t end_color_or_duty,
    uint8_t loop_times,
    uint8_t cycle)
{
    uint8_t params[5] = {0};
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    params[0] = func_code;
    params[1] = start_color;
    params[2] = end_color_or_duty;
    params[3] = loop_times;
    params[4] = cycle;

    return zw101_expect_ack_ok(
        zw101_send_cmd_wait(ZW101_CMD_CONTROL_BLN, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, &ack),
        &ack);
}

errcode_t zw101_custom_get_image_info(uint8_t *area_out, uint8_t *quality_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_GET_IMAGE_INFO,
        NULL,
        0U,
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (ack.ack_code != ZW101_ACK_OK) {
        return ERRCODE_FAIL;
    }

    if (ack.payload_len >= 3U) {
        if (area_out != NULL) {
            *area_out = ack.payload[1];
        }
        if (quality_out != NULL) {
            *quality_out = ack.payload[2];
        }
        return ERRCODE_SUCC;
    }

    return ERRCODE_FAIL;
}

errcode_t zw101_custom_search_now(uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out)
{
    uint8_t params[4] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    zw101_pack_u16(start_page, &params[0], &params[1]);
    zw101_pack_u16(page_num, &params[2], &params[3]);

    ret = zw101_send_cmd_wait(ZW101_CMD_SEARCH_NOW,
        params,
        sizeof(params),
        ZW101_TIMEOUT_COMMON_MS,
        0U,
        &ack);

    if ((ret == ERRCODE_SUCC) && (ack.payload_len >= 5U)) {
        if (page_id_out != NULL) {
            *page_id_out = zw101_unpack_payload_u16(&ack, 1U);
        }
        if (score_out != NULL) {
            *score_out = zw101_unpack_payload_u16(&ack, 3U);
        }
    }

    return zw101_expect_ack_ok(ret, &ack);
}
