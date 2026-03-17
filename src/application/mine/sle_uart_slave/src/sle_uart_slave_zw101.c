/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 ZW101 指纹业务模块。
 *
 * 设计目标：
 * 1) 以《指纹模组产品用户手册_V1.5.1》为唯一协议依据；
 * 2) 主流程采用模块指令：
 *    - 自动注册模板 PS_AutoEnroll (0x31)
 *    - 自动验证指纹 PS_AutoIdentify (0x32)
 *    - 删除模板 PS_DeletChar (0x0C)
 * 3) 对外接口保持与旧版兼容，便于无侵入替换；
 * 4) 全流程日志包含命令输入、关键阶段、确认码释义，便于现场排障。
 */

#include "sle_uart_slave_zw101.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "securec.h"
#include "sle_uart_slave.h"
#include "sle_uart_slave_module.h"
#include "soc_osal.h"
#include "systick.h"
#include "ZW101/zw101_protocol.h"

#define osal_printk mine_slave_log

#if MINE_ZW101_ENABLE

/* ----------------------------- 协议与时序常量 ----------------------------- */

/* 上电后握手重试次数与重试间隔。 */
#define MINE_ZW101_HANDSHAKE_RETRY 3
#define MINE_ZW101_HANDSHAKE_RETRY_GAP_MS 40

/* 设备未就绪时后台重探测间隔。 */
#define MINE_ZW101_REPROBE_INTERVAL_MS 1500

/* 自动流程执行超时：覆盖按手、采图、特征、检索整段流程。 */
#define MINE_ZW101_AUTO_ENROLL_TIMEOUT_MS 30000
#define MINE_ZW101_AUTO_VERIFY_TIMEOUT_MS 18000

/* 命令轮询等待周期，避免忙等占满 CPU。 */
#define MINE_ZW101_WAIT_POLL_MS 10

/* 调试命令缓存长度上限。 */
#define MINE_ZW101_DEBUG_LINE_MAX 96

/*
 * 自动注册默认参数：
 * bit4=1 表示不允许重复注册（手册 3.3.2.2 参数位定义）。
 * 其它位默认 0：
 * - bit2=0: 返回关键步骤应答，便于日志定位。
 * - bit3=0: 不覆盖既有 ID。
 */
#define MINE_ZW101_AUTO_ENROLL_PARAM_DEFAULT ((uint16_t)(1U << 4))

/* 自动注册默认按压次数。 */
#define MINE_ZW101_AUTO_ENROLL_TIMES_DEFAULT 3

/*
 * 自动验证默认参数：
 * bit2=0 表示返回关键步骤应答（合法性/采图/检索）。
 */
#define MINE_ZW101_AUTO_VERIFY_PARAM_DEFAULT ((uint16_t)0x0000U)

/* 自动验证默认分数等级与默认目标 ID。ID=0xFFFF 表示 1:N 搜索。 */
#define MINE_ZW101_AUTO_VERIFY_SCORE_LEVEL_DEFAULT 3
#define MINE_ZW101_AUTO_VERIFY_ID_DEFAULT 0xFFFF

/* 无效匹配 ID 标记。 */
#define MINE_ZW101_MATCH_ID_INVALID 0xFFFF

/* ----------------------------- 内部状态定义 ----------------------------- */

/**
 * @brief 调试命令操作类型。
 */
typedef enum {
    MINE_ZW101_CMD_NONE = 0,
    MINE_ZW101_CMD_HELP,
    MINE_ZW101_CMD_STATUS,
    MINE_ZW101_CMD_ENROLL,
    MINE_ZW101_CMD_VERIFY,
    MINE_ZW101_CMD_DELETE,
    MINE_ZW101_CMD_CLEAR,
    MINE_ZW101_CMD_CANCEL,
} mine_zw101_cmd_op_t;

/**
 * @brief 待执行命令结构体。
 */
typedef struct {
    mine_zw101_cmd_op_t op;
    uint16_t id;
    uint16_t count;
    uint8_t enroll_times;
    uint8_t score_level;
} mine_zw101_cmd_t;

/**
 * @brief 当前自动流程类型。
 */
typedef enum {
    MINE_ZW101_AUTO_FLOW_NONE = 0,
    MINE_ZW101_AUTO_FLOW_ENROLL,
    MINE_ZW101_AUTO_FLOW_VERIFY,
} mine_zw101_auto_flow_t;

/**
 * @brief 自动流程运行态快照。
 */
typedef struct {
    bool active;
    bool done;
    bool success;
    mine_zw101_auto_flow_t flow;
    uint8_t ack_code;
    uint8_t param1;
    uint8_t param2;
    uint16_t match_id;
    uint16_t match_score;
} mine_zw101_auto_state_t;

/* 协议上下文与设备状态。 */
static zw101_context_t g_mine_zw101_ctx;
static bool g_mine_zw101_ready = false;
static uart_bus_t g_mine_zw101_bus = MINE_ZW101_UART_BUS;
static uint32_t g_mine_zw101_next_probe_ms = 0;

/* 状态文本（OLED/日志共用）。 */
static volatile bool g_mine_zw101_status_dirty = false;
static char g_mine_zw101_status_text[MINE_ZW101_STATUS_TEXT_LEN] = "ZW101:OFF";

/* 自动验证结果缓存。 */
static uint16_t g_mine_zw101_last_match_id = MINE_ZW101_MATCH_ID_INVALID;
static uint16_t g_mine_zw101_last_match_score = 0;
static uint32_t g_mine_zw101_next_verify_ms = 0;

/* 命令执行器：单槽位队列（中断回调生产，任务线程消费）。 */
static bool g_mine_zw101_cmd_pending = false;
static bool g_mine_zw101_cmd_running = false;
static mine_zw101_cmd_t g_mine_zw101_cmd = { MINE_ZW101_CMD_NONE, 0, 0, 0, 0 };

/* 自动流程 ACK 状态（由 ACK 回调更新，由任务线程等待）。 */
static mine_zw101_auto_state_t g_mine_zw101_auto_state = {
    false, false, false, MINE_ZW101_AUTO_FLOW_NONE, 0xFF, 0, 0, MINE_ZW101_MATCH_ID_INVALID, 0,
};

/* 串口调试命令行缓存。 */
static bool g_mine_zw101_dbg_capture = false;
static uint16_t g_mine_zw101_dbg_line_len = 0;
static char g_mine_zw101_dbg_line[MINE_ZW101_DEBUG_LINE_MAX] = {0};

/* ----------------------------- 基础工具函数 ----------------------------- */

/**
 * @brief 设置状态文本并标记为脏。
 *
 * @param text 状态文本。
 */
static void mine_zw101_set_status(const char *text)
{
    if (text == NULL) {
        return;
    }

    /* 文本未变化时不重复置脏，减少 OLED 无效刷新。 */
    if (strncmp(g_mine_zw101_status_text, text, sizeof(g_mine_zw101_status_text) - 1U) == 0) {
        return;
    }

    if (snprintf_s(g_mine_zw101_status_text, sizeof(g_mine_zw101_status_text),
        sizeof(g_mine_zw101_status_text) - 1, "%s", text) > 0) {
        g_mine_zw101_status_dirty = true;
    }
}

/**
 * @brief 使用格式化字符串更新状态文本。
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
 * @brief 统一确认码释义。
 *
 * 释义依据手册 3.2 指令应答章节。
 *
 * @param ack_code 确认码。
 * @return const char* 可读释义。
 */
static const char *mine_zw101_ack_desc(uint8_t ack_code)
{
    switch (ack_code) {
        case 0x00:
            return "OK";
        case 0x01:
            return "PKG_RECV_ERR";
        case 0x02:
            return "NO_FINGER";
        case 0x03:
            return "GET_IMAGE_FAIL";
        case 0x04:
            return "IMAGE_TOO_DRY";
        case 0x05:
            return "IMAGE_TOO_WET";
        case 0x06:
            return "IMAGE_TOO_MESSY";
        case 0x07:
            return "FEATURE_TOO_FEW";
        case 0x08:
            return "NOT_MATCH";
        case 0x09:
            return "NOT_FOUND";
        case 0x0A:
            return "MERGE_FAIL";
        case 0x0B:
            return "PAGE_ID_OVERFLOW";
        case 0x0C:
            return "TEMPLATE_INVALID";
        case 0x10:
            return "DELETE_FAIL";
        case 0x11:
            return "EMPTY_FAIL";
        case 0x15:
            return "NO_RAW_IMAGE";
        case 0x17:
            return "RESIDUAL_FINGER";
        case 0x1E:
            return "AUTO_ENROLL_FAIL";
        case 0x1F:
            return "LIB_FULL";
        case 0x22:
            return "TEMPLATE_NOT_EMPTY";
        case 0x23:
            return "TEMPLATE_EMPTY";
        case 0x24:
            return "LIB_EMPTY";
        case 0x25:
            return "ENROLL_TIMES_ERR";
        case 0x26:
            return "TIMEOUT";
        case 0x27:
            return "FINGER_EXISTS";
        case 0x28:
            return "TEMPLATE_ASSOCIATED";
        case 0x29:
            return "SENSOR_INIT_FAIL";
        case 0x33:
            return "IMAGE_AREA_SMALL";
        case 0x34:
            return "IMAGE_UNAVAILABLE";
        case 0x40:
            return "ENROLL_TIMES_NOT_ENOUGH";
        case 0x41:
            return "COMM_TIMEOUT";
        case 0x58:
            return "PROCESS_TERMINATED";
        case 0xFF:
            return "WAIT_ACK_TIMEOUT";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 自动注册阶段参数释义。
 *
 * @param p1 参数1。
 * @param p2 参数2。
 * @return const char* 阶段说明。
 */
static const char *mine_zw101_enroll_stage_desc(uint8_t p1, uint8_t p2)
{
    switch (p1) {
        case 0x00:
            return "LEGAL_CHECK";
        case 0x01:
            return "CAPTURE";
        case 0x02:
            return "GEN_CHAR";
        case 0x03:
            return "FINGER_LEAVE";
        case 0x04:
            return (p2 == 0xF0U) ? "REG_MODEL" : "MERGE";
        case 0x05:
            return (p2 == 0xF1U) ? "DUP_CHECK" : "CHECK";
        case 0x06:
            return (p2 == 0xF2U) ? "STORE" : "SAVE";
        default:
            return "STEP_UNKNOWN";
    }
}

/**
 * @brief 自动验证阶段参数释义。
 *
 * @param p1 参数1。
 * @return const char* 阶段说明。
 */
static const char *mine_zw101_verify_stage_desc(uint8_t p1)
{
    switch (p1) {
        case 0x00:
            return "LEGAL_CHECK";
        case 0x01:
            return "CAPTURE";
        case 0x05:
            return "SEARCH";
        default:
            return "STEP_UNKNOWN";
    }
}

/**
 * @brief 判断自动注册流程是否到达终态。
 *
 * @param ack 确认码。
 * @param p1 参数1。
 * @param p2 参数2。
 * @return true  已终态。
 * @return false 非终态。
 */
static bool mine_zw101_is_auto_enroll_terminal(uint8_t ack, uint8_t p1, uint8_t p2)
{
    if ((p1 == 0x06U) && (p2 == 0xF2U)) {
        return true;
    }

    if ((p1 == 0x00U) && (ack != ZW101_PS_OK)) {
        return true;
    }

    if ((ack == ZW101_PS_FP_DUPLICATION) && (p1 == 0x05U) && (p2 == 0xF1U)) {
        return true;
    }

    if ((ack == ZW101_PS_TIME_OUT) || (ack == ZW101_PS_ENROLL_ERR) ||
        (ack == ZW101_PS_ENROLL_CANCEL) || (ack == ZW101_PS_LIB_FULL_ERR) ||
        (ack == ZW101_PS_TMPL_NOT_EMPTY) || (ack == ZW101_PS_ADDRESS_OVER)) {
        return true;
    }

    if ((ack != ZW101_PS_OK) && ((p1 == 0x04U) || (p1 == 0x05U) || (p1 == 0x06U))) {
        return true;
    }

    return false;
}

/**
 * @brief 判断自动验证流程是否到达终态。
 *
 * @param ack 确认码。
 * @param p1 参数1。
 * @return true  已终态。
 * @return false 非终态。
 */
static bool mine_zw101_is_auto_verify_terminal(uint8_t ack, uint8_t p1)
{
    if (p1 == 0x05U) {
        return true;
    }

    if ((p1 == 0x00U) && (ack != ZW101_PS_OK)) {
        return true;
    }

    if ((ack == ZW101_PS_TIME_OUT) || (ack == ZW101_PS_NOT_SEARCHED) ||
        (ack == ZW101_PS_NOT_MATCH) || (ack == ZW101_PS_TMPL_EMPTY) ||
        (ack == 0x24U) || (ack == ZW101_PS_ADDRESS_OVER)) {
        return true;
    }

    return false;
}

/**
 * @brief 进入临界区，保护中断与任务并发共享状态。
 *
 * @return unsigned int 中断状态快照。
 */
static unsigned int mine_zw101_irq_lock(void)
{
    return osal_irq_lock();
}

/**
 * @brief 离开临界区。
 *
 * @param irq_status 上一次加锁返回的中断状态。
 */
static void mine_zw101_irq_unlock(unsigned int irq_status)
{
    osal_irq_restore(irq_status);
}

/* ----------------------------- 命令槽位管理 ----------------------------- */

/**
 * @brief 将命令写入待执行槽位。
 *
 * @param cmd 待执行命令。
 * @return true  入队成功。
 * @return false 槽位忙或参数错误。
 */
static bool mine_zw101_push_cmd(const mine_zw101_cmd_t *cmd)
{
    unsigned int irq_status;

    if (cmd == NULL) {
        return false;
    }

    irq_status = mine_zw101_irq_lock();
    if (g_mine_zw101_cmd_running || g_mine_zw101_cmd_pending) {
        mine_zw101_irq_unlock(irq_status);
        return false;
    }

    g_mine_zw101_cmd = *cmd;
    g_mine_zw101_cmd_pending = true;
    mine_zw101_irq_unlock(irq_status);
    return true;
}

/**
 * @brief 取出待执行命令并置为运行态。
 *
 * @param cmd 输出命令对象。
 * @return true  取出成功。
 * @return false 当前无待执行命令。
 */
static bool mine_zw101_pop_cmd(mine_zw101_cmd_t *cmd)
{
    unsigned int irq_status;

    if (cmd == NULL) {
        return false;
    }

    irq_status = mine_zw101_irq_lock();
    if (!g_mine_zw101_cmd_pending) {
        mine_zw101_irq_unlock(irq_status);
        return false;
    }

    *cmd = g_mine_zw101_cmd;
    g_mine_zw101_cmd_pending = false;
    g_mine_zw101_cmd_running = true;
    mine_zw101_irq_unlock(irq_status);
    return true;
}

/**
 * @brief 标记当前命令执行完成。
 */
static void mine_zw101_finish_cmd(void)
{
    unsigned int irq_status = mine_zw101_irq_lock();
    g_mine_zw101_cmd_running = false;
    mine_zw101_irq_unlock(irq_status);
}

/**
 * @brief 判断当前是否存在待执行或正在执行命令。
 *
 * @return true  忙。
 * @return false 空闲。
 */
static bool mine_zw101_cmd_busy(void)
{
    bool busy;
    unsigned int irq_status = mine_zw101_irq_lock();
    busy = g_mine_zw101_cmd_running || g_mine_zw101_cmd_pending;
    mine_zw101_irq_unlock(irq_status);
    return busy;
}

/* ----------------------------- 自动流程状态管理 ----------------------------- */

/**
 * @brief 启动自动流程等待状态。
 *
 * @param flow 自动流程类型。
 */
static void mine_zw101_auto_state_start(mine_zw101_auto_flow_t flow)
{
    unsigned int irq_status = mine_zw101_irq_lock();
    g_mine_zw101_auto_state.active = true;
    g_mine_zw101_auto_state.done = false;
    g_mine_zw101_auto_state.success = false;
    g_mine_zw101_auto_state.flow = flow;
    g_mine_zw101_auto_state.ack_code = 0xFF;
    g_mine_zw101_auto_state.param1 = 0;
    g_mine_zw101_auto_state.param2 = 0;
    g_mine_zw101_auto_state.match_id = MINE_ZW101_MATCH_ID_INVALID;
    g_mine_zw101_auto_state.match_score = 0;
    mine_zw101_irq_unlock(irq_status);
}

/**
 * @brief 写入自动流程状态更新。
 *
 * @param active  是否处于活动态。
 * @param done    是否完成。
 * @param success 是否成功。
 * @param flow    流程类型。
 * @param ack     确认码。
 * @param p1      参数1。
 * @param p2      参数2。
 * @param id      匹配 ID。
 * @param score   匹配分数。
 */
static void mine_zw101_auto_state_set(bool active, bool done, bool success, mine_zw101_auto_flow_t flow,
    uint8_t ack, uint8_t p1, uint8_t p2, uint16_t id, uint16_t score)
{
    unsigned int irq_status = mine_zw101_irq_lock();
    g_mine_zw101_auto_state.active = active;
    g_mine_zw101_auto_state.done = done;
    g_mine_zw101_auto_state.success = success;
    g_mine_zw101_auto_state.flow = flow;
    g_mine_zw101_auto_state.ack_code = ack;
    g_mine_zw101_auto_state.param1 = p1;
    g_mine_zw101_auto_state.param2 = p2;
    g_mine_zw101_auto_state.match_id = id;
    g_mine_zw101_auto_state.match_score = score;
    mine_zw101_irq_unlock(irq_status);
}

/**
 * @brief 读取自动流程状态快照。
 *
 * @return mine_zw101_auto_state_t 当前快照。
 */
static mine_zw101_auto_state_t mine_zw101_auto_state_get(void)
{
    mine_zw101_auto_state_t snap;
    unsigned int irq_status = mine_zw101_irq_lock();
    snap = g_mine_zw101_auto_state;
    mine_zw101_irq_unlock(irq_status);
    return snap;
}

/* ----------------------------- HAL 适配 ----------------------------- */

/**
 * @brief ZW101 HAL 串口发送适配。
 *
 * @param data 发送数据指针。
 * @param len  发送长度。
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
 * @param ms 延时毫秒。
 */
static void mine_zw101_delay_ms_adapter(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/* ----------------------------- ACK 回调 ----------------------------- */

/**
 * @brief 处理自动注册 ACK。
 *
 * @param evt ACK 事件。
 */
static void mine_zw101_handle_auto_enroll_ack(const zw101_ack_evt_t *evt)
{
    uint8_t ack;
    uint8_t p1;
    uint8_t p2;
    bool done;
    bool success;

    if ((evt == NULL) || (evt->payload == NULL) || (evt->payload_len < 3U)) {
        return;
    }

    ack = evt->payload[0];
    p1 = evt->payload[1];
    p2 = evt->payload[2];

    osal_printk("[mine zw101] ENROLL step:%s p1:0x%02X p2:0x%02X ack:0x%02X(%s)\r\n",
        mine_zw101_enroll_stage_desc(p1, p2), p1, p2, ack, mine_zw101_ack_desc(ack));

    done = mine_zw101_is_auto_enroll_terminal(ack, p1, p2);
    success = (done && (ack == ZW101_PS_OK) && (p1 == 0x06U) && (p2 == 0xF2U));

    if (success) {
        mine_zw101_set_status("ZW101:ENR OK");
    } else if (done) {
        mine_zw101_set_status_fmt("ZW101:ENR E%02X", ack);
    }

    mine_zw101_auto_state_set(!done, done, success, MINE_ZW101_AUTO_FLOW_ENROLL,
        ack, p1, p2, MINE_ZW101_MATCH_ID_INVALID, 0);
}

/**
 * @brief 处理自动验证 ACK。
 *
 * @param evt ACK 事件。
 */
static void mine_zw101_handle_auto_verify_ack(const zw101_ack_evt_t *evt)
{
    uint8_t ack;
    uint8_t p1;
    uint16_t id = MINE_ZW101_MATCH_ID_INVALID;
    uint16_t score = 0;
    bool done;
    bool success;

    if ((evt == NULL) || (evt->payload == NULL) || (evt->payload_len < 2U)) {
        return;
    }

    ack = evt->payload[0];
    p1 = evt->payload[1];

    if (evt->payload_len >= 6U) {
        id = (uint16_t)(((uint16_t)evt->payload[2] << 8) | evt->payload[3]);
        score = (uint16_t)(((uint16_t)evt->payload[4] << 8) | evt->payload[5]);
    }

    osal_printk("[mine zw101] VERIFY step:%s p1:0x%02X ack:0x%02X(%s) id:%u score:%u\r\n",
        mine_zw101_verify_stage_desc(p1), p1, ack, mine_zw101_ack_desc(ack),
        (unsigned int)id, (unsigned int)score);

    done = mine_zw101_is_auto_verify_terminal(ack, p1);
    success = done && (ack == ZW101_PS_OK) && (p1 == 0x05U);

    if (success) {
        g_mine_zw101_last_match_id = id;
        g_mine_zw101_last_match_score = score;
        mine_zw101_set_status_fmt("ZW101:ID%u S%u", id, score);
    } else if (done) {
        if (ack == ZW101_PS_NOT_SEARCHED) {
            mine_zw101_set_status("ZW101:NO MATCH");
        } else {
            mine_zw101_set_status_fmt("ZW101:VFY E%02X", ack);
        }
    }

    mine_zw101_auto_state_set(!done, done, success, MINE_ZW101_AUTO_FLOW_VERIFY,
        ack, p1, 0, id, score);
}

/**
 * @brief 协议 ACK 统一回调。
 *
 * @param evt ACK 事件。
 */
static void mine_zw101_ack_callback(const zw101_ack_evt_t *evt)
{
    mine_zw101_auto_state_t snap;

    if (evt == NULL) {
        return;
    }

    snap = mine_zw101_auto_state_get();

    if (snap.active && (evt->cmd == ZW101_CMD_AUTO_ENROLL)) {
        mine_zw101_handle_auto_enroll_ack(evt);
        return;
    }

    if (snap.active && (evt->cmd == ZW101_CMD_AUTO_MATCH)) {
        mine_zw101_handle_auto_verify_ack(evt);
        return;
    }

    if (evt->ack_code == ZW101_PS_NO_FINGER) {
        mine_zw101_set_status("ZW101:NO FINGER");
        return;
    }

    if (evt->ack_code != ZW101_PS_OK) {
        mine_zw101_set_status_fmt("ZW101:C%02X E%02X", evt->cmd, evt->ack_code);
    }
}

/* ----------------------------- 核心流程 ----------------------------- */

/**
 * @brief 设备探测（握手 + 传感器检查）。
 *
 * @return true  设备已就绪。
 * @return false 设备未就绪。
 */
static bool mine_zw101_probe_ready(void)
{
    uint8_t ack = 0xFF;
    uint8_t retry;

    for (retry = 0; retry < MINE_ZW101_HANDSHAKE_RETRY; retry++) {
        if (zw101_cmd_handshake(&g_mine_zw101_ctx, &ack) == 0) {
            if (zw101_cmd_check_sensor(&g_mine_zw101_ctx) != 0) {
                if (g_mine_zw101_ctx.ack_code == 0x29U) {
                    mine_zw101_set_status("ZW101:SENSOR ERR");
                    g_mine_zw101_ready = false;
                    return false;
                }
            }

            g_mine_zw101_ready = true;
            mine_zw101_set_status("ZW101:READY");
            g_mine_zw101_next_verify_ms = (uint32_t)uapi_systick_get_ms() + MINE_ZW101_AUTO_VERIFY_INTERVAL_MS;
            return true;
        }

        (void)osal_msleep(MINE_ZW101_HANDSHAKE_RETRY_GAP_MS);
    }

    g_mine_zw101_ready = false;
    mine_zw101_set_status("ZW101:WAIT HS");
    return false;
}

/**
 * @brief 发送取消流程命令（PS_Cancel, 0x30）。
 *
 * @return int 0 成功，-1 失败。
 */
static int mine_zw101_send_cancel(void)
{
    if (zw101_send_command(&g_mine_zw101_ctx, ZW101_CMD_AUTO_CANCEL, NULL, 0) != 0) {
        return -1;
    }

    return zw101_wait_ack(&g_mine_zw101_ctx, ZW101_CMD_AUTO_CANCEL, ZW101_COMMON_TIMEOUT, NULL);
}

/**
 * @brief 等待自动流程结束。
 *
 * @param timeout_ms 超时毫秒。
 * @param out_state  输出状态快照，可为 NULL。
 * @return true  自动流程成功。
 * @return false 自动流程失败或超时。
 */
static bool mine_zw101_wait_auto_done(uint32_t timeout_ms, mine_zw101_auto_state_t *out_state)
{
    uint32_t start_ms = (uint32_t)uapi_systick_get_ms();

    while (1) {
        mine_zw101_auto_state_t snap = mine_zw101_auto_state_get();

        if (snap.done) {
            if (out_state != NULL) {
                *out_state = snap;
            }
            return snap.success;
        }

        if ((uint32_t)(uapi_systick_get_ms() - start_ms) >= timeout_ms) {
            mine_zw101_set_status("ZW101:AUTO TO");
            if (out_state != NULL) {
                *out_state = snap;
            }
            return false;
        }

        (void)osal_msleep(MINE_ZW101_WAIT_POLL_MS);
    }
}

/**
 * @brief 执行自动注册模板流程。
 *
 * @param template_id 目标模板 ID。
 * @param enroll_times 按压次数（建议 2~4）。
 * @return true  注册成功。
 * @return false 注册失败。
 */
static bool mine_zw101_run_auto_enroll(uint16_t template_id, uint8_t enroll_times)
{
    uint8_t params[5];
    mine_zw101_auto_state_t result;
    bool ok;

    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if ((enroll_times < 2U) || (enroll_times > 6U)) {
        enroll_times = MINE_ZW101_AUTO_ENROLL_TIMES_DEFAULT;
    }

    params[0] = (uint8_t)(template_id >> 8);
    params[1] = (uint8_t)template_id;
    params[2] = enroll_times;
    params[3] = (uint8_t)(MINE_ZW101_AUTO_ENROLL_PARAM_DEFAULT >> 8);
    params[4] = (uint8_t)MINE_ZW101_AUTO_ENROLL_PARAM_DEFAULT;

    mine_zw101_set_status_fmt("ZW101:ENR %u", template_id);
    osal_printk("[mine zw101] cmd in: PS_AutoEnroll id:%u times:%u param:0x%04X\r\n",
        (unsigned int)template_id, (unsigned int)enroll_times,
        (unsigned int)MINE_ZW101_AUTO_ENROLL_PARAM_DEFAULT);

    mine_zw101_auto_state_start(MINE_ZW101_AUTO_FLOW_ENROLL);
    if (zw101_send_command(&g_mine_zw101_ctx, ZW101_CMD_AUTO_ENROLL, params, sizeof(params)) != 0) {
        mine_zw101_set_status("ZW101:ENR SEND");
        osal_printk("[mine zw101] ENROLL send failed\r\n");
        return false;
    }

    ok = mine_zw101_wait_auto_done(MINE_ZW101_AUTO_ENROLL_TIMEOUT_MS, &result);
    if (!ok && !result.done) {
        osal_printk("[mine zw101] ENROLL timeout -> send cancel\r\n");
        (void)mine_zw101_send_cancel();
        mine_zw101_auto_state_set(false, true, false, MINE_ZW101_AUTO_FLOW_ENROLL,
            ZW101_PS_TIME_OUT, 0, 0, MINE_ZW101_MATCH_ID_INVALID, 0);
        result = mine_zw101_auto_state_get();
    }

    if (result.success) {
        mine_zw101_set_status_fmt("ZW101:ENR OK %u", template_id);
        osal_printk("[mine zw101] ENROLL success, id:%u\r\n", (unsigned int)template_id);
        return true;
    }

    mine_zw101_set_status_fmt("ZW101:ENR E%02X", result.ack_code);
    osal_printk("[mine zw101] ENROLL failed, ack:0x%02X(%s) p1:0x%02X p2:0x%02X\r\n",
        result.ack_code, mine_zw101_ack_desc(result.ack_code), result.param1, result.param2);
    return false;
}

/**
 * @brief 执行自动验证流程。
 *
 * @param score_level 分数等级（1~5）。
 * @param target_id 目标 ID，0xFFFF 表示 1:N。
 * @return true  验证成功。
 * @return false 验证失败。
 */
static bool mine_zw101_run_auto_verify(uint8_t score_level, uint16_t target_id)
{
    uint8_t params[5];
    mine_zw101_auto_state_t result;
    bool ok;

    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if ((score_level < 1U) || (score_level > 5U)) {
        score_level = MINE_ZW101_AUTO_VERIFY_SCORE_LEVEL_DEFAULT;
    }

    params[0] = score_level;
    params[1] = (uint8_t)(target_id >> 8);
    params[2] = (uint8_t)target_id;
    params[3] = (uint8_t)(MINE_ZW101_AUTO_VERIFY_PARAM_DEFAULT >> 8);
    params[4] = (uint8_t)MINE_ZW101_AUTO_VERIFY_PARAM_DEFAULT;

    mine_zw101_set_status("ZW101:VERIFY");
    osal_printk("[mine zw101] cmd in: PS_AutoIdentify level:%u id:0x%04X param:0x%04X\r\n",
        (unsigned int)score_level, (unsigned int)target_id,
        (unsigned int)MINE_ZW101_AUTO_VERIFY_PARAM_DEFAULT);

    mine_zw101_auto_state_start(MINE_ZW101_AUTO_FLOW_VERIFY);
    if (zw101_send_command(&g_mine_zw101_ctx, ZW101_CMD_AUTO_MATCH, params, sizeof(params)) != 0) {
        mine_zw101_set_status("ZW101:VFY SEND");
        osal_printk("[mine zw101] VERIFY send failed\r\n");
        return false;
    }

    ok = mine_zw101_wait_auto_done(MINE_ZW101_AUTO_VERIFY_TIMEOUT_MS, &result);
    if (!ok && !result.done) {
        osal_printk("[mine zw101] VERIFY timeout -> send cancel\r\n");
        (void)mine_zw101_send_cancel();
        mine_zw101_auto_state_set(false, true, false, MINE_ZW101_AUTO_FLOW_VERIFY,
            ZW101_PS_TIME_OUT, 0, 0, MINE_ZW101_MATCH_ID_INVALID, 0);
        result = mine_zw101_auto_state_get();
    }

    if (result.success) {
        mine_zw101_set_status_fmt("ZW101:ID%u S%u", result.match_id, result.match_score);
        osal_printk("[mine zw101] VERIFY success, id:%u score:%u\r\n",
            (unsigned int)result.match_id, (unsigned int)result.match_score);
        return true;
    }

    if (result.ack_code == ZW101_PS_NOT_SEARCHED) {
        mine_zw101_set_status("ZW101:NO MATCH");
    } else {
        mine_zw101_set_status_fmt("ZW101:VFY E%02X", result.ack_code);
    }

    osal_printk("[mine zw101] VERIFY failed, ack:0x%02X(%s) p1:0x%02X\r\n",
        result.ack_code, mine_zw101_ack_desc(result.ack_code), result.param1);
    return false;
}

/**
 * @brief 执行删除模板流程。
 *
 * @param id 起始模板 ID。
 * @param count 删除数量。
 * @return true  删除成功。
 * @return false 删除失败。
 */
static bool mine_zw101_run_delete(uint16_t id, uint16_t count)
{
    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if (count == 0U) {
        mine_zw101_set_status("ZW101:DEL ARG");
        return false;
    }

    mine_zw101_set_status_fmt("ZW101:DEL %u+%u", id, count);
    osal_printk("[mine zw101] cmd in: PS_DeletChar id:%u count:%u\r\n",
        (unsigned int)id, (unsigned int)count);

    if (zw101_cmd_del_template(&g_mine_zw101_ctx, id, count) != 0) {
        mine_zw101_set_status_fmt("ZW101:DEL E%02X", g_mine_zw101_ctx.ack_code);
        osal_printk("[mine zw101] DELETE failed, ack:0x%02X(%s)\r\n",
            g_mine_zw101_ctx.ack_code, mine_zw101_ack_desc(g_mine_zw101_ctx.ack_code));
        return false;
    }

    mine_zw101_set_status_fmt("ZW101:DEL OK %u", id);
    osal_printk("[mine zw101] DELETE success, id:%u count:%u\r\n",
        (unsigned int)id, (unsigned int)count);
    return true;
}

/**
 * @brief 执行清空模板库流程（可选扩展功能）。
 *
 * @return true  清空成功。
 * @return false 清空失败。
 */
static bool mine_zw101_run_clear(void)
{
    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    mine_zw101_set_status("ZW101:CLEAR");
    osal_printk("[mine zw101] cmd in: PS_Empty\r\n");

    if (zw101_cmd_empty_template(&g_mine_zw101_ctx) != 0) {
        if (g_mine_zw101_ctx.ack_code == ZW101_PS_TMPL_EMPTY) {
            mine_zw101_set_status("ZW101:CLEAR OK");
            osal_printk("[mine zw101] CLEAR ignored: already empty\r\n");
            return true;
        }

        mine_zw101_set_status_fmt("ZW101:CLEAR E%02X", g_mine_zw101_ctx.ack_code);
        osal_printk("[mine zw101] CLEAR failed, ack:0x%02X(%s)\r\n",
            g_mine_zw101_ctx.ack_code, mine_zw101_ack_desc(g_mine_zw101_ctx.ack_code));
        return false;
    }

    mine_zw101_set_status("ZW101:CLEAR OK");
    osal_printk("[mine zw101] CLEAR success\r\n");
    return true;
}

/**
 * @brief 打印可用调试命令帮助。
 */
static void mine_zw101_print_help(void)
{
    osal_printk("[mine zw101] cmd help:\r\n");
    osal_printk("[mine zw101]   FP HELP\r\n");
    osal_printk("[mine zw101]   FP STATUS\r\n");
    osal_printk("[mine zw101]   FP ENROLL <id> [times]\r\n");
    osal_printk("[mine zw101]   FP VERIFY [score] [id]\r\n");
    osal_printk("[mine zw101]   FP DEL <id> [count]\r\n");
    osal_printk("[mine zw101]   FP CLEAR\r\n");
    osal_printk("[mine zw101]   FP CANCEL\r\n");
}

/**
 * @brief 执行单条命令。
 *
 * @param cmd 待执行命令。
 */
static void mine_zw101_exec_cmd(const mine_zw101_cmd_t *cmd)
{
    if ((cmd == NULL) || (cmd->op == MINE_ZW101_CMD_NONE)) {
        return;
    }

    switch (cmd->op) {
        case MINE_ZW101_CMD_HELP:
            mine_zw101_print_help();
            mine_zw101_set_status("ZW101:HELP");
            break;

        case MINE_ZW101_CMD_STATUS:
            osal_printk("[mine zw101] status ready:%u cmd_busy:%u text:%s\r\n",
                (unsigned int)g_mine_zw101_ready,
                (unsigned int)mine_zw101_cmd_busy(),
                g_mine_zw101_status_text);
            mine_zw101_set_status("ZW101:STATUS");
            break;

        case MINE_ZW101_CMD_ENROLL:
            osal_printk("[mine zw101] cmd out: ENROLL %s\r\n",
                mine_zw101_run_auto_enroll(cmd->id, cmd->enroll_times) ? "accept" : "reject");
            break;

        case MINE_ZW101_CMD_VERIFY:
            osal_printk("[mine zw101] cmd out: VERIFY %s\r\n",
                mine_zw101_run_auto_verify(cmd->score_level, cmd->id) ? "accept" : "reject");
            break;

        case MINE_ZW101_CMD_DELETE:
            (void)mine_zw101_run_delete(cmd->id, cmd->count);
            break;

        case MINE_ZW101_CMD_CLEAR:
            (void)mine_zw101_run_clear();
            break;

        case MINE_ZW101_CMD_CANCEL:
            if (mine_zw101_send_cancel() == 0) {
                mine_zw101_set_status("ZW101:CANCEL OK");
                osal_printk("[mine zw101] CANCEL success\r\n");
            } else {
                mine_zw101_set_status("ZW101:CANCEL FAIL");
                osal_printk("[mine zw101] CANCEL failed\r\n");
            }
            break;

        default:
            break;
    }
}

/* ----------------------------- 调试命令解析 ----------------------------- */

/**
 * @brief 将字符串转大写（原地转换）。
 *
 * @param str 待转换字符串。
 */
static void mine_zw101_to_upper(char *str)
{
    uint16_t i;

    if (str == NULL) {
        return;
    }

    for (i = 0; str[i] != '\0'; i++) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }
}

/**
 * @brief 提取下一个空白分隔 token。
 *
 * @param cursor 输入输出游标。
 * @return char* token 起始地址，失败返回 NULL。
 */
static char *mine_zw101_next_token(char **cursor)
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
 * @brief 解析 u16 参数（支持十进制/0x 前缀）。
 *
 * @param token 输入字符串。
 * @param value 输出值。
 * @return true  解析成功。
 * @return false 解析失败。
 */
static bool mine_zw101_parse_u16(const char *token, uint16_t *value)
{
    char *end = NULL;
    unsigned long tmp;

    if ((token == NULL) || (value == NULL)) {
        return false;
    }

    tmp = strtoul(token, &end, 0);
    if ((end == token) || (*end != '\0') || (tmp > 0xFFFFUL)) {
        return false;
    }

    *value = (uint16_t)tmp;
    return true;
}

/**
 * @brief 解析 u8 参数。
 *
 * @param token 输入字符串。
 * @param value 输出值。
 * @return true  解析成功。
 * @return false 解析失败。
 */
static bool mine_zw101_parse_u8(const char *token, uint8_t *value)
{
    uint16_t tmp;

    if (!mine_zw101_parse_u16(token, &tmp)) {
        return false;
    }

    if (tmp > 0xFFU) {
        return false;
    }

    *value = (uint8_t)tmp;
    return true;
}

/**
 * @brief 入队命令并在失败时设置统一状态。
 *
 * @param cmd 待入队命令。
 * @return true  入队成功。
 * @return false 入队失败。
 */
static bool mine_zw101_push_cmd_or_busy(const mine_zw101_cmd_t *cmd)
{
    if (!mine_zw101_push_cmd(cmd)) {
        mine_zw101_set_status("ZW101:CMD BUSY");
        return false;
    }

    return true;
}

/**
 * @brief 解析一行调试命令并写入执行槽位。
 *
 * 支持命令：
 * - FP HELP
 * - FP STATUS
 * - FP ENROLL <id> [times]
 * - FP VERIFY [score] [id]
 * - FP DEL <id> [count]
 * - FP CLEAR
 * - FP CANCEL
 *
 * @param line 输入行。
 */
static void mine_zw101_handle_debug_line(const char *line)
{
    char cmd_line[MINE_ZW101_DEBUG_LINE_MAX] = {0};
    char *cursor;
    char *op;
    char *arg0;
    char *arg1;
    mine_zw101_cmd_t cmd = {
        MINE_ZW101_CMD_NONE,
        MINE_ZW101_AUTO_VERIFY_ID_DEFAULT,
        1,
        MINE_ZW101_AUTO_ENROLL_TIMES_DEFAULT,
        MINE_ZW101_AUTO_VERIFY_SCORE_LEVEL_DEFAULT,
    };

    if (line == NULL) {
        return;
    }

    if (strncpy_s(cmd_line, sizeof(cmd_line), line, sizeof(cmd_line) - 1) != EOK) {
        return;
    }

    mine_zw101_to_upper(cmd_line);
    cursor = cmd_line;

    op = mine_zw101_next_token(&cursor);
    if (op == NULL) {
        return;
    }

    if ((strcmp(op, "FP") != 0) && (strcmp(op, "ZW101") != 0)) {
        return;
    }

    op = mine_zw101_next_token(&cursor);
    if (op == NULL) {
        cmd.op = MINE_ZW101_CMD_HELP;
        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if (strcmp(op, "HELP") == 0) {
        cmd.op = MINE_ZW101_CMD_HELP;
        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if (strcmp(op, "STATUS") == 0) {
        cmd.op = MINE_ZW101_CMD_STATUS;
        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if (strcmp(op, "CANCEL") == 0) {
        cmd.op = MINE_ZW101_CMD_CANCEL;
        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if (strcmp(op, "CLEAR") == 0) {
        cmd.op = MINE_ZW101_CMD_CLEAR;
        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if ((strcmp(op, "DEL") == 0) || (strcmp(op, "DELETE") == 0)) {
        arg0 = mine_zw101_next_token(&cursor);
        arg1 = mine_zw101_next_token(&cursor);
        cmd.op = MINE_ZW101_CMD_DELETE;

        if (!mine_zw101_parse_u16(arg0, &cmd.id)) {
            mine_zw101_set_status("ZW101:DEL ARG");
            return;
        }

        if ((arg1 != NULL) && (!mine_zw101_parse_u16(arg1, &cmd.count))) {
            mine_zw101_set_status("ZW101:DEL ARG");
            return;
        }

        if (cmd.count == 0U) {
            mine_zw101_set_status("ZW101:DEL ARG");
            return;
        }

        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if (strcmp(op, "ENROLL") == 0) {
        arg0 = mine_zw101_next_token(&cursor);
        arg1 = mine_zw101_next_token(&cursor);
        cmd.op = MINE_ZW101_CMD_ENROLL;

        if (!mine_zw101_parse_u16(arg0, &cmd.id)) {
            mine_zw101_set_status("ZW101:ENR ARG");
            return;
        }

        if (arg1 != NULL) {
            if (!mine_zw101_parse_u8(arg1, &cmd.enroll_times)) {
                mine_zw101_set_status("ZW101:ENR ARG");
                return;
            }
        }

        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    if (strcmp(op, "VERIFY") == 0) {
        uint16_t tmp = 0;

        arg0 = mine_zw101_next_token(&cursor);
        arg1 = mine_zw101_next_token(&cursor);
        cmd.op = MINE_ZW101_CMD_VERIFY;

        if ((arg0 != NULL) && (!mine_zw101_parse_u16(arg0, &tmp))) {
            mine_zw101_set_status("ZW101:VFY ARG");
            return;
        }

        if (arg0 != NULL) {
            /* 约定：单参数优先解释为 score(1~5)，否则解释为 target_id。 */
            if ((tmp >= 1U) && (tmp <= 5U)) {
                cmd.score_level = (uint8_t)tmp;
            } else {
                cmd.id = tmp;
            }
        }

        if (arg1 != NULL) {
            if (!mine_zw101_parse_u16(arg1, &cmd.id)) {
                mine_zw101_set_status("ZW101:VFY ARG");
                return;
            }
        }

        (void)mine_zw101_push_cmd_or_busy(&cmd);
        return;
    }

    mine_zw101_set_status("ZW101:CMD ?");
    cmd.op = MINE_ZW101_CMD_HELP;
    (void)mine_zw101_push_cmd_or_busy(&cmd);
}

/* ----------------------------- 对外接口实现 ----------------------------- */

/**
 * @brief 初始化 ZW101 模块并执行探测。
 *
 * @param bus ZW101 所在 UART 总线。
 * @return true  初始化成功且设备就绪。
 * @return false 初始化失败或设备未就绪。
 */
bool mine_zw101_init(uart_bus_t bus)
{
    zw101_hal_t hal = {0};

    g_mine_zw101_ready = false;
    g_mine_zw101_bus = bus;
    g_mine_zw101_next_probe_ms = (uint32_t)uapi_systick_get_ms() + MINE_ZW101_REPROBE_INTERVAL_MS;
    g_mine_zw101_last_match_id = MINE_ZW101_MATCH_ID_INVALID;
    g_mine_zw101_last_match_score = 0;

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

    /* 等待模块上电稳定后再握手。 */
    (void)osal_msleep(ZW101_PWRON_WAIT_PERIOD);

    if (mine_zw101_probe_ready()) {
        return true;
    }

    return false;
}

/**
 * @brief 向协议解析器投喂串口数据。
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

    /*
     * 注意：这里不以 ready 作为前置条件，避免初始化阶段 ACK 无法进入解析器。
     */
    zw101_protocol_parse(&g_mine_zw101_ctx, data, len);
}

/**
 * @brief 尝试解析调试串口文本命令。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 输入字节流。
 * @param len  输入长度。
 * @return true  本次数据已被命令解析消费。
 * @return false 非命令数据，应继续按透传路径处理。
 */
bool mine_zw101_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
#if MINE_ZW101_DEBUG_CMD_ENABLE
    uint16_t idx;
    bool consumed = false;

    if ((bus != MINE_ZW101_DEBUG_UART_BUS) || (data == NULL) || (len == 0)) {
        return false;
    }

    for (idx = 0; idx < len; idx++) {
        uint8_t ch = data[idx];

        if ((!g_mine_zw101_dbg_capture) && (g_mine_zw101_dbg_line_len == 0U)) {
            if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '\t')) {
                continue;
            }

            if ((ch != 'F') && (ch != 'f') && (ch != 'Z') && (ch != 'z')) {
                return false;
            }

            g_mine_zw101_dbg_capture = true;
        }

        if (!g_mine_zw101_dbg_capture) {
            continue;
        }

        consumed = true;

        if ((ch == '\r') || (ch == '\n')) {
            if (g_mine_zw101_dbg_line_len > 0U) {
                g_mine_zw101_dbg_line[g_mine_zw101_dbg_line_len] = '\0';
                mine_zw101_handle_debug_line(g_mine_zw101_dbg_line);
                (void)memset_s(g_mine_zw101_dbg_line, sizeof(g_mine_zw101_dbg_line),
                    0, sizeof(g_mine_zw101_dbg_line));
                g_mine_zw101_dbg_line_len = 0;
            }
            g_mine_zw101_dbg_capture = false;
            continue;
        }

        if ((!isprint((int)ch)) && (ch != ' ') && (ch != '\t')) {
            g_mine_zw101_dbg_capture = false;
            g_mine_zw101_dbg_line_len = 0;
            (void)memset_s(g_mine_zw101_dbg_line, sizeof(g_mine_zw101_dbg_line),
                0, sizeof(g_mine_zw101_dbg_line));
            mine_zw101_set_status("ZW101:CMD CHAR");
            return true;
        }

        if (g_mine_zw101_dbg_line_len >= (MINE_ZW101_DEBUG_LINE_MAX - 1U)) {
            g_mine_zw101_dbg_capture = false;
            g_mine_zw101_dbg_line_len = 0;
            (void)memset_s(g_mine_zw101_dbg_line, sizeof(g_mine_zw101_dbg_line),
                0, sizeof(g_mine_zw101_dbg_line));
            mine_zw101_set_status("ZW101:CMD LONG");
            return true;
        }

        g_mine_zw101_dbg_line[g_mine_zw101_dbg_line_len++] = (char)ch;
    }

    /*
     * 兼容上位机不带 CRLF 的短命令（例如 "FP STATUS"）：
     * UART idle 回调通常已经是完整帧，此处按一整行处理。
     */
    if (consumed && g_mine_zw101_dbg_capture && (g_mine_zw101_dbg_line_len > 0U)) {
        g_mine_zw101_dbg_line[g_mine_zw101_dbg_line_len] = '\0';
        mine_zw101_handle_debug_line(g_mine_zw101_dbg_line);
        (void)memset_s(g_mine_zw101_dbg_line, sizeof(g_mine_zw101_dbg_line),
            0, sizeof(g_mine_zw101_dbg_line));
        g_mine_zw101_dbg_line_len = 0;
        g_mine_zw101_dbg_capture = false;
    }

    return consumed;
#else
    (void)bus;
    (void)data;
    (void)len;
    return false;
#endif
}

/**
 * @brief 请求执行自动注册模板。
 *
 * @param template_id 目标模板 ID。
 * @return true  请求受理成功。
 * @return false 请求被拒绝（未就绪或忙）。
 */
bool mine_zw101_request_enroll(uint16_t template_id)
{
    mine_zw101_cmd_t cmd = {
        MINE_ZW101_CMD_ENROLL,
        template_id,
        1,
        MINE_ZW101_AUTO_ENROLL_TIMES_DEFAULT,
        MINE_ZW101_AUTO_VERIFY_SCORE_LEVEL_DEFAULT,
    };

    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if (!mine_zw101_push_cmd_or_busy(&cmd)) {
        return false;
    }

    mine_zw101_set_status_fmt("ZW101:ENR REQ %u", template_id);
    return true;
}

/**
 * @brief 请求执行自动验证指纹。
 *
 * @return true  请求受理成功。
 * @return false 请求被拒绝（未就绪或忙）。
 */
bool mine_zw101_request_verify(void)
{
    mine_zw101_cmd_t cmd = {
        MINE_ZW101_CMD_VERIFY,
        MINE_ZW101_AUTO_VERIFY_ID_DEFAULT,
        1,
        MINE_ZW101_AUTO_ENROLL_TIMES_DEFAULT,
        MINE_ZW101_AUTO_VERIFY_SCORE_LEVEL_DEFAULT,
    };

    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if (!mine_zw101_push_cmd_or_busy(&cmd)) {
        return false;
    }

    mine_zw101_set_status("ZW101:VFY REQ");
    return true;
}

/**
 * @brief 周期处理函数。
 *
 * 主循环每次调用会执行：
 * 1) 未就绪时的后台重探测；
 * 2) 自动验证周期调度（可选）；
 * 3) 执行一条挂起命令。
 */
void mine_zw101_process(void)
{
    mine_zw101_cmd_t cmd;
    uint32_t now_ms = (uint32_t)uapi_systick_get_ms();

    if (!g_mine_zw101_ready) {
        if ((int32_t)(now_ms - g_mine_zw101_next_probe_ms) >= 0) {
            (void)mine_zw101_probe_ready();
            g_mine_zw101_next_probe_ms = now_ms + MINE_ZW101_REPROBE_INTERVAL_MS;
        }
    }

#if MINE_ZW101_AUTO_VERIFY_ENABLE
    if (g_mine_zw101_ready && (!mine_zw101_cmd_busy()) &&
        ((int32_t)(now_ms - g_mine_zw101_next_verify_ms) >= 0)) {
        mine_zw101_cmd_t auto_verify = {
            MINE_ZW101_CMD_VERIFY,
            MINE_ZW101_AUTO_VERIFY_ID_DEFAULT,
            1,
            MINE_ZW101_AUTO_ENROLL_TIMES_DEFAULT,
            MINE_ZW101_AUTO_VERIFY_SCORE_LEVEL_DEFAULT,
        };

        if (mine_zw101_push_cmd(&auto_verify)) {
            g_mine_zw101_next_verify_ms = now_ms + MINE_ZW101_AUTO_VERIFY_INTERVAL_MS;
        }
    }
#endif

    if (!mine_zw101_pop_cmd(&cmd)) {
        return;
    }

    mine_zw101_exec_cmd(&cmd);
    mine_zw101_finish_cmd();
}

/**
 * @brief 读取并清除一次状态脏标记。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return true  读取成功且有新状态。
 * @return false 无新状态或参数非法。
 */
bool mine_zw101_get_status(char *buf, uint16_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0U) || (!g_mine_zw101_status_dirty)) {
        return false;
    }

    if (snprintf_s(buf, buf_len, buf_len - 1, "%s", g_mine_zw101_status_text) <= 0) {
        return false;
    }

    g_mine_zw101_status_dirty = false;
    return true;
}

#else

/* 功能关闭时提供空实现，避免链接错误。 */
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

bool mine_zw101_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    (void)bus;
    (void)data;
    (void)len;
    return false;
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
