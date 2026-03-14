/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 LD2402 业务模块实现。
 */

#include "sle_uart_slave_ld2402.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "securec.h"
#include "sle_uart_slave.h"
#include "sle_uart_slave_module.h"
#include "soc_osal.h"
#include "systick.h"

#include "LD2402/LD2402.h"

#define osal_printk mine_slave_log

#if MINE_LD2402_ENABLE

/* 版本号与调试命令缓存长度。 */
#define MINE_LD2402_VERSION_TEXT_LEN 24
#define MINE_LD2402_DEBUG_LINE_MAX 96
#define MINE_LD2402_SN_TEXT_MAX_LEN ((MINE_LD2402_SN_MAX_LEN * 2) + 1)

/* LD2402 调试命令类型。 */
typedef enum {
    MINE_LD2402_DBG_OP_NONE = 0,
    MINE_LD2402_DBG_OP_HELP,
    MINE_LD2402_DBG_OP_STATUS,
    MINE_LD2402_DBG_OP_VERSION,
    MINE_LD2402_DBG_OP_SN,
    MINE_LD2402_DBG_OP_MODE_NORMAL,
    MINE_LD2402_DBG_OP_MODE_ENGINEERING,
    MINE_LD2402_DBG_OP_DIST,
    MINE_LD2402_DBG_OP_DELAY,
    MINE_LD2402_DBG_OP_GAIN,
    MINE_LD2402_DBG_OP_SAVE,
    MINE_LD2402_DBG_OP_AUTO_THRESHOLD,
    MINE_LD2402_DBG_OP_PROGRESS,
    MINE_LD2402_DBG_OP_ALARM,
    MINE_LD2402_DBG_OP_POWER,
    MINE_LD2402_DBG_OP_SYNC_3F,
} mine_ld2402_dbg_op_t;

/* 待执行调试命令。 */
typedef struct {
    mine_ld2402_dbg_op_t op;
    uint16_t value0;
    uint16_t value1;
    uint16_t value2;
} mine_ld2402_dbg_cmd_t;

/* 协议上下文与设备状态。 */
static LD2402_Handle_t g_mine_ld2402_handle;
static bool g_mine_ld2402_ready = false;
static uart_bus_t g_mine_ld2402_bus = MINE_LD2402_UART_BUS;

/* 面向 OLED 的状态文本缓存。 */
static volatile bool g_mine_ld2402_status_dirty = false;
static char g_mine_ld2402_status_text[MINE_LD2402_STATUS_TEXT_LEN] = "RADAR:OFF";

/* 调试命令行缓存。 */
static bool g_mine_ld2402_dbg_capture = false;
static char g_mine_ld2402_dbg_line[MINE_LD2402_DEBUG_LINE_MAX] = {0};
static uint16_t g_mine_ld2402_dbg_line_len = 0;

/* 在任务线程执行的调试命令队列（单槽位）。 */
static mine_ld2402_dbg_cmd_t g_mine_ld2402_dbg_cmd = { MINE_LD2402_DBG_OP_NONE, 0, 0, 0 };

/**
 * @brief 更新 LD2402 状态文本并置脏标记。
 *
 * @param text 状态文本。
 */
static void mine_ld2402_set_status(const char *text)
{
    if (text == NULL) {
        return;
    }

    if (snprintf_s(g_mine_ld2402_status_text, sizeof(g_mine_ld2402_status_text),
        sizeof(g_mine_ld2402_status_text) - 1, "%s", text) > 0) {
        g_mine_ld2402_status_dirty = true;
    }
}

/**
 * @brief 按格式化字符串更新 LD2402 状态文本。
 *
 * @param fmt printf 风格格式串。
 */
static void mine_ld2402_set_status_fmt(const char *fmt, ...)
{
    char status[MINE_LD2402_STATUS_TEXT_LEN] = {0};
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    if (vsnprintf_s(status, sizeof(status), sizeof(status) - 1, fmt, args) > 0) {
        mine_ld2402_set_status(status);
    }
    va_end(args);
}

/**
 * @brief 将调试命令加入待执行槽位。
 *
 * @param op     命令类型。
 * @param value0 命令参数 0。
 * @param value1 命令参数 1。
 * @param value2 命令参数 2。
 * @return true  入队成功。
 * @return false 槽位占用，入队失败。
 */
static bool mine_ld2402_enqueue_debug_cmd(mine_ld2402_dbg_op_t op,
    uint16_t value0, uint16_t value1, uint16_t value2)
{
    if (g_mine_ld2402_dbg_cmd.op != MINE_LD2402_DBG_OP_NONE) {
        mine_ld2402_set_status("RADAR:CMD BUSY");
        return false;
    }

    g_mine_ld2402_dbg_cmd.op = op;
    g_mine_ld2402_dbg_cmd.value0 = value0;
    g_mine_ld2402_dbg_cmd.value1 = value1;
    g_mine_ld2402_dbg_cmd.value2 = value2;
    return true;
}

/**
 * @brief 输出 LD2402 调试命令帮助。
 */
static void mine_ld2402_print_help(void)
{
    osal_printk("[mine ld2402] cmd help:\r\n");
    osal_printk("[mine ld2402]   LD HELP\r\n");
    osal_printk("[mine ld2402]   LD STATUS\r\n");
    osal_printk("[mine ld2402]   LD VERSION\r\n");
    osal_printk("[mine ld2402]   LD SN\r\n");
    osal_printk("[mine ld2402]   LD MODE NORMAL\r\n");
    osal_printk("[mine ld2402]   LD MODE ENGINEERING\r\n");
    osal_printk("[mine ld2402]   LD DIST <0.7~10.0>\r\n");
    osal_printk("[mine ld2402]   LD DELAY <sec>\r\n");
    osal_printk("[mine ld2402]   LD GAIN\r\n");
    osal_printk("[mine ld2402]   LD SAVE\r\n");
    osal_printk("[mine ld2402]   LD AUTO <trig10x> <hold10x> [static10x]\r\n");
    osal_printk("[mine ld2402]   LD PROGRESS\r\n");
    osal_printk("[mine ld2402]   LD ALARM\r\n");
    osal_printk("[mine ld2402]   LD PWR\r\n");
    osal_printk("[mine ld2402]   LD SAVE3F\r\n");
}

/**
 * @brief 字符串原地转大写，便于命令不区分大小写。
 *
 * @param str 待处理字符串。
 */
static void mine_ld2402_to_upper(char *str)
{
    uint16_t idx;

    if (str == NULL) {
        return;
    }

    for (idx = 0; str[idx] != '\0'; idx++) {
        str[idx] = (char)toupper((unsigned char)str[idx]);
    }
}

/**
 * @brief 从命令行中提取一个 token（空格/制表符分隔）。
 *
 * @param cursor 命令行游标。
 * @return char* token 起始地址，失败返回 NULL。
 */
static char *mine_ld2402_next_token(char **cursor)
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
 * @brief 解析 16 位无符号整数参数（支持十进制/0x 前缀）。
 *
 * @param token 输入字符串。
 * @param value 输出值。
 * @return true  解析成功。
 * @return false 解析失败。
 */
static bool mine_ld2402_parse_u16(const char *token, uint16_t *value)
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
 * @brief 解析自动门限系数（10 倍放大值）。
 *
 * 手册范围：0x000A~0x00C8（即 10~200）。
 *
 * @param token 输入字符串。
 * @param value 输出值。
 * @return true  解析成功且范围有效。
 * @return false 解析失败或越界。
 */
static bool mine_ld2402_parse_auto_coef_10x(const char *token, uint16_t *value)
{
    if (!mine_ld2402_parse_u16(token, value)) {
        return false;
    }

    return ((*value >= LD2402_AUTO_THRESHOLD_COEF_MIN) &&
        (*value <= LD2402_AUTO_THRESHOLD_COEF_MAX));
}

/**
 * @brief 解析最大距离参数（单位 0.1m，输入格式例如 3 或 3.5）。
 *
 * @param token        输入字符串。
 * @param distance_01m 输出距离值（单位 0.1m）。
 * @return true  解析成功且范围有效（0.7~10.0m）。
 * @return false 解析失败或越界。
 */
static bool mine_ld2402_parse_distance_01m(const char *token, uint16_t *distance_01m)
{
    const char *cursor;
    uint32_t integer_part = 0;
    uint32_t decimal_part = 0;
    uint32_t distance;
    bool dot_seen = false;
    bool decimal_seen = false;

    if ((token == NULL) || (distance_01m == NULL) || (*token == '\0')) {
        return false;
    }

    cursor = token;
    while (*cursor != '\0') {
        if (*cursor == '.') {
            if (dot_seen) {
                return false;
            }
            dot_seen = true;
            cursor++;
            continue;
        }

        if (!isdigit((unsigned char)*cursor)) {
            return false;
        }

        if (!dot_seen) {
            integer_part = (integer_part * 10U) + (uint32_t)(*cursor - '0');
            if (integer_part > 100U) {
                return false;
            }
        } else {
            if (decimal_seen) {
                return false;
            }
            decimal_part = (uint32_t)(*cursor - '0');
            decimal_seen = true;
        }

        cursor++;
    }

    if (dot_seen && (!decimal_seen)) {
        return false;
    }

    distance = (integer_part * 10U) + decimal_part;
    if ((distance < 7U) || (distance > 100U)) {
        return false;
    }

    *distance_01m = (uint16_t)distance;
    return true;
}

/**
 * @brief 将 SN 字节数组转换为十六进制字符串并输出日志。
 *
 * @param sn     SN 字节数组。
 * @param sn_len SN 字节长度。
 */
static void mine_ld2402_dump_sn_hex(const uint8_t *sn, int sn_len)
{
    char sn_text[MINE_LD2402_SN_TEXT_MAX_LEN] = {0};
    uint16_t idx;
    uint16_t pos = 0;
    int write_len;

    if ((sn == NULL) || (sn_len <= 0)) {
        osal_printk("[mine ld2402] sn read failed\r\n");
        return;
    }

    if (sn_len > MINE_LD2402_SN_MAX_LEN) {
        sn_len = MINE_LD2402_SN_MAX_LEN;
    }

    for (idx = 0; idx < (uint16_t)sn_len; idx++) {
        if ((sizeof(sn_text) - pos) <= 1) {
            break;
        }

        write_len = snprintf_s(&sn_text[pos], sizeof(sn_text) - pos,
            (sizeof(sn_text) - pos) - 1, "%02X", sn[idx]);
        if (write_len <= 0) {
            break;
        }
        pos = (uint16_t)(pos + (uint16_t)write_len);
    }

    if (pos == 0) {
        osal_printk("[mine ld2402] sn read failed\r\n");
        return;
    }

    osal_printk("[mine ld2402] sn(hex):%s\r\n", sn_text);
}

/**
 * @brief LD2402 HAL 串口发送适配。
 *
 * @param data 待发送数据。
 * @param len  数据长度。
 * @return int 0 成功，-1 失败。
 */
static int mine_ld2402_uart_send_adapter(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return -1;
    }

    if (uapi_uart_write(g_mine_ld2402_bus, data, len, 0) < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief LD2402 HAL 毫秒计时适配。
 *
 * @return uint32_t 当前系统毫秒 tick。
 */
static uint32_t mine_ld2402_get_tick_ms_adapter(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

/**
 * @brief LD2402 HAL 延时适配。
 *
 * @param ms 延时毫秒数。
 */
static void mine_ld2402_delay_ms_adapter(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief LD2402 数据帧回调。
 *
 * 将运动状态与距离压缩为 OLED 可显示文本。
 *
 * @param data 解析后的雷达数据帧。
 */
static void mine_ld2402_data_callback(LD2402_DataFrame_t *data)
{
    if (data == NULL) {
        return;
    }

    mine_ld2402_set_status_fmt("RADAR:S%u D:%u",
        (unsigned int)data->status, (unsigned int)data->distance_cm);
}

/**
 * @brief 执行一条已解析完成的 LD2402 调试命令。
 *
 * @param cmd 调试命令对象。
 */
static void mine_ld2402_exec_debug_cmd(const mine_ld2402_dbg_cmd_t *cmd)
{
    char version[MINE_LD2402_VERSION_TEXT_LEN] = {0};
    char sn_text[MINE_LD2402_SN_TEXT_MAX_LEN] = {0};
    uint8_t sn_buf[MINE_LD2402_SN_MAX_LEN] = {0};
    uint16_t progress = 0;
    uint32_t power_inter = 0;
    LD2402_AutoThresholdAlarm_t alarm = {0};
    int sn_len;

    if ((cmd == NULL) || (cmd->op == MINE_LD2402_DBG_OP_NONE)) {
        return;
    }

    if ((!g_mine_ld2402_ready) && (cmd->op != MINE_LD2402_DBG_OP_HELP) &&
        (cmd->op != MINE_LD2402_DBG_OP_STATUS)) {
        mine_ld2402_set_status("RADAR:NOT READY");
        return;
    }

    switch (cmd->op) {
        case MINE_LD2402_DBG_OP_HELP:
            mine_ld2402_print_help();
            mine_ld2402_set_status("RADAR:CMD HELP");
            break;
        case MINE_LD2402_DBG_OP_STATUS:
            osal_printk("[mine ld2402] status ready:%u cfg:%u bus:%s text:%s\r\n",
                (unsigned int)g_mine_ld2402_ready,
                (unsigned int)g_mine_ld2402_handle.is_in_config_mode,
                mine_slave_uart_bus_name((uint8_t)g_mine_ld2402_bus),
                g_mine_ld2402_status_text);
            mine_ld2402_set_status("RADAR:STATUS");
            break;
        case MINE_LD2402_DBG_OP_VERSION:
            if (LD2402_GetVersion(&g_mine_ld2402_handle, version, sizeof(version)) == 0) {
                osal_printk("[mine ld2402] version:%s\r\n", version);
                mine_ld2402_set_status("RADAR:VER OK");
            } else {
                mine_ld2402_set_status("RADAR:VER FAIL");
            }
            break;
        case MINE_LD2402_DBG_OP_SN:
            if (LD2402_GetSN_Char(&g_mine_ld2402_handle, sn_text, sizeof(sn_text)) == 0) {
                osal_printk("[mine ld2402] sn(char):%s\r\n", sn_text);
                mine_ld2402_set_status("RADAR:SN OK");
            } else {
                sn_len = LD2402_GetSN_Hex(&g_mine_ld2402_handle, sn_buf, sizeof(sn_buf));
                if (sn_len > 0) {
                    mine_ld2402_dump_sn_hex(sn_buf, sn_len);
                    mine_ld2402_set_status("RADAR:SN HEX");
                } else {
                    mine_ld2402_set_status("RADAR:SN FAIL");
                }
            }
            break;
        case MINE_LD2402_DBG_OP_MODE_NORMAL:
            if (LD2402_SetNormalMode(&g_mine_ld2402_handle) == 0) {
                mine_ld2402_set_status("RADAR:MODE N");
            } else {
                mine_ld2402_set_status("RADAR:MODE N !");
            }
            break;
        case MINE_LD2402_DBG_OP_MODE_ENGINEERING:
            if (LD2402_SetEngineeringMode(&g_mine_ld2402_handle) == 0) {
                mine_ld2402_set_status("RADAR:MODE E");
            } else {
                mine_ld2402_set_status("RADAR:MODE E !");
            }
            break;
        case MINE_LD2402_DBG_OP_DIST:
            if (LD2402_SetMaxDistance(&g_mine_ld2402_handle, (float)cmd->value0 / 10.0f) == 0) {
                mine_ld2402_set_status_fmt("RADAR:DIST %u.%u",
                    (unsigned int)(cmd->value0 / 10), (unsigned int)(cmd->value0 % 10));
            } else {
                mine_ld2402_set_status("RADAR:DIST !");
            }
            break;
        case MINE_LD2402_DBG_OP_DELAY:
            if (LD2402_SetDisappearDelay(&g_mine_ld2402_handle, cmd->value0) == 0) {
                mine_ld2402_set_status_fmt("RADAR:DELAY %u", (unsigned int)cmd->value0);
            } else {
                mine_ld2402_set_status("RADAR:DELAY !");
            }
            break;
        case MINE_LD2402_DBG_OP_GAIN:
            if (LD2402_AutoGainAdjust(&g_mine_ld2402_handle) == 0) {
                mine_ld2402_set_status("RADAR:GAIN OK");
            } else {
                mine_ld2402_set_status("RADAR:GAIN !");
            }
            break;
        case MINE_LD2402_DBG_OP_SAVE:
            if (LD2402_SaveParams(&g_mine_ld2402_handle) == 0) {
                mine_ld2402_set_status("RADAR:SAVE OK");
            } else {
                mine_ld2402_set_status("RADAR:SAVE !");
            }
            break;
        case MINE_LD2402_DBG_OP_AUTO_THRESHOLD:
            if (LD2402_StartAutoThreshold(&g_mine_ld2402_handle,
                cmd->value0, cmd->value1, cmd->value2) == 0) {
                mine_ld2402_set_status_fmt("RADAR:AUTO %u/%u/%u",
                    (unsigned int)cmd->value0,
                    (unsigned int)cmd->value1,
                    (unsigned int)cmd->value2);
            } else {
                mine_ld2402_set_status("RADAR:AUTO !");
            }
            break;
        case MINE_LD2402_DBG_OP_PROGRESS:
            if (LD2402_GetAutoThresholdProgress(&g_mine_ld2402_handle, &progress) == 0) {
                osal_printk("[mine ld2402] auto threshold progress:%u%%\r\n", (unsigned int)progress);
                mine_ld2402_set_status_fmt("RADAR:PROG %u%%", (unsigned int)progress);
            } else {
                mine_ld2402_set_status("RADAR:PROG !");
            }
            break;
        case MINE_LD2402_DBG_OP_ALARM:
            if (LD2402_GetAutoThresholdAlarm(&g_mine_ld2402_handle, &alarm) == 0) {
                osal_printk("[mine ld2402] alarm status:%u gate_bitmap:0x%04X\r\n",
                    (unsigned int)alarm.alarm_status,
                    (unsigned int)alarm.gate_bitmap);
                if (alarm.alarm_status == 0) {
                    mine_ld2402_set_status("RADAR:ALARM OK");
                } else {
                    mine_ld2402_set_status_fmt("RADAR:ALARM %u", (unsigned int)alarm.alarm_status);
                }
            } else {
                mine_ld2402_set_status("RADAR:ALARM !");
            }
            break;
        case MINE_LD2402_DBG_OP_POWER:
            if (LD2402_GetPowerInterference(&g_mine_ld2402_handle, &power_inter) == 0) {
                osal_printk("[mine ld2402] power interference:%u\r\n", (unsigned int)power_inter);
                mine_ld2402_set_status_fmt("RADAR:PWR %u", (unsigned int)power_inter);
            } else {
                mine_ld2402_set_status("RADAR:PWR !");
            }
            break;
        case MINE_LD2402_DBG_OP_SYNC_3F:
            if ((LD2402_RefreshSaveFlag(&g_mine_ld2402_handle) == 0) &&
                (LD2402_SaveParams(&g_mine_ld2402_handle) == 0)) {
                mine_ld2402_set_status("RADAR:3F OK");
            } else {
                mine_ld2402_set_status("RADAR:3F !");
            }
            break;
        default:
            break;
    }
}

/**
 * @brief 解析一行 LD2402 串口调试命令并入队。
 *
 * 支持前缀 `LD` 或 `LD2402`，例如：
 * `LD VERSION`、`LD MODE ENGINEERING`、`LD DIST 3.5`。
 *
 * @param line 命令行字符串。
 */
static void mine_ld2402_handle_debug_line(const char *line)
{
    char cmd_line[MINE_LD2402_DEBUG_LINE_MAX] = {0};
    char *cursor;
    char *op;
    char *arg0;
    char *arg1;
    char *arg2;
    uint16_t value0;
    uint16_t value1;
    uint16_t value2;

    if (line == NULL) {
        return;
    }

    if (strncpy_s(cmd_line, sizeof(cmd_line), line, sizeof(cmd_line) - 1) != EOK) {
        return;
    }

    mine_ld2402_to_upper(cmd_line);
    cursor = cmd_line;

    op = mine_ld2402_next_token(&cursor);
    if (op == NULL) {
        return;
    }

    if ((strcmp(op, "LD") != 0) && (strcmp(op, "LD2402") != 0)) {
        return;
    }

    op = mine_ld2402_next_token(&cursor);
    if (op == NULL) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_HELP, 0, 0, 0);
        return;
    }

    if (strcmp(op, "HELP") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_HELP, 0, 0, 0);
        return;
    }

    if (strcmp(op, "STATUS") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_STATUS, 0, 0, 0);
        return;
    }

    if (strcmp(op, "VERSION") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_VERSION, 0, 0, 0);
        return;
    }

    if (strcmp(op, "SN") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_SN, 0, 0, 0);
        return;
    }

    if (strcmp(op, "PROGRESS") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_PROGRESS, 0, 0, 0);
        return;
    }

    if (strcmp(op, "ALARM") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_ALARM, 0, 0, 0);
        return;
    }

    if ((strcmp(op, "PWR") == 0) || (strcmp(op, "POWER") == 0)) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_POWER, 0, 0, 0);
        return;
    }

    if ((strcmp(op, "SAVE3F") == 0) || (strcmp(op, "SYNC3F") == 0)) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_SYNC_3F, 0, 0, 0);
        return;
    }

    if (strcmp(op, "GAIN") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_GAIN, 0, 0, 0);
        return;
    }

    if (strcmp(op, "SAVE") == 0) {
        (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_SAVE, 0, 0, 0);
        return;
    }

    if (strcmp(op, "MODE") == 0) {
        arg0 = mine_ld2402_next_token(&cursor);
        if (arg0 == NULL) {
            mine_ld2402_set_status("RADAR:CMD ARG");
            return;
        }

        if ((strcmp(arg0, "N") == 0) || (strcmp(arg0, "NORMAL") == 0)) {
            (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_MODE_NORMAL, 0, 0, 0);
            return;
        }

        if ((strcmp(arg0, "E") == 0) || (strcmp(arg0, "ENG") == 0) ||
            (strcmp(arg0, "ENGINEERING") == 0)) {
            (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_MODE_ENGINEERING, 0, 0, 0);
            return;
        }

        mine_ld2402_set_status("RADAR:CMD ARG");
        return;
    }

    if (strcmp(op, "DIST") == 0) {
        arg0 = mine_ld2402_next_token(&cursor);
        if ((!mine_ld2402_parse_distance_01m(arg0, &value0)) ||
            (!mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_DIST, value0, 0, 0))) {
            mine_ld2402_set_status("RADAR:CMD ARG");
        }
        return;
    }

    if (strcmp(op, "DELAY") == 0) {
        arg0 = mine_ld2402_next_token(&cursor);
        if ((!mine_ld2402_parse_u16(arg0, &value0)) ||
            (!mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_DELAY, value0, 0, 0))) {
            mine_ld2402_set_status("RADAR:CMD ARG");
        }
        return;
    }

    if (strcmp(op, "AUTO") == 0) {
        arg0 = mine_ld2402_next_token(&cursor);
        arg1 = mine_ld2402_next_token(&cursor);
        arg2 = mine_ld2402_next_token(&cursor);
        if ((!mine_ld2402_parse_auto_coef_10x(arg0, &value0)) ||
            (!mine_ld2402_parse_auto_coef_10x(arg1, &value1))) {
            mine_ld2402_set_status("RADAR:CMD ARG");
            return;
        }

        if (arg2 != NULL) {
            if (!mine_ld2402_parse_auto_coef_10x(arg2, &value2)) {
                mine_ld2402_set_status("RADAR:CMD ARG");
                return;
            }
        } else {
            /* 未填写静止系数时，默认复用保持系数。 */
            value2 = value1;
        }

        if (!mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_AUTO_THRESHOLD,
            value0, value1, value2)) {
            mine_ld2402_set_status("RADAR:CMD BUSY");
        }
        return;
    }

    mine_ld2402_set_status("RADAR:CMD ?");
    (void)mine_ld2402_enqueue_debug_cmd(MINE_LD2402_DBG_OP_HELP, 0, 0, 0);
}

/**
 * @brief 初始化 LD2402 模块并读取基础信息。
 *
 * @param bus LD2402 所在 UART 总线。
 * @return true  初始化成功。
 * @return false 初始化失败。
 */
bool mine_ld2402_init(uart_bus_t bus)
{
    LD2402_HAL_t hal = {0};
    char version[MINE_LD2402_VERSION_TEXT_LEN] = {0};
    char sn_text[MINE_LD2402_SN_TEXT_MAX_LEN] = {0};
    uint8_t sn_buf[MINE_LD2402_SN_MAX_LEN] = {0};
    int sn_len;

    g_mine_ld2402_ready = false;
    g_mine_ld2402_bus = bus;
    g_mine_ld2402_dbg_cmd.op = MINE_LD2402_DBG_OP_NONE;
    g_mine_ld2402_dbg_cmd.value0 = 0;
    g_mine_ld2402_dbg_cmd.value1 = 0;
    g_mine_ld2402_dbg_cmd.value2 = 0;
    g_mine_ld2402_dbg_capture = false;
    g_mine_ld2402_dbg_line_len = 0;
    (void)memset_s(g_mine_ld2402_dbg_line, sizeof(g_mine_ld2402_dbg_line), 0, sizeof(g_mine_ld2402_dbg_line));

    if (!mine_slave_uart_bus_enabled(bus)) {
        mine_ld2402_set_status("RADAR:BUS OFF");
        return false;
    }

    hal.uart_send = mine_ld2402_uart_send_adapter;
    hal.get_tick_ms = mine_ld2402_get_tick_ms_adapter;
    hal.delay_ms = mine_ld2402_delay_ms_adapter;
    hal.uart_rx_irq_ctrl = NULL;

    LD2402_Init(&g_mine_ld2402_handle, &hal);
    g_mine_ld2402_handle.on_data_received = mine_ld2402_data_callback;

    /* 提前置 ready，确保初始化阶段下发命令时 ACK 能被回调解析。 */
    g_mine_ld2402_ready = true;

    if (LD2402_GetVersion(&g_mine_ld2402_handle, version, sizeof(version)) == 0) {
        osal_printk("[mine ld2402] version:%s\r\n", version);
    } else {
        mine_ld2402_set_status("RADAR:NO ACK");
        return false;
    }

    if (LD2402_GetSN_Char(&g_mine_ld2402_handle, sn_text, sizeof(sn_text)) == 0) {
        osal_printk("[mine ld2402] sn(char):%s\r\n", sn_text);
    } else {
        sn_len = LD2402_GetSN_Hex(&g_mine_ld2402_handle, sn_buf, sizeof(sn_buf));
        mine_ld2402_dump_sn_hex(sn_buf, sn_len);
    }

    /* 调试阶段默认切到工程模式，便于接收结构化数据帧。 */
    if (LD2402_SetEngineeringMode(&g_mine_ld2402_handle) == 0) {
        mine_ld2402_set_status("RADAR:ENG");
    } else {
        mine_ld2402_set_status("RADAR:READY");
    }

    return true;
}

/**
 * @brief 向 LD2402 协议解析器喂入串口数据。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 数据缓冲区。
 * @param len  数据长度。
 */
void mine_ld2402_feed(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    uint16_t idx;

    if ((!g_mine_ld2402_ready) || (bus != g_mine_ld2402_bus) || (data == NULL) || (len == 0)) {
        return;
    }

    for (idx = 0; idx < len; idx++) {
        LD2402_InputByte(&g_mine_ld2402_handle, data[idx]);
    }
}

/**
 * @brief 尝试消费来自调试串口的 LD2402 文本命令。
 *
 * 命令按行解析（`\r`/`\n` 结尾），仅当首字符为 `L/l` 时进入命令捕获。
 * 非命令数据返回 false，保持原有透传路径。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 输入数据缓冲区。
 * @param len  输入数据长度。
 * @return true  本次数据已作为命令消费。
 * @return false 非命令数据。
 */
bool mine_ld2402_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
#if MINE_LD2402_DEBUG_CMD_ENABLE
    uint16_t idx;
    bool consumed = false;

    if ((bus != MINE_LD2402_DEBUG_UART_BUS) || (data == NULL) || (len == 0)) {
        return false;
    }

    for (idx = 0; idx < len; idx++) {
        uint8_t ch = data[idx];

        if ((!g_mine_ld2402_dbg_capture) && (g_mine_ld2402_dbg_line_len == 0)) {
            if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '\t')) {
                continue;
            }

            if ((ch != 'L') && (ch != 'l')) {
                return false;
            }

            g_mine_ld2402_dbg_capture = true;
        }

        if (!g_mine_ld2402_dbg_capture) {
            continue;
        }

        consumed = true;
        if ((ch == '\r') || (ch == '\n')) {
            if (g_mine_ld2402_dbg_line_len > 0) {
                g_mine_ld2402_dbg_line[g_mine_ld2402_dbg_line_len] = '\0';
                mine_ld2402_handle_debug_line(g_mine_ld2402_dbg_line);
                (void)memset_s(g_mine_ld2402_dbg_line, sizeof(g_mine_ld2402_dbg_line),
                    0, sizeof(g_mine_ld2402_dbg_line));
                g_mine_ld2402_dbg_line_len = 0;
            }
            g_mine_ld2402_dbg_capture = false;
            continue;
        }

        if ((!isprint((int)ch)) && (ch != ' ') && (ch != '\t')) {
            g_mine_ld2402_dbg_capture = false;
            g_mine_ld2402_dbg_line_len = 0;
            (void)memset_s(g_mine_ld2402_dbg_line, sizeof(g_mine_ld2402_dbg_line),
                0, sizeof(g_mine_ld2402_dbg_line));
            mine_ld2402_set_status("RADAR:CMD CHAR");
            return true;
        }

        if (g_mine_ld2402_dbg_line_len >= (MINE_LD2402_DEBUG_LINE_MAX - 1)) {
            g_mine_ld2402_dbg_capture = false;
            g_mine_ld2402_dbg_line_len = 0;
            (void)memset_s(g_mine_ld2402_dbg_line, sizeof(g_mine_ld2402_dbg_line),
                0, sizeof(g_mine_ld2402_dbg_line));
            mine_ld2402_set_status("RADAR:CMD LONG");
            return true;
        }

        g_mine_ld2402_dbg_line[g_mine_ld2402_dbg_line_len++] = (char)ch;
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
 * @brief LD2402 业务周期处理函数。
 *
 * 在主循环中调用，负责串行执行调试命令，避免与 RX 回调并发修改协议上下文。
 */
void mine_ld2402_process(void)
{
    if (g_mine_ld2402_dbg_cmd.op != MINE_LD2402_DBG_OP_NONE) {
        mine_ld2402_dbg_cmd_t cmd = g_mine_ld2402_dbg_cmd;
        g_mine_ld2402_dbg_cmd.op = MINE_LD2402_DBG_OP_NONE;
        g_mine_ld2402_dbg_cmd.value0 = 0;
        g_mine_ld2402_dbg_cmd.value1 = 0;
        g_mine_ld2402_dbg_cmd.value2 = 0;
        mine_ld2402_exec_debug_cmd(&cmd);
    }
}

/**
 * @brief 获取待刷新的 LD2402 状态文本。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return true  获取成功且有新状态。
 * @return false 无新状态或参数错误。
 */
bool mine_ld2402_get_status(char *buf, uint16_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0) || (!g_mine_ld2402_status_dirty)) {
        return false;
    }

    if (snprintf_s(buf, buf_len, buf_len - 1, "%s", g_mine_ld2402_status_text) <= 0) {
        return false;
    }

    g_mine_ld2402_status_dirty = false;
    return true;
}

#else

/* 功能关闭时提供空实现，避免外部调用链接失败。 */
bool mine_ld2402_init(uart_bus_t bus)
{
    (void)bus;
    return false;
}

void mine_ld2402_feed(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    (void)bus;
    (void)data;
    (void)len;
}

bool mine_ld2402_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    (void)bus;
    (void)data;
    (void)len;
    return false;
}

void mine_ld2402_process(void)
{
}

bool mine_ld2402_get_status(char *buf, uint16_t buf_len)
{
    (void)buf;
    (void)buf_len;
    return false;
}

#endif