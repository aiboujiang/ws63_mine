/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 ZW101 业务模块实现。
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

/* 冷启动场景下的握手重试参数。 */
#define MINE_ZW101_HANDSHAKE_RETRY 3
#define MINE_ZW101_HANDSHAKE_RETRY_GAP_MS 40

/* 取图重试参数：用于等待手指放置到位。 */
#define MINE_ZW101_CAPTURE_RETRY 25
#define MINE_ZW101_CAPTURE_RETRY_GAP_MS 120
#define MINE_ZW101_ENROLL_SAMPLE_GAP_MS 300

/* 调试命令行长度上限。 */
#define MINE_ZW101_DEBUG_LINE_MAX 96
/* ZW101 索引表位图长度（1 页 256 个模板位）。 */
#define MINE_ZW101_INDEX_BITMAP_LEN 32
/* ZW101 索引表最大页数（常见为 0~3，共 4 页）。 */
#define MINE_ZW101_INDEX_PAGE_COUNT 4

/* 无效匹配 ID 标记值。 */
#define MINE_ZW101_MATCH_ID_INVALID 0xFFFF

/* ZW101 业务工作状态机。 */
typedef enum {
    MINE_ZW101_WORK_IDLE = 0,
    MINE_ZW101_WORK_ENROLL,
    MINE_ZW101_WORK_VERIFY,
} mine_zw101_work_t;

/* ZW101 串口调试命令类型。 */
typedef enum {
    MINE_ZW101_DBG_OP_NONE = 0,
    MINE_ZW101_DBG_OP_HELP,
    MINE_ZW101_DBG_OP_STATUS,
    MINE_ZW101_DBG_OP_VERIFY,
    MINE_ZW101_DBG_OP_ENROLL,
    MINE_ZW101_DBG_OP_LIST,
    MINE_ZW101_DBG_OP_DELETE,
    MINE_ZW101_DBG_OP_CLEAR,
} mine_zw101_dbg_op_t;

/* 待执行调试命令。 */
typedef struct {
    mine_zw101_dbg_op_t op;
    uint16_t id;
    uint16_t count;
} mine_zw101_dbg_cmd_t;

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

/* 调试命令行缓存状态。 */
static bool g_mine_zw101_dbg_capture = false;
static char g_mine_zw101_dbg_line[MINE_ZW101_DEBUG_LINE_MAX] = {0};
static uint16_t g_mine_zw101_dbg_line_len = 0;

/* 待调度的调试命令（在任务线程中执行）。 */
static mine_zw101_dbg_cmd_t g_mine_zw101_dbg_cmd = { MINE_ZW101_DBG_OP_NONE, 0, 0 };

/* 列表查询时缓存当前页位图。 */
static bool g_mine_zw101_index_valid = false;
static uint8_t g_mine_zw101_index_page = 0;
static uint8_t g_mine_zw101_index_req_page = 0;
static uint8_t g_mine_zw101_index_bitmap[MINE_ZW101_INDEX_BITMAP_LEN] = {0};

static bool mine_zw101_enqueue_debug_cmd(mine_zw101_dbg_op_t op, uint16_t id, uint16_t count);
static void mine_zw101_print_help(void);
static void mine_zw101_handle_debug_line(const char *line);
static void mine_zw101_dump_index_page(uint8_t page, uint16_t *total_nums);
static bool mine_zw101_run_list_flow(void);
static bool mine_zw101_run_delete_flow(uint16_t id, uint16_t count);
static bool mine_zw101_run_clear_flow(void);
static void mine_zw101_exec_debug_cmd(const mine_zw101_dbg_cmd_t *cmd);

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
 * @brief 获取 ZW101 命令码的可读名称。
 *
 * 命令名称与协议手册第 3.3 节保持一致，便于定位具体指令步骤。
 *
 * @param cmd 命令码。
 * @return const char* 命令名称。
 */
static const char *mine_zw101_cmd_name(uint8_t cmd)
{
    switch (cmd) {
        case ZW101_CMD_MATCH_GETIMAGE:
            return "PS_GetImage";
        case ZW101_CMD_ENROLL_GETIMAGE:
            return "PS_GetEnrollImage";
        case ZW101_CMD_GEN_EXTRACT:
            return "PS_GenChar";
        case ZW101_CMD_SEARCH_TEMPLATE:
            return "PS_Search";
        case ZW101_CMD_GEN_TEMPLATE:
            return "PS_RegModel";
        case ZW101_CMD_STORE_TEMPLATE:
            return "PS_StoreChar";
        case ZW101_CMD_DEL_TEMPLATE:
            return "PS_DeletChar";
        case ZW101_CMD_EMPTY_TEMPLATE:
            return "PS_Empty";
        case ZW101_CMD_READ_TEMPLATE_INDEX_TABLE:
            return "PS_ReadIndexTable";
        case ZW101_CMD_AUTO_ENROLL:
            return "PS_AutoEnroll";
        case ZW101_CMD_AUTO_MATCH:
            return "PS_AutoIdentify";
        default:
            return "PS_Unknown";
    }
}

/**
 * @brief 获取确认码的手册释义文本。
 *
 * 依据《指纹模组产品用户手册》3.2“确认码定义”整理，
 * 用于日志中快速定位失败原因。
 *
 * @param ack_code 确认码。
 * @return const char* 释义文本。
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
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 统一打印 ZW101 单步命令输入输出日志。
 *
 * 日志格式固定包含流程名、步骤名、命令名、确认码与手册释义，
 * 用于快速判断“识别/录入”各阶段是否成功。
 *
 * @param flow_name 流程名（例如 ENROLL/VERIFY）。
 * @param step_name 步骤名（例如 CAPTURE#1/EXTRACT）。
 * @param cmd       命令码。
 * @param ret_code  调用返回值，0=成功，非0=失败。
 * @param ack_code  确认码。
 */
static void mine_zw101_log_step_result(const char *flow_name, const char *step_name,
    uint8_t cmd, int ret_code, uint8_t ack_code)
{
    const char *flow = (flow_name == NULL) ? "FLOW" : flow_name;
    const char *step = (step_name == NULL) ? "STEP" : step_name;

    osal_printk("[mine zw101] %s %s %s(0x%02X) %s ack:0x%02X(%s)\r\n",
        flow, step, mine_zw101_cmd_name(cmd), cmd,
        (ret_code == 0) ? "OK" : "FAIL",
        ack_code, mine_zw101_ack_desc(ack_code));
}

/**
 * @brief 将调试命令加入待执行队列。
 *
 * @param op    命令类型。
 * @param id    命令附带模板 ID。
 * @param count 命令附带数量参数。
 * @return true  入队成功。
 * @return false 队列忙，入队失败。
 */
static bool mine_zw101_enqueue_debug_cmd(mine_zw101_dbg_op_t op, uint16_t id, uint16_t count)
{
    if (g_mine_zw101_dbg_cmd.op != MINE_ZW101_DBG_OP_NONE) {
        mine_zw101_set_status("ZW101:CMD BUSY");
        return false;
    }

    g_mine_zw101_dbg_cmd.op = op;
    g_mine_zw101_dbg_cmd.id = id;
    g_mine_zw101_dbg_cmd.count = count;
    return true;
}

/**
 * @brief 输出调试命令帮助信息。
 */
static void mine_zw101_print_help(void)
{
    osal_printk("[mine zw101] cmd help:\r\n");
    osal_printk("[mine zw101]   FP HELP\r\n");
    osal_printk("[mine zw101]   FP STATUS\r\n");
    osal_printk("[mine zw101]   FP VERIFY\r\n");
    osal_printk("[mine zw101]   FP ENROLL <id>\r\n");
    osal_printk("[mine zw101]   FP LIST\r\n");
    osal_printk("[mine zw101]   FP DEL <id> [count]\r\n");
    osal_printk("[mine zw101]   FP CLEAR\r\n");
}

/**
 * @brief 将字符串转为大写，便于命令不区分大小写处理。
 *
 * @param str 待处理字符串。
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
 * @brief 从命令行中提取下一个 token（空格分隔）。
 *
 * @param cursor 命令行游标。
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
 * @brief 解析 16 位无符号整数参数，支持十进制与 0x 前缀。
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
 * @brief 解析一行串口调试命令并入队。
 *
 * 支持前缀 `FP` 或 `ZW101`，例如：
 * `FP ENROLL 1`、`FP LIST`、`FP DEL 1 1`、`FP CLEAR`。
 *
 * @param line 命令行字符串。
 */
static void mine_zw101_handle_debug_line(const char *line)
{
    char cmd_line[MINE_ZW101_DEBUG_LINE_MAX] = {0};
    char *cursor;
    char *op;
    char *arg0;
    char *arg1;
    uint16_t id;
    uint16_t count = 1;

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
        (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_HELP, 0, 0);
        return;
    }

    if (strcmp(op, "HELP") == 0) {
        (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_HELP, 0, 0);
        return;
    }

    if (strcmp(op, "STATUS") == 0) {
        (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_STATUS, 0, 0);
        return;
    }

    if (strcmp(op, "VERIFY") == 0) {
        (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_VERIFY, 0, 0);
        return;
    }

    if (strcmp(op, "LIST") == 0) {
        (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_LIST, 0, 0);
        return;
    }

    if (strcmp(op, "CLEAR") == 0) {
        (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_CLEAR, 0, 0);
        return;
    }

    if (strcmp(op, "ENROLL") == 0) {
        arg0 = mine_zw101_next_token(&cursor);
        if ((!mine_zw101_parse_u16(arg0, &id)) ||
            (!mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_ENROLL, id, 1))) {
            mine_zw101_set_status("ZW101:CMD ARG");
        }
        return;
    }

    if ((strcmp(op, "DEL") == 0) || (strcmp(op, "DELETE") == 0)) {
        arg0 = mine_zw101_next_token(&cursor);
        arg1 = mine_zw101_next_token(&cursor);
        if (!mine_zw101_parse_u16(arg0, &id)) {
            mine_zw101_set_status("ZW101:CMD ARG");
            return;
        }
        if ((arg1 != NULL) && (!mine_zw101_parse_u16(arg1, &count))) {
            mine_zw101_set_status("ZW101:CMD ARG");
            return;
        }
        if (!mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_DELETE, id, count)) {
            mine_zw101_set_status("ZW101:CMD BUSY");
        }
        return;
    }

    mine_zw101_set_status("ZW101:CMD ?");
    (void)mine_zw101_enqueue_debug_cmd(MINE_ZW101_DBG_OP_HELP, 0, 0);
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
        return;
    }

    if ((evt->cmd == ZW101_CMD_READ_TEMPLATE_INDEX_TABLE) && (evt->ack_code == ZW101_ACK_SUCCESS) &&
        (evt->payload != NULL) && (evt->payload_len >= (1 + MINE_ZW101_INDEX_BITMAP_LEN))) {
        if (memcpy_s(g_mine_zw101_index_bitmap, sizeof(g_mine_zw101_index_bitmap),
            &evt->payload[1], MINE_ZW101_INDEX_BITMAP_LEN) == EOK) {
            g_mine_zw101_index_page = g_mine_zw101_index_req_page;
            g_mine_zw101_index_valid = true;
        }
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
 * @brief 打印一页索引表中的已占用模板 ID。
 *
 * @param page       索引页号。
 * @param total_nums 累计模板总数（可为 NULL）。
 */
static void mine_zw101_dump_index_page(uint8_t page, uint16_t *total_nums)
{
    uint16_t byte_idx;
    uint8_t bit_idx;
    uint16_t line_count = 0;
    bool page_has_id = false;

    osal_printk("[mine zw101] list page:%u ->", (unsigned int)page);
    for (byte_idx = 0; byte_idx < MINE_ZW101_INDEX_BITMAP_LEN; byte_idx++) {
        for (bit_idx = 0; bit_idx < 8; bit_idx++) {
            uint16_t template_id;
            uint8_t bit_mask = (uint8_t)(1U << bit_idx);

            if ((g_mine_zw101_index_bitmap[byte_idx] & bit_mask) == 0) {
                continue;
            }

            template_id = (uint16_t)(((uint16_t)page << 8) + (byte_idx * 8U) + bit_idx);
            osal_printk(" %u", (unsigned int)template_id);
            page_has_id = true;
            line_count++;

            if ((total_nums != NULL) && (*total_nums < 0xFFFFU)) {
                (*total_nums)++;
            }

            /* 每输出 16 个 ID 换一行，避免日志行过长。 */
            if ((line_count % 16U) == 0U) {
                osal_printk("\r\n[mine zw101]              ");
            }
        }
    }

    if (!page_has_id) {
        osal_printk(" empty");
    }
    osal_printk("\r\n");
}

/**
 * @brief 执行指纹模板列表查询流程。
 *
 * 通过读取索引表页位图，输出所有已占用模板 ID。
 *
 * @return true  查询成功。
 * @return false 查询失败。
 */
static bool mine_zw101_run_list_flow(void)
{
    uint8_t page;
    uint16_t total_nums = 0;
    bool queried = false;

    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if (g_mine_zw101_work != MINE_ZW101_WORK_IDLE) {
        mine_zw101_set_status("ZW101:BUSY");
        return false;
    }

    osal_printk("[mine zw101] list begin\r\n");
    for (page = 0; page < MINE_ZW101_INDEX_PAGE_COUNT; page++) {
        g_mine_zw101_index_req_page = page;
        g_mine_zw101_index_valid = false;

        if (zw101_cmd_get_id_availability(&g_mine_zw101_ctx, page) != 0) {
            /* 页越界通常意味着设备不支持更高页号，前面结果仍然有效。 */
            if ((g_mine_zw101_ctx.ack_code == ZW101_PS_ADDRESS_OVER) && (page > 0)) {
                break;
            }
            mine_zw101_set_status_fmt("ZW101:LIST E%02X", g_mine_zw101_ctx.ack_code);
            return false;
        }

        if ((!g_mine_zw101_index_valid) || (g_mine_zw101_index_page != page)) {
            mine_zw101_set_status("ZW101:LIST ACK");
            return false;
        }

        queried = true;
        mine_zw101_dump_index_page(page, &total_nums);
    }

    if (!queried) {
        mine_zw101_set_status("ZW101:LIST FAIL");
        return false;
    }

    if (total_nums == 0) {
        mine_zw101_set_status("ZW101:LIST EMPTY");
    } else {
        mine_zw101_set_status_fmt("ZW101:LIST %u", total_nums);
    }
    osal_printk("[mine zw101] list total:%u\r\n", (unsigned int)total_nums);
    return true;
}

/**
 * @brief 执行删除模板流程。
 *
 * @param id    起始模板 ID。
 * @param count 删除模板数量。
 * @return true  删除成功。
 * @return false 删除失败。
 */
static bool mine_zw101_run_delete_flow(uint16_t id, uint16_t count)
{
    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if (g_mine_zw101_work != MINE_ZW101_WORK_IDLE) {
        mine_zw101_set_status("ZW101:BUSY");
        return false;
    }

    if (count == 0) {
        mine_zw101_set_status("ZW101:DEL ARG");
        return false;
    }

    mine_zw101_set_status_fmt("ZW101:DEL %u+%u", id, count);
    if (zw101_cmd_del_template(&g_mine_zw101_ctx, id, count) != 0) {
        if (g_mine_zw101_ctx.ack_code == ZW101_PS_TMPL_EMPTY) {
            mine_zw101_set_status("ZW101:DEL MISS");
        } else {
            mine_zw101_set_status_fmt("ZW101:DEL E%02X", g_mine_zw101_ctx.ack_code);
        }
        return false;
    }

    mine_zw101_set_status_fmt("ZW101:DEL OK %u", id);
    osal_printk("[mine zw101] delete success, id:%u count:%u\r\n",
        (unsigned int)id, (unsigned int)count);
    return true;
}

/**
 * @brief 执行清空模板库流程。
 *
 * @return true  清空成功。
 * @return false 清空失败。
 */
static bool mine_zw101_run_clear_flow(void)
{
    if (!g_mine_zw101_ready) {
        mine_zw101_set_status("ZW101:NOT READY");
        return false;
    }

    if (g_mine_zw101_work != MINE_ZW101_WORK_IDLE) {
        mine_zw101_set_status("ZW101:BUSY");
        return false;
    }

    mine_zw101_set_status("ZW101:CLEAR");
    if (zw101_cmd_empty_template(&g_mine_zw101_ctx) != 0) {
        /* 已空库场景按成功处理，方便重复测试。 */
        if (g_mine_zw101_ctx.ack_code == ZW101_PS_TMPL_EMPTY) {
            mine_zw101_set_status("ZW101:CLEAR OK");
            return true;
        }
        mine_zw101_set_status_fmt("ZW101:CLEAR E%02X", g_mine_zw101_ctx.ack_code);
        return false;
    }

    mine_zw101_set_status("ZW101:CLEAR OK");
    osal_printk("[mine zw101] clear all success\r\n");
    return true;
}

/**
 * @brief 执行一条已解析完成的调试命令。
 *
 * @param cmd 调试命令对象。
 */
static void mine_zw101_exec_debug_cmd(const mine_zw101_dbg_cmd_t *cmd)
{
    if ((cmd == NULL) || (cmd->op == MINE_ZW101_DBG_OP_NONE)) {
        return;
    }

    switch (cmd->op) {
        case MINE_ZW101_DBG_OP_HELP:
            mine_zw101_print_help();
            mine_zw101_set_status("ZW101:CMD HELP");
            break;
        case MINE_ZW101_DBG_OP_STATUS:
            osal_printk("[mine zw101] status ready:%u work:%u text:%s\r\n",
                (unsigned int)g_mine_zw101_ready,
                (unsigned int)g_mine_zw101_work,
                g_mine_zw101_status_text);
            mine_zw101_set_status("ZW101:STATUS");
            break;
        case MINE_ZW101_DBG_OP_VERIFY:
            osal_printk("[mine zw101] cmd in: FP VERIFY\r\n");
            if (!mine_zw101_request_verify()) {
                mine_zw101_set_status("ZW101:VFY BUSY");
                osal_printk("[mine zw101] cmd out: VERIFY reject\r\n");
            } else {
                osal_printk("[mine zw101] cmd out: VERIFY accept\r\n");
            }
            break;
        case MINE_ZW101_DBG_OP_ENROLL:
            osal_printk("[mine zw101] cmd in: FP ENROLL %u\r\n", (unsigned int)cmd->id);
            if (!mine_zw101_request_enroll(cmd->id)) {
                mine_zw101_set_status("ZW101:ENR BUSY");
                osal_printk("[mine zw101] cmd out: ENROLL reject\r\n");
            } else {
                osal_printk("[mine zw101] cmd out: ENROLL accept\r\n");
            }
            break;
        case MINE_ZW101_DBG_OP_LIST:
            (void)mine_zw101_run_list_flow();
            break;
        case MINE_ZW101_DBG_OP_DELETE:
            (void)mine_zw101_run_delete_flow(cmd->id, cmd->count);
            break;
        case MINE_ZW101_DBG_OP_CLEAR:
            (void)mine_zw101_run_clear_flow();
            break;
        default:
            break;
    }
}

/**
 * @brief 执行带重试的取图流程。
 *
 * 当返回“无手指”时按配置间隔重试，其他错误立即失败。
 *
 * @param operate_cmd 取图操作码（录入或验证场景）。
 * @param ack_code    输出最终确认码，可为 NULL。
 * @return int 0 成功，-1 失败。
 */
static int mine_zw101_capture_with_retry(uint8_t operate_cmd, uint8_t *ack_code)
{
    uint8_t retry_idx;
    uint8_t last_ack = ZW101_PS_TIME_OUT;

    for (retry_idx = 0; retry_idx < MINE_ZW101_CAPTURE_RETRY; retry_idx++) {
        if (zw101_cmd_capture_image(&g_mine_zw101_ctx, operate_cmd) == 0) {
            if (ack_code != NULL) {
                *ack_code = ZW101_PS_OK;
            }
            return 0;
        }

        last_ack = g_mine_zw101_ctx.ack_code;

        /*
         * 无手指错误码在当前协议定义中统一为 0x02，
         * 这里使用单一判断，避免重复条件触发编译告警。
         */
        if (g_mine_zw101_ctx.ack_code == ZW101_PS_NO_FINGER) {
            (void)osal_msleep(MINE_ZW101_CAPTURE_RETRY_GAP_MS);
            continue;
        }

        if (ack_code != NULL) {
            *ack_code = last_ack;
        }
        return -1;
    }

    if (ack_code != NULL) {
        *ack_code = last_ack;
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
    uint8_t step_ack = 0xFF;

    osal_printk("[mine zw101] enroll start, id:%u\r\n", (unsigned int)template_id);
    mine_zw101_set_status_fmt("ZW101:ENR %u", template_id);

    if (mine_zw101_capture_with_retry(ZW101_CMD_ENROLL_GETIMAGE, &step_ack) != 0) {
        mine_zw101_log_step_result("ENROLL", "CAPTURE#1", ZW101_CMD_ENROLL_GETIMAGE, -1, step_ack);
        mine_zw101_set_status("ZW101:ENR CAP1");
        return false;
    }
    mine_zw101_log_step_result("ENROLL", "CAPTURE#1", ZW101_CMD_ENROLL_GETIMAGE, 0, step_ack);

    if (zw101_cmd_general_extract(&g_mine_zw101_ctx, 1) != 0) {
        mine_zw101_log_step_result("ENROLL", "EXTRACT#1", ZW101_CMD_GEN_EXTRACT, -1, g_mine_zw101_ctx.ack_code);
        mine_zw101_set_status("ZW101:ENR FEAT1");
        return false;
    }
    mine_zw101_log_step_result("ENROLL", "EXTRACT#1", ZW101_CMD_GEN_EXTRACT, 0, g_mine_zw101_ctx.ack_code);

    mine_zw101_set_status("ZW101:ENR NEXT");
    (void)osal_msleep(MINE_ZW101_ENROLL_SAMPLE_GAP_MS);

    if (mine_zw101_capture_with_retry(ZW101_CMD_ENROLL_GETIMAGE, &step_ack) != 0) {
        mine_zw101_log_step_result("ENROLL", "CAPTURE#2", ZW101_CMD_ENROLL_GETIMAGE, -1, step_ack);
        mine_zw101_set_status("ZW101:ENR CAP2");
        return false;
    }
    mine_zw101_log_step_result("ENROLL", "CAPTURE#2", ZW101_CMD_ENROLL_GETIMAGE, 0, step_ack);

    if (zw101_cmd_general_extract(&g_mine_zw101_ctx, 2) != 0) {
        mine_zw101_log_step_result("ENROLL", "EXTRACT#2", ZW101_CMD_GEN_EXTRACT, -1, g_mine_zw101_ctx.ack_code);
        mine_zw101_set_status("ZW101:ENR FEAT2");
        return false;
    }
    mine_zw101_log_step_result("ENROLL", "EXTRACT#2", ZW101_CMD_GEN_EXTRACT, 0, g_mine_zw101_ctx.ack_code);

    if (zw101_cmd_general_template(&g_mine_zw101_ctx) != 0) {
        mine_zw101_log_step_result("ENROLL", "REG_MODEL", ZW101_CMD_GEN_TEMPLATE, -1, g_mine_zw101_ctx.ack_code);
        mine_zw101_set_status("ZW101:ENR MODEL");
        return false;
    }
    mine_zw101_log_step_result("ENROLL", "REG_MODEL", ZW101_CMD_GEN_TEMPLATE, 0, g_mine_zw101_ctx.ack_code);

    if (zw101_cmd_store_template(&g_mine_zw101_ctx, 1, template_id) != 0) {
        mine_zw101_log_step_result("ENROLL", "STORE", ZW101_CMD_STORE_TEMPLATE, -1, g_mine_zw101_ctx.ack_code);
        if (g_mine_zw101_ctx.ack_code == ZW101_PS_TMPL_NOT_EMPTY) {
            mine_zw101_set_status("ZW101:ID USED");
        } else if (g_mine_zw101_ctx.ack_code == ZW101_PS_FP_DUPLICATION) {
            mine_zw101_set_status("ZW101:DUP FP");
        } else {
            mine_zw101_set_status("ZW101:ENR SAVE");
        }
        return false;
    }
    mine_zw101_log_step_result("ENROLL", "STORE", ZW101_CMD_STORE_TEMPLATE, 0, g_mine_zw101_ctx.ack_code);

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
    uint8_t step_ack = 0xFF;

    osal_printk("[mine zw101] verify start\r\n");
    g_mine_zw101_match_id = MINE_ZW101_MATCH_ID_INVALID;
    g_mine_zw101_match_score = 0;

    mine_zw101_set_status("ZW101:VERIFY");

    if (mine_zw101_capture_with_retry(ZW101_CMD_MATCH_GETIMAGE, &step_ack) != 0) {
        mine_zw101_log_step_result("VERIFY", "CAPTURE", ZW101_CMD_MATCH_GETIMAGE, -1, step_ack);
        mine_zw101_set_status("ZW101:VFY CAP");
        return false;
    }
    mine_zw101_log_step_result("VERIFY", "CAPTURE", ZW101_CMD_MATCH_GETIMAGE, 0, step_ack);

    if (zw101_cmd_general_extract(&g_mine_zw101_ctx, 1) != 0) {
        mine_zw101_log_step_result("VERIFY", "EXTRACT", ZW101_CMD_GEN_EXTRACT, -1, g_mine_zw101_ctx.ack_code);
        mine_zw101_set_status("ZW101:VFY FEAT");
        return false;
    }
    mine_zw101_log_step_result("VERIFY", "EXTRACT", ZW101_CMD_GEN_EXTRACT, 0, g_mine_zw101_ctx.ack_code);

    if (zw101_cmd_match1n(&g_mine_zw101_ctx, 1, MINE_ZW101_SEARCH_START_PAGE, MINE_ZW101_SEARCH_PAGE_NUM) != 0) {
        mine_zw101_log_step_result("VERIFY", "SEARCH", ZW101_CMD_SEARCH_TEMPLATE, -1, g_mine_zw101_ctx.ack_code);
        if ((g_mine_zw101_ctx.ack_code == ZW101_PS_NOT_MATCH) ||
            (g_mine_zw101_ctx.ack_code == ZW101_PS_NOT_SEARCHED)) {
            mine_zw101_set_status("ZW101:NO MATCH");
        } else {
            mine_zw101_set_status_fmt("ZW101:VFY E%02X", g_mine_zw101_ctx.ack_code);
        }
        osal_printk("[mine zw101] verify failed, reason:%s\r\n", mine_zw101_ack_desc(g_mine_zw101_ctx.ack_code));
        return false;
    }
    mine_zw101_log_step_result("VERIFY", "SEARCH", ZW101_CMD_SEARCH_TEMPLATE, 0, g_mine_zw101_ctx.ack_code);

    if (g_mine_zw101_match_id == MINE_ZW101_MATCH_ID_INVALID) {
        mine_zw101_set_status("ZW101:MATCH OK");
        osal_printk("[mine zw101] verify success, id:unknown score:0\r\n");
    } else {
        mine_zw101_set_status_fmt("ZW101:ID%u S%u", g_mine_zw101_match_id, g_mine_zw101_match_score);
        osal_printk("[mine zw101] verify success, id:%u score:%u\r\n",
            (unsigned int)g_mine_zw101_match_id, (unsigned int)g_mine_zw101_match_score);
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
 * @brief 尝试消费来自调试串口的 ZW101 文本命令。
 *
 * 命令按行解析（`\r`/`\n` 结尾），仅当首字符为 `F/f/Z/z` 时进入命令捕获。
 * 非命令数据返回 false，保持原有透传路径。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 输入数据缓冲区。
 * @param len  输入数据长度。
 * @return true  本次数据已作为命令消费。
 * @return false 非命令数据。
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

        if ((!g_mine_zw101_dbg_capture) && (g_mine_zw101_dbg_line_len == 0)) {
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
            if (g_mine_zw101_dbg_line_len > 0) {
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

        if (g_mine_zw101_dbg_line_len >= (MINE_ZW101_DEBUG_LINE_MAX - 1)) {
            g_mine_zw101_dbg_capture = false;
            g_mine_zw101_dbg_line_len = 0;
            (void)memset_s(g_mine_zw101_dbg_line, sizeof(g_mine_zw101_dbg_line),
                0, sizeof(g_mine_zw101_dbg_line));
            mine_zw101_set_status("ZW101:CMD LONG");
            return true;
        }

        g_mine_zw101_dbg_line[g_mine_zw101_dbg_line_len++] = (char)ch;
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
#if MINE_ZW101_DEBUG_CMD_ENABLE
    /*
     * 调试命令在任务线程串行执行，避免与中断回调并发访问协议上下文。
     * 注意：命令分发必须放在 ready 判断之前，这样 HELP/STATUS 才能在设备未就绪时响应。
     */
    if (g_mine_zw101_dbg_cmd.op != MINE_ZW101_DBG_OP_NONE) {
        mine_zw101_dbg_cmd_t cmd = g_mine_zw101_dbg_cmd;
        g_mine_zw101_dbg_cmd.op = MINE_ZW101_DBG_OP_NONE;
        g_mine_zw101_dbg_cmd.id = 0;
        g_mine_zw101_dbg_cmd.count = 0;
        mine_zw101_exec_debug_cmd(&cmd);
    }
#endif

    if (!g_mine_zw101_ready) {
        return;
    }

#if MINE_ZW101_AUTO_VERIFY_ENABLE
    uint32_t now_ms;

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
