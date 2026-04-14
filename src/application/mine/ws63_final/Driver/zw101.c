/**
 * @file zw101.c
 * @brief ZW101 指纹模组驱动实现（ws63_final 重构版）。
 *
 * 关键约束：
 * 1) 仅保留门锁业务必需命令，避免旧 ZA/RAW 调试路径继续扩散；
 * 2) Driver 统一负责帧组包、ACK 等待、超时与字段解析；
 * 3) VERIFY/ENROLL 等流程均通过同步 ACK 结果回传给 Task 层状态机。
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
#define ZW101_ACK_NO_FINGER 0x02U
#define ZW101_ACK_GET_ECHO_READY 0x55U
#define ZW101_ACK_TIMEOUT 0x26U

#define ZW101_CMD_DELETE 0x0CU
#define ZW101_CMD_CLEAR 0x0DU
#define ZW101_CMD_VALID_TEMPLATE_NUM 0x1DU
#define ZW101_CMD_CANCEL 0x30U
#define ZW101_CMD_AUTO_ENROLL 0x31U
#define ZW101_CMD_AUTO_IDENTIFY 0x32U
#define ZW101_CMD_HANDSHAKE 0x35U
#define ZW101_CMD_CHECK_SENSOR 0x36U
#define ZW101_CMD_GET_IMAGE_INFO 0x3DU
#define ZW101_CMD_GET_ECHO 0x53U

/* AutoIdentify 阶段码：对齐手册与 sle_uart_slave 解析口径。 */
#define ZW101_VERIFY_STAGE_LEGAL_CHECK 0x00U
#define ZW101_VERIFY_STAGE_CAPTURE 0x01U
#define ZW101_VERIFY_STAGE_SEARCH 0x05U

/* ----------------------------- 缓冲与超时 ----------------------------- */
#define ZW101_RX_TMP_BUF_SIZE 64U
#define ZW101_RX_CACHE_SIZE 320U
#define ZW101_FRAME_BUF_SIZE 256U
#define ZW101_CMD_FRAME_MAX_SIZE 96U
#define ZW101_ACK_PAYLOAD_MAX_LEN 64U

#define ZW101_DRAIN_MAX_ROUND 32U
#define ZW101_INIT_RETRY_TIMES 3U

#define ZW101_WAIT_POLL_MS 5U
#define ZW101_TIMEOUT_COMMON_MS 1000U
#define ZW101_TIMEOUT_AUTO_MS 30000U

/* 详细追踪日志：用于定位“未触摸也返回成功”这类链路疑难问题。 */
#define ZW101_TRACE_DETAIL_ENABLE 1U
#define ZW101_TRACE_PAYLOAD_PREVIEW_MAX 16U

/* ----------------------------- 驱动上下文 ----------------------------- */
static uint8_t g_zw101_sub_port = 0U;
static uint8_t g_zw101_ready = 0U;
static uint8_t g_zw101_rx_cache[ZW101_RX_CACHE_SIZE] = {0};
static uint16_t g_zw101_rx_cache_len = 0U;
static uint32_t g_zw101_cmd_seq = 0U;

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
 * @brief 打印 ACK/载荷预览日志，辅助排查异常结果来源。
 */
static void zw101_trace_payload(const char *tag,
    uint8_t cmd,
    uint32_t seq,
    const uint8_t *payload,
    uint16_t payload_len)
{
#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
    uint16_t i;
    uint16_t preview_len;

    if ((tag == NULL) || (payload == NULL) || (payload_len == 0U)) {
        return;
    }

    preview_len = payload_len;
    if (preview_len > ZW101_TRACE_PAYLOAD_PREVIEW_MAX) {
        preview_len = ZW101_TRACE_PAYLOAD_PREVIEW_MAX;
    }

    osal_printk("[zw101 trace] %s seq=%u cmd=0x%02X len=%u data=",
        tag,
        (unsigned int)seq,
        (unsigned int)cmd,
        (unsigned int)payload_len);
    for (i = 0U; i < preview_len; i++) {
        osal_printk("%02X", (unsigned int)payload[i]);
        if ((uint16_t)(i + 1U) < preview_len) {
            osal_printk(" ");
        }
    }
    if (payload_len > preview_len) {
        osal_printk(" ...");
    }
    osal_printk("\r\n");
#else
    (void)tag;
    (void)cmd;
    (void)seq;
    (void)payload;
    (void)payload_len;
#endif
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
        if (memcpy_s(g_zw101_rx_cache,
            sizeof(g_zw101_rx_cache),
            &data[len - ZW101_RX_CACHE_SIZE],
            ZW101_RX_CACHE_SIZE) == EOK) {
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
 * @return uint8_t 1=提取成功，0=当前缓存不足以组成完整帧。
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
 * @brief 从 WK2114 子口读取一次数据并入缓存。
 */
static void zw101_pump_uart_once(uint8_t trace_silent)
{
    uint8_t rx_tmp[ZW101_RX_TMP_BUF_SIZE] = {0};
    uint8_t len;

    if (g_zw101_sub_port == 0U) {
        return;
    }

    len = wk2114_subport_read(g_zw101_sub_port, rx_tmp, sizeof(rx_tmp));
    if (len > 0U) {
        zw101_rx_cache_push(rx_tmp, len);
#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
    if (trace_silent == 0U) {
        osal_printk("[zw101 trace] pump len=%u cache=%u\r\n",
        (unsigned int)len,
        (unsigned int)g_zw101_rx_cache_len);
    }
#else
    (void)trace_silent;
#endif
    }
}

/**
 * @brief 清空硬件 FIFO 与软件缓存，避免旧包干扰新命令。
 */
static void zw101_drain_uart(uint8_t trace_silent)
{
    uint8_t i;
    uint8_t read_len;
    uint32_t drained_bytes = 0U;
    uint8_t dummy[ZW101_RX_TMP_BUF_SIZE] = {0};

    if (g_zw101_sub_port == 0U) {
        return;
    }

    for (i = 0U; i < ZW101_DRAIN_MAX_ROUND; i++) {
        read_len = wk2114_subport_read(g_zw101_sub_port, dummy, sizeof(dummy));
        if (read_len == 0U) {
            break;
        }
        drained_bytes += read_len;
    }

    zw101_rx_cache_clear();

#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
    if ((trace_silent == 0U) && (drained_bytes > 0U)) {
        osal_printk("[zw101 trace] drain removed=%u bytes\r\n", (unsigned int)drained_bytes);
    }
#else
    (void)trace_silent;
#endif
}

/**
 * @brief 发送一条 ZW101 命令帧。
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
 * @brief 等待一帧 ACK。
 */
static errcode_t zw101_wait_ack(uint8_t cmd,
    uint32_t seq,
    uint32_t timeout_ms,
    uint8_t trace_silent,
    uint8_t wait_verify_terminal,
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
        zw101_pump_uart_once(trace_silent);

        while (zw101_rx_cache_pop_frame(frame, &frame_len) == 1U) {
            if (frame[6] != ZW101_PACKET_ACK) {
#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
                if (trace_silent == 0U) {
                    osal_printk("[zw101 trace] seq=%u cmd=0x%02X ignore packet=0x%02X len=%u\r\n",
                        (unsigned int)seq,
                        (unsigned int)cmd,
                        (unsigned int)frame[6],
                        (unsigned int)frame_len);
                }
#endif
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

#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
            if (trace_silent == 0U) {
                osal_printk("[zw101 trace] seq=%u cmd=0x%02X ack=0x%02X payload_len=%u frame_len=%u\r\n",
                    (unsigned int)seq,
                    (unsigned int)cmd,
                    (unsigned int)frame[9],
                    (unsigned int)payload_len,
                    (unsigned int)frame_len);
                zw101_trace_payload("ack_payload", cmd, seq, &frame[9], payload_len);
            }
#endif

            /*
             * AutoIdentify 同步等待策略：
             * 1) ACK=0x00 但阶段非 SEARCH（LEGAL_CHECK/CAPTURE）时继续等待；
             * 2) ACK!=0x00 视为终态失败，立即返回给上层。
             */
            if ((wait_verify_terminal != 0U) && (cmd == ZW101_CMD_AUTO_IDENTIFY) &&
                (out_result != NULL) && (out_result->ack_code == ZW101_ACK_OK)) {
                if (out_result->payload_len < 2U) {
#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
                    if (trace_silent == 0U) {
                        osal_printk("[zw101 trace] seq=%u cmd=0x%02X verify ack missing stage, continue\r\n",
                            (unsigned int)seq,
                            (unsigned int)cmd);
                    }
#endif
                    continue;
                }

                if (out_result->payload[1] != ZW101_VERIFY_STAGE_SEARCH) {
#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
                    if (trace_silent == 0U) {
                        osal_printk("[zw101 trace] seq=%u cmd=0x%02X verify stage=0x%02X continue\r\n",
                            (unsigned int)seq,
                            (unsigned int)cmd,
                            (unsigned int)out_result->payload[1]);
                    }
#endif
                    continue;
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

#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
    if (trace_silent == 0U) {
        osal_printk("[zw101 trace] seq=%u cmd=0x%02X wait timeout=%u cache=%u\r\n",
            (unsigned int)seq,
            (unsigned int)cmd,
            (unsigned int)timeout_ms,
            (unsigned int)g_zw101_rx_cache_len);
    }
#endif
    return ERRCODE_FAIL;
}

/**
 * @brief 发送命令并等待 ACK。
 */
static errcode_t zw101_send_cmd_wait(uint8_t cmd,
    const uint8_t *params,
    uint16_t params_len,
    uint32_t timeout_ms,
    uint8_t trace_silent,
    uint8_t wait_verify_terminal,
    zw101_ack_result_t *out_result)
{
    errcode_t ret;
    uint32_t seq;

    seq = ++g_zw101_cmd_seq;

#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
    if (trace_silent == 0U) {
        osal_printk("[zw101 trace] seq=%u send cmd=0x%02X params_len=%u timeout=%u\r\n",
            (unsigned int)seq,
            (unsigned int)cmd,
            (unsigned int)params_len,
            (unsigned int)timeout_ms);
        if ((params != NULL) && (params_len > 0U)) {
            zw101_trace_payload("cmd_params", cmd, seq, params, params_len);
        }
    }
#endif

    zw101_drain_uart(trace_silent);

    ret = zw101_send_command(cmd, params, params_len);
    if (ret != ERRCODE_SUCC) {
#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
        if (trace_silent == 0U) {
            osal_printk("[zw101 trace] seq=%u cmd=0x%02X send fail ret=0x%x\r\n",
                (unsigned int)seq,
                (unsigned int)cmd,
                (unsigned int)ret);
        }
#endif
        return ret;
    }

    ret = zw101_wait_ack(cmd, seq, timeout_ms, trace_silent, wait_verify_terminal, out_result);
    if ((ret != ERRCODE_SUCC) && (out_result != NULL) && (out_result->ack_code == ZW101_ACK_TIMEOUT)) {
        osal_printk("[zw101] cmd 0x%02X wait ack timeout\r\n", (unsigned int)cmd);
    }

#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
    if (trace_silent == 0U) {
        if (out_result != NULL) {
            osal_printk("[zw101 trace] seq=%u cmd=0x%02X done ret=0x%x ack=0x%02X payload=%u\r\n",
                (unsigned int)seq,
                (unsigned int)cmd,
                (unsigned int)ret,
                (unsigned int)out_result->ack_code,
                (unsigned int)out_result->payload_len);
        } else {
            osal_printk("[zw101 trace] seq=%u cmd=0x%02X done ret=0x%x (no out_result)\r\n",
                (unsigned int)seq,
                (unsigned int)cmd,
                (unsigned int)ret);
        }
    }
#endif
    return ret;
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
 * @brief 解析 ACK 载荷中的 16 位字段（payload[0] 为 ack）。
 */
static uint16_t zw101_unpack_payload_u16(const zw101_ack_result_t *ack, uint16_t offset)
{
    if ((ack == NULL) || ((uint16_t)(offset + 1U) >= ack->payload_len)) {
        return 0U;
    }

    return (uint16_t)(((uint16_t)ack->payload[offset] << 8) | ack->payload[offset + 1U]);
}

/**
 * @brief 填充调用方 ACK 输出参数。
 */
static void zw101_set_ack_out(uint8_t *ack_out, const zw101_ack_result_t *ack)
{
    if (ack_out == NULL) {
        return;
    }

    if (ack == NULL) {
        *ack_out = 0xFFU;
        return;
    }

    *ack_out = ack->ack_code;
}

/**
 * @brief 统一判定“发送成功且 ACK=0x00 才成功”。
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
 * @brief 检查驱动是否可发送业务命令。
 */
static errcode_t zw101_check_ready(void)
{
    if ((g_zw101_sub_port == 0U) || (g_zw101_ready == 0U)) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 标准握手（0x35），仅用于初始化兜底。
 */
static errcode_t zw101_handshake(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    ret = zw101_send_cmd_wait(ZW101_CMD_HANDSHAKE, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);
    return zw101_expect_ack_ok(ret, &ack);
}

/**
 * @brief 传感器检测（0x36），用于初始化探测。
 */
static errcode_t zw101_check_sensor(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    ret = zw101_send_cmd_wait(ZW101_CMD_CHECK_SENSOR, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);
    return zw101_expect_ack_ok(ret, &ack);
}

/* ----------------------------- 对外接口 ----------------------------- */

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

    zw101_drain_uart(0U);

    for (retry = 0U; retry < ZW101_INIT_RETRY_TIMES; retry++) {
        ack = 0xFFU;
        ret = zw101_echo(&ack);
        osal_printk("[zw101] init try%u echo ret=0x%x ack=0x%02X\r\n",
            (unsigned int)(retry + 1U),
            (unsigned int)ret,
            (unsigned int)ack);
        if ((ret == ERRCODE_SUCC) && ((ack == ZW101_ACK_GET_ECHO_READY) || (ack == ZW101_ACK_OK))) {
            ack = 0xFFU;
            ret = zw101_check_sensor(&ack);
            osal_printk("[zw101] init try%u check_sensor ret=0x%x ack=0x%02X\r\n",
                (unsigned int)(retry + 1U),
                (unsigned int)ret,
                (unsigned int)ack);
            if (ret == ERRCODE_SUCC) {
                g_zw101_ready = 1U;
                osal_printk("[zw101] init ok (echo+sensor)\r\n");
                return ERRCODE_SUCC;
            }
        }

        ack = 0xFFU;
        ret = zw101_handshake(&ack);
        osal_printk("[zw101] init try%u handshake ret=0x%x ack=0x%02X\r\n",
            (unsigned int)(retry + 1U),
            (unsigned int)ret,
            (unsigned int)ack);
        if (ret == ERRCODE_SUCC) {
            ack = 0xFFU;
            ret = zw101_check_sensor(&ack);
            osal_printk("[zw101] init try%u check_sensor ret=0x%x ack=0x%02X\r\n",
                (unsigned int)(retry + 1U),
                (unsigned int)ret,
                (unsigned int)ack);
            if (ret == ERRCODE_SUCC) {
                g_zw101_ready = 1U;
                osal_printk("[zw101] init ok (handshake+sensor)\r\n");
                return ERRCODE_SUCC;
            }
        }

        ws63_bsp_sleep_ms(60U);
    }

    osal_printk("[zw101] init failed\r\n");
    return ERRCODE_FAIL;
}

uint8_t zw101_is_ready(void)
{
    return g_zw101_ready;
}

void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    if ((sub_port != g_zw101_sub_port) || (data == NULL) || (len == 0U)) {
        return;
    }

    /*
     * 统一把轮询回调数据喂入缓存，避免 ACK 只在“主动读串口”路径可见。
     * 同步命令会在等待函数中从同一缓存抽帧，保证行为一致。
     */
    zw101_rx_cache_push(data, len);
}

errcode_t zw101_echo(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (g_zw101_sub_port == 0U) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_GET_ECHO, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);

    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ((ack.ack_code == ZW101_ACK_GET_ECHO_READY) || (ack.ack_code == ZW101_ACK_OK)) ?
        ERRCODE_SUCC : ERRCODE_FAIL;
}

errcode_t zw101_check_finger_present(uint8_t *finger_present_out, uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (finger_present_out != NULL) {
        *finger_present_out = 1U;
    }

    if (zw101_check_ready() != ERRCODE_SUCC) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    /*
     * 离手检测改用 PS_GetImageInfo(0x3D)：
     * - ACK=0x02 表示传感器无手指；
     * - ACK=0x00 表示当前检测到按压。
     * 该路径会高频轮询，保持 trace_silent=1U 降低日志噪声。
     */
    ret = zw101_send_cmd_wait(ZW101_CMD_GET_IMAGE_INFO, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 1U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    if (ack.ack_code == ZW101_ACK_OK) {
        if (finger_present_out != NULL) {
            *finger_present_out = 1U;
        }
        return ERRCODE_SUCC;
    }

    if (ack.ack_code == ZW101_ACK_NO_FINGER) {
        if (finger_present_out != NULL) {
            *finger_present_out = 0U;
        }
        return ERRCODE_SUCC;
    }

    return ERRCODE_FAIL;
}

errcode_t zw101_enroll(uint16_t page_id, uint8_t enroll_times, uint16_t param_flags, uint8_t *ack_out)
{
    uint8_t params[5] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    zw101_pack_u16(page_id, &params[0], &params[1]);
    params[2] = enroll_times;
    zw101_pack_u16(param_flags, &params[3], &params[4]);

    ret = zw101_send_cmd_wait(ZW101_CMD_AUTO_ENROLL,
        params,
        sizeof(params),
        ZW101_TIMEOUT_AUTO_MS,
        0U,
        0U,
        &ack);
    zw101_set_ack_out(ack_out, &ack);
    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_verify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out,
    uint8_t *ack_out)
{
    uint8_t params[5] = {0};
    uint16_t parsed_match_id = 0xFFFFU;
    uint16_t parsed_score = 0U;
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
        1U,
        &ack);
    zw101_set_ack_out(ack_out, &ack);

    if ((ret == ERRCODE_SUCC) && (ack.ack_code == ZW101_ACK_OK)) {
        if (ack.payload_len < 6U) {
            osal_printk("[zw101] verify terminal payload too short, len=%u\r\n",
                (unsigned int)ack.payload_len);
            return ERRCODE_FAIL;
        }

        /* payload[0]=ack, payload[1]=stage, payload[2..3]=id, payload[4..5]=score。 */
        parsed_match_id = zw101_unpack_payload_u16(&ack, 2U);
        parsed_score = zw101_unpack_payload_u16(&ack, 4U);

        if ((parsed_match_id == 0xFFFFU) || (parsed_score == 0U)) {
            osal_printk("[zw101] verify terminal invalid id=%u score=%u\r\n",
                (unsigned int)parsed_match_id,
                (unsigned int)parsed_score);
            return ERRCODE_FAIL;
        }

        if (match_id_out != NULL) {
            *match_id_out = parsed_match_id;
        }
        if (score_out != NULL) {
            *score_out = parsed_score;
        }

#if (ZW101_TRACE_DETAIL_ENABLE == 1U)
        osal_printk("[zw101 trace] verify parsed ack=0x%02X stage=0x%02X match_id=%u score=%u raw_len=%u\r\n",
            (unsigned int)ack.ack_code,
            (unsigned int)ack.payload[1],
            (unsigned int)parsed_match_id,
            (unsigned int)parsed_score,
            (unsigned int)ack.payload_len);
#endif
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_list(uint16_t *valid_num_out, uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (valid_num_out == NULL)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_VALID_TEMPLATE_NUM, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);

    if ((ret == ERRCODE_SUCC) && (ack.payload_len >= 3U)) {
        *valid_num_out = zw101_unpack_payload_u16(&ack, 1U);
    }

    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_delete(uint16_t page_id, uint16_t count, uint8_t *ack_out)
{
    uint8_t params[4] = {0};
    errcode_t ret;
    zw101_ack_result_t ack;

    if ((zw101_check_ready() != ERRCODE_SUCC) || (count == 0U)) {
        return ERRCODE_FAIL;
    }

    zw101_pack_u16(page_id, &params[0], &params[1]);
    zw101_pack_u16(count, &params[2], &params[3]);

    ret = zw101_send_cmd_wait(ZW101_CMD_DELETE, params, sizeof(params), ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);
    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_clear(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_CLEAR, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);
    return zw101_expect_ack_ok(ret, &ack);
}

errcode_t zw101_cancel(uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t ack;

    if (zw101_check_ready() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    ret = zw101_send_cmd_wait(ZW101_CMD_CANCEL, NULL, 0U, ZW101_TIMEOUT_COMMON_MS, 0U, 0U, &ack);
    zw101_set_ack_out(ack_out, &ack);
    return zw101_expect_ack_ok(ret, &ack);
}
