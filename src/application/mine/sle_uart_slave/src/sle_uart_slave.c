/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 UART0 <-> SLE 桥接。
 */

#include "sle_uart_slave.h"
#include "sle_uart_slave_ld2402.h"
#include "sle_uart_slave_module.h"
#include "sle_uart_slave_zw101.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "app_init.h"
#include "common_def.h"
#include "pinctrl.h"
#include "securec.h"
#include "sle_errcode.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/*
 * 是否启用 PRINT 通道日志。该通道通常会携带 APP| 前缀，
 * 默认关闭以避免与 OSAL 日志形成重复打印。
 */
#ifndef MINE_LOG_PRINT_CHANNEL_ENABLE
#define MINE_LOG_PRINT_CHANNEL_ENABLE 0
#endif

/*
 * 是否启用 UART0 镜像日志。
 * 当 OSAL 日志本身已输出到串口时，建议关闭以避免重复打印。
 */
#ifndef MINE_LOG_UART0_MIRROR_ENABLE
#define MINE_LOG_UART0_MIRROR_ENABLE 0
#endif

/* UART2 接收日志最多展示字节数，避免长帧刷屏影响调试。 */
#define MINE_UART_RX_LOG_SHOW_MAX_BYTES 64
/* 文本日志缓冲区长度：每字节最多展开为2字符（如\n）+ 结尾符。 */
#define MINE_UART_RX_LOG_TEXT_MAX_LEN ((MINE_UART_RX_LOG_SHOW_MAX_BYTES * 2) + 1)
/* ZW101 二进制接收日志最小打印间隔（毫秒），用于抑制高频刷屏。 */
#define MINE_ZW101_BINARY_LOG_INTERVAL_MS 1000
/* LD2402 二进制接收日志最小打印间隔（毫秒），用于抑制高频刷屏。 */
#define MINE_LD2402_BINARY_LOG_INTERVAL_MS 1000

/* ZW101 状态上报最大文本长度（不含 SSAPC 侧自动追加的数据标签）。 */
#define MINE_ZW101_STATUS_FORWARD_TEXT_LEN 64

/* Camera 调试命令缓存长度。 */
#define MINE_CAMERA_DEBUG_LINE_MAX 64
/* Camera START 命令写回 UART2 的文本。 */
#define MINE_CAMERA_START_COLLECT_TEXT "start collect\r\n"

/* 多路 UART 接收缓冲区（按 UART0/1/2 索引）。 */
static uint8_t g_mine_uart_rx_buffer[MINE_UART_BUS_COUNT][MINE_UART_RX_BUFFER_SIZE] = {0};

/* UART 回调投递到任务消息队列。 */
static unsigned long g_mine_uart_msg_queue = 0;
static unsigned int g_mine_uart_msg_size = sizeof(mine_sle_uart_slave_msg_t);

#if MINE_ZW101_ENABLE
/* ZW101 二进制日志节流状态。 */
static uint32_t g_mine_zw101_binary_last_log_ms = 0;
static uint32_t g_mine_zw101_binary_suppressed = 0;
#endif

#if MINE_LD2402_ENABLE
/* LD2402 二进制日志节流状态。 */
static uint32_t g_mine_ld2402_binary_last_log_ms = 0;
static uint32_t g_mine_ld2402_binary_suppressed = 0;
#endif

/* 保留原 OSAL 日志出口，并镜像到 PRINT 通道。 */
static void (*g_mine_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/**
 * @brief 将日志同步镜像到 UART0，保证串口调试口持续可见。
 *
 * 采用“尽力发送”策略：不额外打印失败日志，避免日志回路递归。
 *
 * @param log_buf    日志缓冲区。
 * @param format_len 已格式化日志长度。
 */
#if MINE_LOG_UART0_MIRROR_ENABLE
static void mine_slave_log_mirror_uart0(const char *log_buf, int32_t format_len)
{
    if ((log_buf == NULL) || (format_len <= 0)) {
        return;
    }

    /* 保持 UART0 与系统日志同步输出，不因串口未就绪中断主流程。 */
    (void)uapi_uart_write(UART_BUS_0, (const uint8_t *)log_buf, (uint16_t)format_len, 0);
}
#endif

/**
 * @brief Slave 统一日志接口，主路输出到 OSAL，可选输出到 PRINT/UART0 镜像。
 *
 * 默认仅保留 OSAL 输出，避免 APP 前缀与镜像导致的重复日志。
 *
 * @param fmt printf 风格格式串。
 */
void mine_slave_log(const char *fmt, ...)
{
    char log_buf[MINE_LOG_BUFFER_LEN] = {0};
    va_list args;
    int32_t format_len;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    format_len = vsnprintf_s(log_buf, sizeof(log_buf), sizeof(log_buf) - 1, fmt, args);
    va_end(args);
    if (format_len <= 0) {
        return;
    }

    g_mine_raw_osal_printk("%s", log_buf);
#if MINE_LOG_PRINT_CHANNEL_ENABLE
    /* 如需保留 APP| 前缀通道，可显式打开该开关。 */
    PRINT("%s", log_buf);
#endif
#if MINE_LOG_UART0_MIRROR_ENABLE
    mine_slave_log_mirror_uart0(log_buf, format_len);
#endif
}

#define osal_printk mine_slave_log

/**
 * @brief 判断某 UART 总线是否在当前掩码下启用。
 *
 * @param bus UART 总线号。
 * @return true  总线启用。
 * @return false 总线未启用或越界。
 */
bool mine_slave_uart_bus_enabled(uart_bus_t bus)
{
    uint32_t bus_index = (uint32_t)bus;
    if (bus_index >= MINE_UART_BUS_COUNT) {
        return false;
    }
    return ((MINE_UART_ENABLE_MASK & (1U << bus_index)) != 0U);
}

/**
 * @brief 将 UART 总线号转换为字符串名称。
 *
 * @param bus UART 总线号。
 * @return const char* 可读总线名称。
 */
const char *mine_slave_uart_bus_name(uint8_t bus)
{
    if (bus == UART_BUS_0) {
        return "UART0";
    }
    if (bus == UART_BUS_1) {
        return "UART1";
    }
    if (bus == UART_BUS_2) {
        return "UART2";
    }
    return "UART?";
}

#if MINE_CAMERA_ENABLE
/**
 * @brief 将字符串原地转换为大写，便于命令匹配不区分大小写。
 *
 * @param text 待转换字符串。
 */
static void mine_camera_to_upper(char *text)
{
    uint16_t idx;

    if (text == NULL) {
        return;
    }

    for (idx = 0; text[idx] != '\0'; idx++) {
        text[idx] = (char)toupper((unsigned char)text[idx]);
    }
}

/**
 * @brief 从串口输入中提取一行可解析命令文本并做首尾空白裁剪。
 *
 * @param data    输入字节流。
 * @param len     输入长度。
 * @param out     输出命令文本。
 * @param out_len 输出缓存长度。
 * @return true  提取成功。
 * @return false 不是有效命令行。
 */
static bool mine_camera_extract_debug_line(const uint8_t *data, uint16_t len, char *out, uint16_t out_len)
{
    uint16_t idx;
    uint16_t pos = 0;
    uint16_t start = 0;
    uint16_t end;

    if ((data == NULL) || (len == 0U) || (out == NULL) || (out_len < 2U)) {
        return false;
    }

    for (idx = 0; idx < len; idx++) {
        uint8_t ch = data[idx];

        if ((ch == '\r') || (ch == '\n')) {
            continue;
        }

        if ((!isprint((int)ch)) && (ch != ' ') && (ch != '\t')) {
            return false;
        }

        if (pos >= (uint16_t)(out_len - 1U)) {
            return false;
        }

        out[pos++] = (char)ch;
    }
    out[pos] = '\0';

    if (pos == 0U) {
        return false;
    }

    while ((start < pos) && ((out[start] == ' ') || (out[start] == '\t'))) {
        start++;
    }
    if (start >= pos) {
        return false;
    }

    end = pos;
    while ((end > start) && ((out[end - 1U] == ' ') || (out[end - 1U] == '\t'))) {
        end--;
    }

    if (start > 0U) {
        (void)memmove_s(out, out_len, &out[start], (size_t)(end - start));
    }
    out[end - start] = '\0';
    return true;
}

/**
 * @brief 尝试处理 Camera 调试命令。
 *
 * 支持命令：
 * - CAM START：向 UART2 输出 "start collect"，启动人脸数据采集。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 输入字节流。
 * @param len  输入长度。
 * @return true  命令已消费，不再继续透传。
 * @return false 非 Camera 调试命令。
 */
static bool mine_camera_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
#if MINE_CAMERA_DEBUG_CMD_ENABLE
    char cmd_line[MINE_CAMERA_DEBUG_LINE_MAX] = {0};
    int32_t write_ret;

    if (bus != MINE_CAMERA_DEBUG_UART_BUS) {
        return false;
    }

    if (!mine_camera_extract_debug_line(data, len, cmd_line, sizeof(cmd_line))) {
        return false;
    }

    mine_camera_to_upper(cmd_line);
    if (strncmp(cmd_line, "CAM", 3U) != 0) {
        return false;
    }

    if (strcmp(cmd_line, "CAM START") == 0) {
        write_ret = uapi_uart_write(MINE_CAMERA_UART_BUS,
            (const uint8_t *)MINE_CAMERA_START_COLLECT_TEXT,
            (uint16_t)(sizeof(MINE_CAMERA_START_COLLECT_TEXT) - 1U), 0);
        if (write_ret < 0) {
            osal_printk("[mine cam] CAM START write uart2 failed, ret=%d\r\n", (int)write_ret);
            mine_slave_oled_push_state("CAM START FAIL");
        } else {
            osal_printk("[mine cam] CAM START -> uart2: %s", MINE_CAMERA_START_COLLECT_TEXT);
            mine_slave_oled_push_state("CAM START");
        }
        return true;
    }

    if (strcmp(cmd_line, "CAM HELP") == 0) {
        osal_printk("[mine cam] cmd help:\r\n");
        osal_printk("[mine cam]   CAM START\r\n");
        return true;
    }

    osal_printk("[mine cam] unknown cmd:%s\r\n", cmd_line);
    mine_slave_oled_push_state("CAM CMD ?");
    return true;
#else
    (void)bus;
    (void)data;
    (void)len;
    return false;
#endif
}
#endif

#if MINE_ZW101_ENABLE
/**
 * @brief 提取状态文本主体，去掉模块前缀（例如 "ZW101:"）。
 *
 * @param status_text 原始状态文本。
 * @return const char* 去前缀后的主体文本。
 */
static const char *mine_slave_status_body(const char *status_text)
{
    const char *body;

    if (status_text == NULL) {
        return NULL;
    }

    body = strchr(status_text, ':');
    if ((body != NULL) && (*(body + 1) != '\0')) {
        return (body + 1);
    }

    return status_text;
}

/**
 * @brief 将 ZW101 内部状态归一化为主机可读文本。
 *
 * 示例：
 * - ZW101:VERIFY   -> VERIFYING
 * - ZW101:ENR OK 1 -> ENROLL SUCCESS
 * - ZW101:ID1 S92  -> VERIFY SUCCESS ID1 S92
 *
 * @param raw_status ZW101 原始状态文本。
 * @param out_text   输出缓冲区。
 * @param out_len    输出缓冲区长度。
 * @return true  转换成功。
 * @return false 输入非法或转换失败。
 */
static bool mine_slave_build_zw101_forward_text(const char *raw_status, char *out_text, uint16_t out_len)
{
    const char *body;

    if ((raw_status == NULL) || (out_text == NULL) || (out_len == 0U)) {
        return false;
    }

    body = mine_slave_status_body(raw_status);
    if ((body == NULL) || (*body == '\0')) {
        return false;
    }

    if (strncmp(body, "ENR OK", 6U) == 0) {
        return (snprintf_s(out_text, out_len, out_len - 1, "ENROLL SUCCESS") > 0);
    }

    if ((strcmp(body, "VERIFY") == 0) || (strncmp(body, "VFY", 3U) == 0)) {
        return (snprintf_s(out_text, out_len, out_len - 1, "VERIFYING") > 0);
    }

    if ((strncmp(body, "ENR ", 4U) == 0) || (strncmp(body, "ENR REQ", 7U) == 0) || (strcmp(body, "ENR SEND") == 0)) {
        return (snprintf_s(out_text, out_len, out_len - 1, "ENROLLING") > 0);
    }

    if (strncmp(body, "ID", 2U) == 0) {
        return (snprintf_s(out_text, out_len, out_len - 1, "VERIFY SUCCESS %s", body) > 0);
    }

    if (strcmp(body, "NO MATCH") == 0) {
        return (snprintf_s(out_text, out_len, out_len - 1, "VERIFY FAIL NO MATCH") > 0);
    }

    return (snprintf_s(out_text, out_len, out_len - 1, "%s", body) > 0);
}

/**
 * @brief 将 ZW101 状态文本通过现有 UART2->SLE 通道上报给主机。
 *
 * 复用统一发送接口，标签由 SSAPC 上行层自动补齐为 [ZW101]。
 *
 * @param status_text ZW101 原始状态文本。
 */
static void mine_slave_forward_zw101_status_to_host(const char *status_text)
{
    mine_sle_uart_slave_msg_t msg = {0};
    char forward_text[MINE_ZW101_STATUS_FORWARD_TEXT_LEN] = {0};

    if (!mine_slave_build_zw101_forward_text(status_text, forward_text, sizeof(forward_text))) {
        return;
    }

    msg.uart_bus = (uint8_t)MINE_ZW101_UART_BUS;
    msg.value = (uint8_t *)forward_text;
    msg.value_len = (uint16_t)strlen(forward_text);
    if (msg.value_len == 0U) {
        return;
    }

    /* 连接未就绪时允许丢弃，避免阻塞主流程。 */
    (void)mine_sle_uart_slave_send_to_host(&msg);
}
#endif

/**
 * @brief 将 UART 接收字节转为可读文本并输出日志。
 *
 * 仅展示前 MINE_UART_RX_LOG_SHOW_MAX_BYTES 字节，避免超长帧刷屏；
 * 可打印 ASCII 直接输出，\r/\n/\t 使用转义字符显示，其余字节以 '.' 占位。
 *
 * @param bus            数据来源 UART 总线。
 * @param buffer         接收缓冲区。
 * @param length         接收字节长度。
 * @param peer_connected 当前 SLE 连接状态。
 */
static void mine_sle_uart_slave_dump_uart_rx(uart_bus_t bus, const uint8_t *buffer,
    uint16_t length, bool peer_connected)
{
    char log_text[MINE_UART_RX_LOG_TEXT_MAX_LEN] = {0};
    uint16_t show_len;
    uint16_t idx;
    uint16_t pos = 0;
    uint16_t printable_count = 0;
    uint32_t printable_ratio_pct;
    bool likely_binary = false;
    bool truncated = false;
    uint8_t ch;

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

    show_len = length;
    if (show_len > MINE_UART_RX_LOG_SHOW_MAX_BYTES) {
        show_len = MINE_UART_RX_LOG_SHOW_MAX_BYTES;
        truncated = true;
    }

    for (idx = 0; idx < show_len; idx++) {
        if ((sizeof(log_text) - pos) <= 1) {
            break;
        }

        ch = buffer[idx];
        if ((ch >= 0x20U) && (ch <= 0x7EU)) {
            /* 直接显示可打印 ASCII，便于观察纯文本协议内容。 */
            printable_count++;
            log_text[pos++] = (char)ch;
            continue;
        }

        if (((sizeof(log_text) - pos) > 2) && (ch == '\r')) {
            log_text[pos++] = '\\';
            log_text[pos++] = 'r';
            continue;
        }

        if (((sizeof(log_text) - pos) > 2) && (ch == '\n')) {
            log_text[pos++] = '\\';
            log_text[pos++] = 'n';
            continue;
        }

        if (((sizeof(log_text) - pos) > 2) && (ch == '\t')) {
            log_text[pos++] = '\\';
            log_text[pos++] = 't';
            continue;
        }

        /* 不可打印字节统一转为 '.'，避免日志污染终端控制字符。 */
        log_text[pos++] = '.';
    }

    log_text[pos] = '\0';

    /*
     * 通过可打印字符占比粗判“文本帧/二进制帧”：
     * - 长度较长且可打印比例偏低时，按二进制流处理并节流日志；
     * - 其余情况保持原有文本预览输出。
     */
    if (show_len > 0U) {
        printable_ratio_pct = ((uint32_t)printable_count * 100U) / (uint32_t)show_len;
        likely_binary = ((show_len >= 16U) && (printable_ratio_pct < 35U));
    }

    if (pos == 0) {
        if (peer_connected) {
            osal_printk("[mine slave] %s rx len:%u (dump failed)\r\n",
                mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length);
        } else {
            osal_printk("[mine slave] %s rx len:%u link:0 (dump failed)\r\n",
                mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length);
        }
        return;
    }

#if MINE_ZW101_ENABLE
    if ((bus == MINE_ZW101_UART_BUS) && likely_binary) {
        uint32_t now_ms = (uint32_t)uapi_systick_get_ms();

        if ((g_mine_zw101_binary_last_log_ms != 0U) &&
            ((uint32_t)(now_ms - g_mine_zw101_binary_last_log_ms) < MINE_ZW101_BINARY_LOG_INTERVAL_MS)) {
            g_mine_zw101_binary_suppressed++;
            return;
        }

        if (g_mine_zw101_binary_suppressed > 0U) {
            osal_printk("[mine slave] ZW101 binary rx suppressed:%lu\r\n",
                (unsigned long)g_mine_zw101_binary_suppressed);
            g_mine_zw101_binary_suppressed = 0;
        }

        g_mine_zw101_binary_last_log_ms = now_ms;
    }
#endif

#if MINE_LD2402_ENABLE
    if ((bus == MINE_LD2402_UART_BUS) && likely_binary) {
        uint32_t now_ms = (uint32_t)uapi_systick_get_ms();

        if ((g_mine_ld2402_binary_last_log_ms != 0U) &&
            ((uint32_t)(now_ms - g_mine_ld2402_binary_last_log_ms) < MINE_LD2402_BINARY_LOG_INTERVAL_MS)) {
            g_mine_ld2402_binary_suppressed++;
            return;
        }

        osal_printk("[mine slave] LD2402 binary rx len:%u ratio:%lu%% suppressed:%lu\r\n",
            (unsigned int)length,
            (unsigned long)printable_ratio_pct,
            (unsigned long)g_mine_ld2402_binary_suppressed);
        g_mine_ld2402_binary_suppressed = 0;
        g_mine_ld2402_binary_last_log_ms = now_ms;
        return;
    }
#endif

    if (truncated) {
        if (peer_connected) {
            osal_printk("[mine slave] %s rx len:%u show:%u data:%s ...\r\n",
                mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length,
                (unsigned int)show_len, log_text);
        } else {
            osal_printk("[mine slave] %s rx len:%u link:0 show:%u data:%s ...\r\n",
                mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length,
                (unsigned int)show_len, log_text);
        }
    } else {
        if (peer_connected) {
            osal_printk("[mine slave] %s rx len:%u data:%s\r\n",
                mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length, log_text);
        } else {
            osal_printk("[mine slave] %s rx len:%u link:0 data:%s\r\n",
                mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length, log_text);
        }
    }
}

/**
 * @brief 向所有已启用 UART 广播写入数据。
 *
 * @param data 待发送数据指针。
 * @param len  数据长度。
 */
void mine_slave_uart_write_enabled_buses(const uint8_t *data, uint16_t len)
{
    uint8_t bus_index;
    int32_t write_ret;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_slave_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }

        write_ret = uapi_uart_write((uart_bus_t)bus_index, data, len, 0);
        if (write_ret < 0) {
            osal_printk("[mine slave] %s write failed, ret=%d\r\n",
                mine_slave_uart_bus_name(bus_index), (int)write_ret);
        }
    }
}

/**
 * @brief 统一处理 UART 回调数据并投递到 Slave 任务消息队列。
 *
 * @param bus    数据来源 UART 总线。
 * @param buffer 接收缓冲区。
 * @param length 接收长度。
 * @param error  回调错误标志（当前未使用）。
 */
static void mine_sle_uart_slave_read_handler_common(uart_bus_t bus, const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_msg_t msg = {0};
    void *buffer_copy = NULL;
    bool peer_connected;
    int write_ret;

    unused(error);

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

    peer_connected = mine_sle_uart_slave_is_connected();

    /*
     * 所有 UART 接收都打印可读文本预览，便于直接核对数字/字母/符号内容；
     * 未连接时继续保留本地日志观测，不执行后续入队转发。
     */
    mine_sle_uart_slave_dump_uart_rx(bus, (const uint8_t *)buffer, length, peer_connected);

#if MINE_LD2402_ENABLE
    mine_ld2402_feed(bus, (const uint8_t *)buffer, length);
#if MINE_LD2402_DEBUG_CMD_ENABLE
    /* LD2402 调试命令由本地解析消费，避免继续透传到 Host。 */
    if (mine_ld2402_try_handle_debug_cmd(bus, (const uint8_t *)buffer, length)) {
        return;
    }
#endif
#endif
#if MINE_ZW101_ENABLE
    mine_zw101_feed(bus, (const uint8_t *)buffer, length);
#if MINE_ZW101_DEBUG_CMD_ENABLE
    /* 调试命令由本地解析消费，避免继续透传到 Host。 */
    if (mine_zw101_try_handle_debug_cmd(bus, (const uint8_t *)buffer, length)) {
        return;
    }
#endif

#if (MINE_ZW101_RAW_UPLINK_ENABLE == 0)
    /*
     * ZW101 模式默认不透传 UART2 原始二进制帧，避免主机出现乱码。
     * 可读状态文本由 mine_slave_forward_zw101_status_to_host 单独上报。
     */
    if (bus == MINE_ZW101_UART_BUS) {
        return;
    }
#endif
#endif

#if MINE_CAMERA_ENABLE
    /* Camera 命令由本地解析消费，避免把命令文本继续透传到 Host。 */
    if (mine_camera_try_handle_debug_cmd(bus, (const uint8_t *)buffer, length)) {
        return;
    }
#endif

    /* 未连接时不入队转发到 SLE，仅保留本地串口接收日志与模块处理。 */
    if (!peer_connected) {
        return;
    }

    buffer_copy = osal_vmalloc(length);
    if (buffer_copy == NULL) {
        return;
    }

    if (memcpy_s(buffer_copy, length, buffer, length) != EOK) {
        osal_vfree(buffer_copy);
        return;
    }

    msg.uart_bus = (uint8_t)bus;
    msg.value = (uint8_t *)buffer_copy;
    msg.value_len = length;

    write_ret = osal_msg_queue_write_copy(g_mine_uart_msg_queue, &msg, g_mine_uart_msg_size, 0);
    if (write_ret != OSAL_SUCCESS) {
        osal_vfree(buffer_copy);
    }
}

/**
 * @brief UART0 接收回调包装，转发到统一处理函数。
 */
static void mine_sle_uart_slave_read_handler_uart0(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_read_handler_common(UART_BUS_0, buffer, length, error);
}

/**
 * @brief UART1 接收回调包装，转发到统一处理函数。
 */
static void mine_sle_uart_slave_read_handler_uart1(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_read_handler_common(UART_BUS_1, buffer, length, error);
}

/**
 * @brief UART2 接收回调包装，转发到统一处理函数。
 */
static void mine_sle_uart_slave_read_handler_uart2(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_read_handler_common(UART_BUS_2, buffer, length, error);
}

/**
 * @brief 初始化单路 UART 并注册 RX 回调。
 *
 * @param bus 目标 UART 总线。
 * @return true  初始化成功。
 * @return false 初始化失败或参数不支持。
 */
static bool mine_sle_uart_slave_uart_init_one(uart_bus_t bus)
{
    uart_attr_t attr = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE,
    };
    uart_buffer_config_t uart_buffer_cfg = {0};
    uart_pin_config_t pin_cfg = {0};
    void (*rx_cb)(const void *, uint16_t, bool) = NULL;
    uint8_t bus_index = (uint8_t)bus;
    uint8_t pin_mode;
    errcode_t ret;

    if (bus == UART_BUS_0) {
        rx_cb = mine_sle_uart_slave_read_handler_uart0;
    } else if (bus == UART_BUS_1) {
        rx_cb = mine_sle_uart_slave_read_handler_uart1;
    } else if (bus == UART_BUS_2) {
        rx_cb = mine_sle_uart_slave_read_handler_uart2;
    }
    if (rx_cb == NULL) {
        return false;
    }

    pin_cfg.cts_pin = PIN_NONE;
    pin_cfg.rts_pin = PIN_NONE;

    if (bus == UART_BUS_0) {
        pin_cfg.tx_pin = MINE_UART0_TXD_PIN;
        pin_cfg.rx_pin = MINE_UART0_RXD_PIN;
        pin_mode = MINE_UART0_PIN_MODE;
    } else if (bus == UART_BUS_1) {
        pin_cfg.tx_pin = MINE_UART1_TXD_PIN;
        pin_cfg.rx_pin = MINE_UART1_RXD_PIN;
        pin_mode = MINE_UART1_PIN_MODE;
    } else {
        pin_cfg.tx_pin = MINE_UART2_TXD_PIN;
        pin_cfg.rx_pin = MINE_UART2_RXD_PIN;
        pin_mode = MINE_UART2_PIN_MODE;
    }

    if ((pin_cfg.tx_pin == PIN_NONE) || (pin_cfg.rx_pin == PIN_NONE)) {
        osal_printk("[mine slave] %s pin not configured, skip\r\n", mine_slave_uart_bus_name(bus_index));
        return false;
    }

#if MINE_ZW101_ENABLE
    /* 仅对 ZW101 所在 UART 总线使用专用波特率，其他总线保持默认值。 */
    if (bus == MINE_ZW101_UART_BUS) {
        attr.baud_rate = MINE_ZW101_UART_BAUD;
    }
#endif

    uart_buffer_cfg.rx_buffer = g_mine_uart_rx_buffer[bus_index];
    uart_buffer_cfg.rx_buffer_size = MINE_UART_RX_BUFFER_SIZE;

    uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
    uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

    (void)uapi_uart_deinit(bus);
    ret = uapi_uart_init(bus, &pin_cfg, &attr, NULL, &uart_buffer_cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] %s init failed, ret=%x\r\n", mine_slave_uart_bus_name(bus_index), ret);
        return false;
    }

    ret = uapi_uart_register_rx_callback(bus, UART_RX_CONDITION_MASK_IDLE, 1, rx_cb);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] %s rx cb failed, ret=%x\r\n", mine_slave_uart_bus_name(bus_index), ret);
        return false;
    }

    return true;
}

/**
 * @brief 按使能掩码初始化 Slave 侧 UART 通道。
 */
void mine_sle_uart_slave_uart_init(void)
{
    uint8_t bus_index;
    uint8_t enabled_count = 0;
    uint8_t ok_count = 0;

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_slave_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }
        enabled_count++;
        if (mine_sle_uart_slave_uart_init_one((uart_bus_t)bus_index)) {
            ok_count++;
        }
    }

    osal_printk("[mine slave] uart init summary, enabled:%u ok:%u\r\n", enabled_count, ok_count);
    if (ok_count > 0) {
        mine_slave_oled_push_state("UART INIT OK");
    } else {
        mine_slave_oled_push_state("UART INIT FAIL");
    }
}

/**
 * @brief Slave 主任务线程。
 *
 * 负责 OLED/UART/SLE 初始化、LD2402 状态更新以及
 * UART 消息队列消费并转发到 SLE。
 *
 * @param arg 任务入参（当前未使用）。
 * @return void* 任务退出返回值。
 */
static void *mine_sle_uart_slave_task(const char *arg)
{
    int read_ret;
    errcode_t send_ret;
#if MINE_LD2402_ENABLE
    /* 仅在雷达功能启用时保留状态缓冲区，避免未使用告警。 */
    char radar_status[24] = {0};
#endif
#if MINE_ZW101_ENABLE
    /* 仅在指纹功能启用时保留状态缓冲区，避免未使用告警。 */
    char zw101_status[24] = {0};
#endif

    unused(arg);
    osal_msleep(MINE_INIT_DELAY_MS);

    osal_printk("[mine slave] task start\r\n");
    mine_slave_oled_init();
    mine_sle_uart_slave_uart_init();
    /* 启动阶段先上报 UART2 角色，便于确认三选一互斥配置是否生效。 */
    osal_printk("[mine slave] uart2 mode:%s\r\n", MINE_UART2_MODE_NAME);
#if MINE_CAMERA_ENABLE
    mine_slave_oled_push_state("UART2 CAMERA");
#endif
#if MINE_LD2402_ENABLE
    if (mine_ld2402_init(MINE_LD2402_UART_BUS)) {
        mine_slave_oled_push_state("LD2402 READY");
    } else {
        mine_slave_oled_push_state("LD2402 WAIT");
    }
#endif
#if MINE_ZW101_ENABLE
    if (mine_zw101_init(MINE_ZW101_UART_BUS)) {
        mine_slave_oled_push_state("ZW101 READY");
#if MINE_ZW101_AUTO_ENROLL_ENABLE
        (void)mine_zw101_request_enroll(MINE_ZW101_AUTO_ENROLL_ID);
#elif MINE_ZW101_AUTO_VERIFY_ENABLE
        (void)mine_zw101_request_verify();
#endif
    } else {
        mine_slave_oled_push_state("ZW101 WAIT");
    }
#endif
    mine_slave_oled_push_state("SLE INIT...");
    if (mine_sle_uart_slave_init() != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] init failed\r\n");
        mine_slave_oled_push_state("INIT FAIL");
        return NULL;
    }

    while (1) {
        mine_sle_uart_slave_msg_t msg = {0};

        read_ret = osal_msg_queue_read_copy(g_mine_uart_msg_queue, &msg,
            &g_mine_uart_msg_size, MINE_TASK_LOOP_WAIT_MS);
#if MINE_LD2402_ENABLE
        mine_ld2402_process();
        if (mine_ld2402_get_status(radar_status, sizeof(radar_status))) {
            mine_slave_oled_push_state(radar_status);
        }
#endif
#if MINE_ZW101_ENABLE
        mine_zw101_process();
        if (mine_zw101_get_status(zw101_status, sizeof(zw101_status))) {
            mine_slave_oled_push_state(zw101_status);
            mine_slave_forward_zw101_status_to_host(zw101_status);
        }
#endif
        mine_slave_oled_flush_pending();
        if (read_ret != OSAL_SUCCESS) {
            continue;
        }

        if ((msg.value != NULL) && (msg.value_len > 0)) {
#if MINE_ZW101_ENABLE
            /* ZW101 二进制数据高频上报，队列日志默认抑制以降低刷屏。 */
            if (msg.uart_bus != MINE_ZW101_UART_BUS) {
                osal_printk("[mine slave] %s rx queue len:%u\r\n",
                    mine_slave_uart_bus_name(msg.uart_bus), msg.value_len);
            }
#else
            osal_printk("[mine slave] %s rx queue len:%u\r\n",
                mine_slave_uart_bus_name(msg.uart_bus), msg.value_len);
#endif
            send_ret = mine_sle_uart_slave_send_to_host(&msg);
            if (send_ret != ERRCODE_SLE_SUCCESS) {
                osal_printk("[mine slave] uart->sle send failed:%x\r\n", send_ret);
            }
            osal_vfree(msg.value);
        }
    }
}

/**
 * @brief Slave 应用入口。
 *
 * 负责创建 UART 消息队列和主任务线程。
 */
static void mine_sle_uart_slave_entry(void)
{
    osal_task *task_handle = NULL;
    int create_ret;

    osal_kthread_lock();

    create_ret = osal_msg_queue_create("mine_sle_slave_msg", (unsigned short)g_mine_uart_msg_size,
        &g_mine_uart_msg_queue, 0, g_mine_uart_msg_size);
    if (create_ret != OSAL_SUCCESS) {
        osal_printk("[mine slave] create queue failed:%x\r\n", create_ret);
        osal_kthread_unlock();
        return;
    }
    osal_printk("[mine slave] queue created\r\n");

    task_handle = osal_kthread_create((osal_kthread_handler)mine_sle_uart_slave_task,
        0, "mine_sle_slave", MINE_SLE_UART_SLAVE_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MINE_SLE_UART_SLAVE_TASK_PRIO);
        osal_kfree(task_handle);
        osal_printk("[mine slave] task created\r\n");
    } else {
        osal_printk("[mine slave] task create failed\r\n");
    }

    osal_kthread_unlock();
}

app_run(mine_sle_uart_slave_entry);
