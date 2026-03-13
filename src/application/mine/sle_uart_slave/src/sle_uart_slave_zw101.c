/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 ZW101 业务模块实现。
 */

#include "sle_uart_slave_zw101.h"

#include <stdarg.h>

#include "securec.h"
#include "sle_uart_slave.h"
#include "sle_uart_slave_module.h"
#include "soc_osal.h"
#include "systick.h"
#include "ZW101/zw101_protocol.h"

#define osal_printk mine_slave_log

#if MINE_ZW101_ENABLE

/* 冷启动场景下的握手重试参数。 */
#define MINE_ZW101_HANDSHAKE_RETRY 3
#define MINE_ZW101_HANDSHAKE_RETRY_GAP_MS 40

/* 取图重试参数：用于等待手指放置到位。 */
#define MINE_ZW101_CAPTURE_RETRY 25
#define MINE_ZW101_CAPTURE_RETRY_GAP_MS 120
#define MINE_ZW101_ENROLL_SAMPLE_GAP_MS 300

/* 无效匹配 ID 标记值。 */
#define MINE_ZW101_MATCH_ID_INVALID 0xFFFF

/* ZW101 业务工作状态机。 */
typedef enum {
    MINE_ZW101_WORK_IDLE = 0,
    MINE_ZW101_WORK_ENROLL,
    MINE_ZW101_WORK_VERIFY,
} mine_zw101_work_t;

/* 协议上下文与设备可用状态。 */
static zw101_context_t g_mine_zw101_ctx;
static bool g_mine_zw101_ready = false;
static uart_bus_t g_mine_zw101_bus = MINE_ZW101_UART_BUS;

/* 面向 OLED 的状态文本缓存。 */
static volatile bool g_mine_zw101_status_dirty = false;
static char g_mine_zw101_status_text[MINE_ZW101_STATUS_TEXT_LEN] = "ZW101:OFF";

/* 业务流程状态与识别结果缓存。 */
static mine_zw101_work_t g_mine_zw101_work = MINE_ZW101_WORK_IDLE;
static uint16_t g_mine_zw101_pending_enroll_id = MINE_ZW101_AUTO_ENROLL_ID;
static uint16_t g_mine_zw101_match_id = MINE_ZW101_MATCH_ID_INVALID;
static uint16_t g_mine_zw101_match_score = 0;

/* 自动验证调度时间点（毫秒 tick）。 */
static uint32_t g_mine_zw101_next_verify_ms = 0;

/**
 * @brief ZW101 HAL 串口发送适配。
 *
 * @param data 待发送数据。
 * @param len  数据长度。
 * @return int 0 成功，-1 失败。
 */
static int mine_zw101_uart_send_adapter(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return -1;
    }

    if (uapi_uart_write(g_mine_zw101_bus, data, len, 0) < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief ZW101 HAL 毫秒计时适配。
 *
 * @return uint32_t 当前系统毫秒 tick。
 */
static uint32_t mine_zw101_get_tick_ms_adapter(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

/**
 * @brief ZW101 HAL 延时适配。
 *
 * @param ms 延时毫秒数。
 */
static void mine_zw101_delay_ms_adapter(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 更新 ZW101 状态文本并置脏标记。
 *
 * @param text 状态文本。
 */
static void mine_zw101_set_status(const char *text)
{
    if (text == NULL) {
        return;
    }

    if (snprintf_s(g_mine_zw101_status_text, sizeof(g_mine_zw101_status_text),
        sizeof(g_mine_zw101_status_text) - 1, "%s", text) > 0) {
        g_mine_zw101_status_dirty = true;
    }
}

/**
 * @brief 按格式化字符串更新 ZW101 状态文本。
 *
 * @param fmt printf 风格格式串。
 */
static void mine_zw101_set_status_fmt(const char *fmt, ...)
{
    char status[MINE_ZW101_STATUS_TEXT_LEN] = {0};
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    if (vsnprintf_s(status, sizeof(status), sizeof(status) - 1, fmt, args) > 0) {
        mine_zw101_set_status(status);
    }
    va_end(args);
}

/**
 * @brief ZW101 ACK 回调处理。
 *
 * 重点处理搜索成功回包，解析匹配 ID 与相似度评分，
 * 并同步更新状态文本和日志输出。
 *
 * @param evt ACK 事件。
 */
static void mine_zw101_ack_callback(const zw101_ack_evt_t *evt)
{
    if (evt == NULL) {
        return;
    }

    if ((evt->cmd == ZW101_CMD_SEARCH_TEMPLATE) && (evt->ack_code == ZW101_ACK_SUCCESS) &&
        (evt->payload != NULL) && (evt->payload_len >= 5)) {
        g_mine_zw101_match_id = (uint16_t)(((uint16_t)evt->payload[1] << 8) | evt->payload[2]);
        g_mine_zw101_match_score = (uint16_t)(((uint16_t)evt->payload[3] << 8) | evt->payload[4]);
        mine_zw101_set_status_fmt("ZW101:ID%u S%u", g_mine_zw101_match_id, g_mine_zw101_match_score);
        osal_printk("[mine zw101] match success, id:%u score:%u\r\n",
            (unsigned int)g_mine_zw101_match_id, (unsigned int)g_mine_zw101_match_score);
        return;
    }

    if (evt->ack_code == ZW101_ACK_ERR_NO_FINGER) {
        mine_zw101_set_status("ZW101:NO FINGER");
        return;
    }

    if (evt->ack_code != ZW101_ACK_SUCCESS) {
        mine_zw101_set_status_fmt("ZW101:C%02X E%02X", evt->cmd, evt->ack_code);
    }
}

/**
 * @brief 执行带重试的取图流程。
 *
 * 当返回“无手指”时按配置间隔重试，其他错误立即失败。
 *
 * @param operate_cmd 取图操作码（录入或验证场景）。
 * @return int 0 成功，-1 失败。
 */
static int mine_zw101_capture_with_retry(uint8_t operate_cmd)
{
    uint8_t retry_idx;

    for (retry_idx = 0; retry_idx < MINE_ZW101_CAPTURE_RETRY; retry_idx++) {
        if (zw101_cmd_capture_image(&g_mine_zw101_ctx, operate_cmd) == 0) {
            return 0;
        }

        if ((g_mine_zw101_ctx.ack_code == ZW101_ACK_ERR_NO_FINGER) ||
            (g_mine_zw101_ctx.ack_code == ZW101_PS_NO_FINGER)) {
            (void)osal_msleep(MINE_ZW101_CAPTURE_RETRY_GAP_MS);
            continue;
        }

        return -1;
    }

    return -1;
}

/**
 * @brief 执行指纹录入流程（两次取图 + 特征提取 + 建模 + 存储）。
 *
 * @param template_id 目标模板 ID。
 * @return true  录入成功。
 * @return false 录入失败。
 */
static bool mine_zw101_run_enroll_flow(uint16_t template_id)
{
    mine_zw101_set_status_fmt("ZW101:ENR %u", template_id);

    if (mine_zw101_capture_with_retry(ZW101_CMD_ENROLL_GETIMAGE) != 0) {
        mine_zw101_set_status("ZW101:ENR CAP1");
        return false;
    }

    if (zw101_cmd_general_extract(&g_mine_zw101_ctx, 1) != 0) {
        mine_zw101_set_status("ZW101:ENR FEAT1");
        return false;
    }

    mine_zw101_set_status("ZW101:ENR NEXT");
    (void)osal_msleep(MINE_ZW101_ENROLL_SAMPLE_GAP_MS);

    if (mine_zw101_capture_with_retry(ZW101_CMD_ENROLL_GETIMAGE) != 0) {
        mine_zw101_set_status("ZW101:ENR CAP2");
        return false;
    }

    if (zw101_cmd_general_extract(&g_mine_zw101_ctx, 2) != 0) {
        mine_zw101_set_status("ZW101:ENR FEAT2");
        return false;
    }

    if (zw101_cmd_general_template(&g_mine_zw101_ctx) != 0) {
        mine_zw101_set_status("ZW101:ENR MODEL");
        return false;
    }

    if (zw101_cmd_store_template(&g_mine_zw101_ctx, 1, template_id) != 0) {
        if (g_mine_zw101_ctx.ack_code == ZW101_PS_TMPL_NOT_EMPTY) {
            mine_zw101_set_status("ZW101:ID USED");
        } else if (g_mine_zw101_ctx.ack_code == ZW101_PS_FP_DUPLICATION) {
            mine_zw101_set_status("ZW101:DUP FP");
        } else {
            mine_zw101_set_status("ZW101:ENR SAVE");
        }
        return false;
    }

    mine_zw101_set_status_fmt("ZW101:ENR OK %u", template_id);
    osal_printk("[mine zw101] enroll success, id:%u\r\n", (unsigned int)template_id);
    return true;
}

/**
 * @brief 执行指纹验证流程（取图 + 特征提取 + 1:N 搜索）。
 *
 * @return true  验证流程成功完成（可能匹配成功）。
 * @return false 验证流程失败。
 */
static bool mine_zw101_run_verify_flow(void)
{
    g_mine_zw101_match_id = MINE_ZW101_MATCH_ID_INVALID;
    g_mine_zw101_match_score = 0;

    mine_zw101_set_status("ZW101:VERIFY");

    if (mine_zw101_capture_with_retry(ZW101_CMD_MATCH_GETIMAGE) != 0) {
        mine_zw101_set_status("ZW101:VFY CAP");
        return false;
    }

    if (zw101_cmd_general_extract(&g_mine_zw101_ctx, 1) != 0) {
        mine_zw101_set_status("ZW101:VFY FEAT");
        return false;
    }

    if (zw101_cmd_match1n(&g_mine_zw101_ctx, 1, MINE_ZW101_SEARCH_START_PAGE, MINE_ZW101_SEARCH_PAGE_NUM) != 0) {
        if ((g_mine_zw101_ctx.ack_code == ZW101_PS_NOT_MATCH) ||
            (g_mine_zw101_ctx.ack_code == ZW101_PS_NOT_SEARCHED)) {
            mine_zw101_set_status("ZW101:NO MATCH");
        } else {
            mine_zw101_set_status_fmt("ZW101:VFY E%02X", g_mine_zw101_ctx.ack_code);
        }
        return false;
    }

    if (g_mine_zw101_match_id == MINE_ZW101_MATCH_ID_INVALID) {
        mine_zw101_set_status("ZW101:MATCH OK");
        osal_printk("[mine zw101] match success\r\n");
    }

    return true;
}

/**
 * @brief 初始化 ZW101 设备并完成检测。
 *
 * 流程包括上电等待、握手重试与传感器检查，成功后置 READY。
 *
 * @param bus ZW101 所在 UART 总线。
 * @return true  初始化与检测成功。
 * @return false 初始化或检测失败。
 */
bool mine_zw101_init(uart_bus_t bus)
{
    zw101_hal_t hal = {0};
    uint8_t ack_code = 0xFF;
    uint8_t retry_idx;

    g_mine_zw101_ready = false;
    g_mine_zw101_bus = bus;
    g_mine_zw101_work = MINE_ZW101_WORK_IDLE;

    if (!mine_slave_uart_bus_enabled(bus)) {
        mine_zw101_set_status("ZW101:BUS OFF");
        return false;
    }

    hal.uart_send = mine_zw101_uart_send_adapter;
    hal.get_tick_ms = mine_zw101_get_tick_ms_adapter;
    hal.delay_ms = mine_zw101_delay_ms_adapter;

    zw101_init(&g_mine_zw101_ctx, &hal);
    zw101_set_callbacks(&g_mine_zw101_ctx, mine_zw101_ack_callback, NULL);
    zw101_reset_protocol_parse(&g_mine_zw101_ctx);

    (void)osal_msleep(ZW101_PWRON_WAIT_PERIOD);

    for (retry_idx = 0; retry_idx < MINE_ZW101_HANDSHAKE_RETRY; retry_idx++) {
        if (zw101_cmd_handshake(&g_mine_zw101_ctx, &ack_code) == 0) {
            if ((zw101_cmd_check_sensor(&g_mine_zw101_ctx) != 0) &&
                (g_mine_zw101_ctx.ack_code == 0x29)) {
                mine_zw101_set_status("ZW101:SENSOR ERR");
                return false;
            }

            g_mine_zw101_ready = true;
            mine_zw101_set_status("ZW101:READY");
            g_mine_zw101_next_verify_ms = (uint32_t)uapi_systick_get_ms() + MINE_ZW101_AUTO_VERIFY_INTERVAL_MS;
            return true;
        }

        (void)osal_msleep(MINE_ZW101_HANDSHAKE_RETRY_GAP_MS);
    }

    mine_zw101_set_status("ZW101:WAIT HS");
    return false;
}

/**
 * @brief 向 ZW101 协议解析器喂入串口数据。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 数据缓冲区。
 * @param len  数据长度。
 */
void mine_zw101_feed(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    if ((bus != g_mine_zw101_bus) || (data == NULL) || (len == 0)) {
        return;
    }

    zw101_protocol_parse(&g_mine_zw101_ctx, data, len);
}

/**
 * @brief 请求触发一次录入任务。
 *
 * @param template_id 目标模板 ID。
 * @return true  请求受理成功。
 * @return false 模块未就绪或当前非空闲。
 */
bool mine_zw101_request_enroll(uint16_t template_id)
{
    if ((!g_mine_zw101_ready) || (g_mine_zw101_work != MINE_ZW101_WORK_IDLE)) {
        return false;
    }

    g_mine_zw101_pending_enroll_id = template_id;
    g_mine_zw101_work = MINE_ZW101_WORK_ENROLL;
    mine_zw101_set_status_fmt("ZW101:ENR REQ %u", template_id);
    return true;
}

/**
 * @brief 请求触发一次验证任务。
 *
 * @return true  请求受理成功。
 * @return false 模块未就绪或当前非空闲。
 */
bool mine_zw101_request_verify(void)
{
    if ((!g_mine_zw101_ready) || (g_mine_zw101_work != MINE_ZW101_WORK_IDLE)) {
        return false;
    }

    g_mine_zw101_work = MINE_ZW101_WORK_VERIFY;
    mine_zw101_set_status("ZW101:VFY REQ");
    return true;
}

/**
 * @brief ZW101 业务周期处理函数。
 *
 * 在主循环中调用，负责自动验证调度与录入/验证任务执行。
 */
void mine_zw101_process(void)
{
    uint32_t now_ms;

    if (!g_mine_zw101_ready) {
        return;
    }

#if MINE_ZW101_AUTO_VERIFY_ENABLE
    if (g_mine_zw101_work == MINE_ZW101_WORK_IDLE) {
        now_ms = (uint32_t)uapi_systick_get_ms();
        if ((int32_t)(now_ms - g_mine_zw101_next_verify_ms) >= 0) {
            g_mine_zw101_work = MINE_ZW101_WORK_VERIFY;
        }
    }
#endif

    if (g_mine_zw101_work == MINE_ZW101_WORK_ENROLL) {
        uint16_t enroll_id = g_mine_zw101_pending_enroll_id;
        g_mine_zw101_work = MINE_ZW101_WORK_IDLE;
        (void)mine_zw101_run_enroll_flow(enroll_id);
#if MINE_ZW101_AUTO_VERIFY_ENABLE
        (void)mine_zw101_request_verify();
#endif
        return;
    }

    if (g_mine_zw101_work == MINE_ZW101_WORK_VERIFY) {
        g_mine_zw101_work = MINE_ZW101_WORK_IDLE;
        (void)mine_zw101_run_verify_flow();
#if MINE_ZW101_AUTO_VERIFY_ENABLE
        g_mine_zw101_next_verify_ms = (uint32_t)uapi_systick_get_ms() + MINE_ZW101_AUTO_VERIFY_INTERVAL_MS;
#endif
    }
}

/**
 * @brief 获取待刷新的 ZW101 状态文本。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return true  获取成功且有新状态。
 * @return false 无新状态或参数错误。
 */
bool mine_zw101_get_status(char *buf, uint16_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0) || (!g_mine_zw101_status_dirty)) {
        return false;
    }

    if (snprintf_s(buf, buf_len, buf_len - 1, "%s", g_mine_zw101_status_text) <= 0) {
        return false;
    }

    g_mine_zw101_status_dirty = false;
    return true;
}

#else

/* 功能关闭时提供空实现，避免外部调用链接失败。 */
bool mine_zw101_init(uart_bus_t bus)
{
    (void)bus;
    return false;
}

void mine_zw101_feed(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    (void)bus;
    (void)data;
    (void)len;
}

bool mine_zw101_request_enroll(uint16_t template_id)
{
    (void)template_id;
    return false;
}

bool mine_zw101_request_verify(void)
{
    return false;
}

void mine_zw101_process(void)
{
}

bool mine_zw101_get_status(char *buf, uint16_t buf_len)
{
    (void)buf;
    (void)buf_len;
    return false;
}

#endif
