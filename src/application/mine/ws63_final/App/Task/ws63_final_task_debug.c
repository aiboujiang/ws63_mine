/**
 * @file ws63_final_task_debug.c
 * @brief ws63_final 调试命令子模块实现。
 */

#include "ws63_final_task_debug.h"

#include "ws63_final_task.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "osal_debug.h"

#include "securec.h"

#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_debug_uart.h"
#include "ws63_motor.h"
#include "ws63_encoder.h"

#define WS63_DEBUG_LOG_BUF_MAX 192U
#define WS63_DEBUG_CMD_QUEUE_DEPTH 8U
#define WS63_DEBUG_RAW_CMD_MAX_BYTES 64U

#if (WS63_DEBUG_UART_ENABLE == 1U)
static uint8_t g_ws63_debug_uart_ready = 0U;
static uint8_t g_ws63_debug_watch_enable = 0U;
static uint32_t g_ws63_debug_last_watch_ms = 0U;
static uint8_t g_ws63_debug_uart_rx_buf[WS63_DEBUG_UART_RX_BUF_SIZE] = {0};
static char g_ws63_debug_cmd_line[WS63_DEBUG_CMD_MAX_LEN] = {0};
static uint16_t g_ws63_debug_cmd_line_len = 0U;
static char g_ws63_debug_cmd_queue[WS63_DEBUG_CMD_QUEUE_DEPTH][WS63_DEBUG_CMD_MAX_LEN] = {{0}};
static uint8_t g_ws63_debug_cmd_q_head = 0U;
static uint8_t g_ws63_debug_cmd_q_tail = 0U;
static uint8_t g_ws63_debug_cmd_q_count = 0U;
static uint8_t g_ws63_debug_cmd_overflow = 0U;
static uint8_t g_ws63_debug_cmd_too_long = 0U;
static uint8_t g_ws63_debug_uart_rx_error = 0U;
/* 记录上一字符是否为 '\\r'，用于兼容 CRLF，避免一条命令被执行两次。 */
static uint8_t g_ws63_debug_last_char_cr = 0U;

/**
 * @brief 把电机状态转换为可读字符串。
 */
static const char *ws63_motor_state_to_text(ws63_motor_state_t state)
{
    switch (state) {
        case WS63_MOTOR_STATE_FORWARD:
            return "FWD";
        case WS63_MOTOR_STATE_REVERSE:
            return "REV";
        case WS63_MOTOR_STATE_BRAKE:
        case WS63_MOTOR_STATE_COAST:
        default:
            return "STOP";
    }
}

/**
 * @brief 调试命令队列临界区加锁。
 */
static unsigned int ws63_debug_irq_lock(void)
{
    return ws63_os_irq_lock();
}

/**
 * @brief 调试命令队列临界区解锁。
 */
static void ws63_debug_irq_unlock(unsigned int irq_status)
{
    ws63_os_irq_unlock(irq_status);
}

/**
 * @brief 将完整命令行压入队列。
 */
static void ws63_debug_queue_push_line(const char *line)
{
    unsigned int irq_status;
    uint16_t i;
    uint8_t tail;

    if ((line == NULL) || (line[0] == '\0')) {
        return;
    }

    irq_status = ws63_debug_irq_lock();
    if (g_ws63_debug_cmd_q_count >= WS63_DEBUG_CMD_QUEUE_DEPTH) {
        g_ws63_debug_cmd_overflow = 1U;
        ws63_debug_irq_unlock(irq_status);
        return;
    }

    tail = g_ws63_debug_cmd_q_tail;
    for (i = 0U; (i < (WS63_DEBUG_CMD_MAX_LEN - 1U)) && (line[i] != '\0'); i++) {
        g_ws63_debug_cmd_queue[tail][i] = line[i];
    }
    g_ws63_debug_cmd_queue[tail][i] = '\0';

    g_ws63_debug_cmd_q_tail = (uint8_t)((tail + 1U) % WS63_DEBUG_CMD_QUEUE_DEPTH);
    g_ws63_debug_cmd_q_count++;
    ws63_debug_irq_unlock(irq_status);
}

/**
 * @brief 从命令队列弹出一条完整命令。
 */
static uint8_t ws63_debug_queue_pop_line(char *out, uint16_t out_size)
{
    unsigned int irq_status;
    uint16_t i;
    uint8_t head;

    if ((out == NULL) || (out_size < 2U)) {
        return 0U;
    }

    irq_status = ws63_debug_irq_lock();
    if (g_ws63_debug_cmd_q_count == 0U) {
        ws63_debug_irq_unlock(irq_status);
        return 0U;
    }

    head = g_ws63_debug_cmd_q_head;
    for (i = 0U; (i < (out_size - 1U)) && (g_ws63_debug_cmd_queue[head][i] != '\0'); i++) {
        out[i] = g_ws63_debug_cmd_queue[head][i];
    }
    out[i] = '\0';

    g_ws63_debug_cmd_q_head = (uint8_t)((head + 1U) % WS63_DEBUG_CMD_QUEUE_DEPTH);
    g_ws63_debug_cmd_q_count--;
    ws63_debug_irq_unlock(irq_status);
    return 1U;
}

/**
 * @brief 获取并清空异步接收告警标记。
 */
static void ws63_debug_take_async_flags(uint8_t *rx_error, uint8_t *too_long, uint8_t *overflow)
{
    unsigned int irq_status;

    if ((rx_error == NULL) || (too_long == NULL) || (overflow == NULL)) {
        return;
    }

    irq_status = ws63_debug_irq_lock();
    *rx_error = g_ws63_debug_uart_rx_error;
    *too_long = g_ws63_debug_cmd_too_long;
    *overflow = g_ws63_debug_cmd_overflow;
    g_ws63_debug_uart_rx_error = 0U;
    g_ws63_debug_cmd_too_long = 0U;
    g_ws63_debug_cmd_overflow = 0U;
    ws63_debug_irq_unlock(irq_status);
}

/**
 * @brief 根据电机状态统一 RPM 方向符号。
 */
static int32_t ws63_debug_normalize_rpm(ws63_motor_state_t state, int32_t rpm_raw)
{
    int32_t rpm_abs;

    rpm_abs = (rpm_raw >= 0) ? rpm_raw : -rpm_raw;
    if (state == WS63_MOTOR_STATE_FORWARD) {
        return rpm_abs;
    }
    if (state == WS63_MOTOR_STATE_REVERSE) {
        return -rpm_abs;
    }

    /* STOP/BRAKE 阶段保留编码器原始符号，便于观察电机惯性衰减方向。 */
    return rpm_raw;
}

/**
 * @brief 将电机轴 RPM 转换为输出轴 RPS 的千分值。
 */
static int32_t ws63_debug_motor_rpm_to_output_rps_milli(int32_t motor_rpm)
{
    int64_t numerator;
    int64_t denominator;

    if (WS63_MOTOR_GEAR_RATIO == 0U) {
        return 0;
    }

    numerator = (int64_t)motor_rpm * 1000LL;
    denominator = (int64_t)60 * (int64_t)WS63_MOTOR_GEAR_RATIO;
    if (denominator == 0) {
        return 0;
    }

    return (int32_t)(numerator / denominator);
}

/**
 * @brief 调试串口接收回调：按行组帧后压入命令队列。
 */
static void ws63_debug_uart_rx_callback(const void *buffer, uint16_t length, bool error)
{
    const uint8_t *rx_data;
    uint16_t i;
    uint8_t ch;

    if (error) {
        g_ws63_debug_uart_rx_error = 1U;
    }

    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    rx_data = (const uint8_t *)buffer;
    for (i = 0U; i < length; i++) {
        ch = rx_data[i];

        if (ch == '\r') {
            if (g_ws63_debug_cmd_line_len > 0U) {
                g_ws63_debug_cmd_line[g_ws63_debug_cmd_line_len] = '\0';
                ws63_debug_queue_push_line(g_ws63_debug_cmd_line);
                g_ws63_debug_cmd_line_len = 0U;
            }
            g_ws63_debug_last_char_cr = 1U;
            continue;
        }

        if (ch == '\n') {
            if (g_ws63_debug_last_char_cr == 1U) {
                g_ws63_debug_last_char_cr = 0U;
                continue;
            }

            if (g_ws63_debug_cmd_line_len > 0U) {
                g_ws63_debug_cmd_line[g_ws63_debug_cmd_line_len] = '\0';
                ws63_debug_queue_push_line(g_ws63_debug_cmd_line);
                g_ws63_debug_cmd_line_len = 0U;
            }
            continue;
        }

        g_ws63_debug_last_char_cr = 0U;

        if ((ch == 0x08U) || (ch == 0x7FU)) {
            if (g_ws63_debug_cmd_line_len > 0U) {
                g_ws63_debug_cmd_line_len--;
            }
            continue;
        }

        if (isprint((int)ch)) {
            if (g_ws63_debug_cmd_line_len < (WS63_DEBUG_CMD_MAX_LEN - 1U)) {
                g_ws63_debug_cmd_line[g_ws63_debug_cmd_line_len] = (char)ch;
                g_ws63_debug_cmd_line_len++;
            } else {
                g_ws63_debug_cmd_line_len = 0U;
                g_ws63_debug_cmd_too_long = 1U;
            }
        }
    }
}

/**
 * @brief 向调试串口发送文本。
 */
static void ws63_debug_uart_send_text(const char *text)
{
    size_t text_len;

    if ((g_ws63_debug_uart_ready == 0U) || (text == NULL)) {
        return;
    }

    text_len = strlen(text);
    if (text_len == 0U) {
        return;
    }

    (void)ws63_debug_uart_write((const uint8_t *)text, (uint16_t)text_len, 0U);
}

/**
 * @brief 输出调试日志。
 */
static void ws63_debug_log(const char *fmt, ...)
{
    int32_t ret;
    va_list args;
    char log_buf[WS63_DEBUG_LOG_BUF_MAX] = {0};

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    ret = vsnprintf_s(log_buf, sizeof(log_buf), sizeof(log_buf) - 1U, fmt, args);
    va_end(args);
    if (ret < 0) {
        return;
    }

#if (WS63_DEBUG_LOG_MIRROR_SYS == 1U)
    osal_printk("%s", log_buf);
#endif
    ws63_debug_uart_send_text(log_buf);
}

/**
 * @brief 去除命令首尾空白。
 */
static char *ws63_debug_trim(char *text)
{
    char *end;

    if (text == NULL) {
        return NULL;
    }

    while ((*text != '\0') && isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while ((end > text) && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return text;
}

/**
 * @brief 解析占空比参数。
 */
static uint8_t ws63_debug_parse_duty(const char *text, uint8_t *duty_out)
{
    char *end = NULL;
    unsigned long value;

    if ((text == NULL) || (duty_out == NULL)) {
        return 0U;
    }

    value = strtoul(text, &end, 10);
    if ((end == text) || (*ws63_debug_trim(end) != '\0') || (value > 100UL)) {
        return 0U;
    }

    *duty_out = (uint8_t)value;
    return 1U;
}

/**
 * @brief 解析 16 位整型参数并做范围校验。
 */
static uint8_t ws63_debug_parse_u16_range(const char *text,
    uint16_t min_value,
    uint16_t max_value,
    uint16_t *value_out)
{
    char *end = NULL;
    unsigned long value;

    if ((text == NULL) || (value_out == NULL) || (min_value > max_value)) {
        return 0U;
    }

    value = strtoul(text, &end, 10);
    if ((end == text) || (*ws63_debug_trim(end) != '\0')) {
        return 0U;
    }
    if ((value < (unsigned long)min_value) || (value > (unsigned long)max_value)) {
        return 0U;
    }

    *value_out = (uint16_t)value;
    return 1U;
}

/**
 * @brief 将以空格分隔的十进制参数串解析为整数数组。
 */
static uint8_t ws63_debug_parse_u32_tokens(const char *text,
    uint32_t *values,
    uint8_t max_values,
    uint8_t *count_out)
{
    char *end = NULL;
    const char *cursor;
    uint8_t count;
    unsigned long value;

    if ((text == NULL) || (values == NULL) || (count_out == NULL) || (max_values == 0U)) {
        return 0U;
    }

    cursor = text;
    count = 0U;

    while (*cursor != '\0') {
        while ((*cursor != '\0') && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        if (count >= max_values) {
            return 0U;
        }

        value = strtoul(cursor, &end, 10);
        if (end == cursor) {
            return 0U;
        }

        values[count] = (uint32_t)value;
        count++;
        cursor = end;
    }

    *count_out = count;
    return (count > 0U) ? 1U : 0U;
}

/**
 * @brief 将十六进制字符转换为数值。
 */
static uint8_t ws63_debug_hex_char_to_value(char ch, uint8_t *value_out)
{
    if (value_out == NULL) {
        return 0U;
    }

    if ((ch >= '0') && (ch <= '9')) {
        *value_out = (uint8_t)(ch - '0');
        return 1U;
    }
    if ((ch >= 'A') && (ch <= 'F')) {
        *value_out = (uint8_t)(ch - 'A' + 10);
        return 1U;
    }
    if ((ch >= 'a') && (ch <= 'f')) {
        *value_out = (uint8_t)(ch - 'a' + 10);
        return 1U;
    }

    return 0U;
}

/**
 * @brief 解析十六进制字节串（格式示例："FD FC 01 02"）。
 */
static uint8_t ws63_debug_parse_hex_bytes(const char *text,
    uint8_t *out_buf,
    uint16_t out_max,
    uint16_t *out_len)
{
    const char *cursor;
    uint16_t count;
    uint8_t high_nibble;
    uint8_t low_nibble;

    if ((text == NULL) || (out_buf == NULL) || (out_len == NULL) || (out_max == 0U)) {
        return 0U;
    }

    cursor = text;
    count = 0U;

    while (*cursor != '\0') {
        while ((*cursor != '\0') && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        if (count >= out_max) {
            return 0U;
        }

        if (!ws63_debug_hex_char_to_value(*cursor, &high_nibble)) {
            return 0U;
        }
        cursor++;

        if (!ws63_debug_hex_char_to_value(*cursor, &low_nibble)) {
            return 0U;
        }
        cursor++;

        out_buf[count] = (uint8_t)((high_nibble << 4) | low_nibble);
        count++;

        while ((*cursor != '\0') && isspace((unsigned char)*cursor)) {
            cursor++;
        }
    }

    if (count == 0U) {
        return 0U;
    }

    *out_len = count;
    return 1U;
}

/**
 * @brief 输出当前电机与编码器状态。
 */
static void ws63_debug_dump_motor_status(const char *tag)
{
    ws63_motor_state_t state;
    int32_t rpm_raw;
    int32_t rpm_show;
    int32_t out_rps_milli;
    int32_t out_rps_int;
    uint32_t out_rps_frac;
    const char *out_rps_sign;

    state = ws63_motor_get_state();
    rpm_raw = ws63_task_get_motor_rpm();
    rpm_show = ws63_debug_normalize_rpm(state, rpm_raw);
    out_rps_milli = ws63_debug_motor_rpm_to_output_rps_milli(rpm_show);

    out_rps_int = out_rps_milli / 1000;
    out_rps_frac = (uint32_t)((out_rps_milli >= 0) ? (out_rps_milli % 1000) : (-out_rps_milli % 1000));
    out_rps_sign = ((out_rps_milli < 0) && (out_rps_int == 0)) ? "-" : "";

    ws63_debug_log("[ws63 dbg] %s dir=%s motor_rpm=%ld out_rps=%s%ld.%03lu\\r\\n",
        (tag == NULL) ? "status" : tag,
        ws63_motor_state_to_text(state),
        (long)rpm_show,
        out_rps_sign,
        (long)out_rps_int,
        (unsigned long)out_rps_frac);
}

/**
 * @brief 输出当前蜂鸣器状态。
 */
static void ws63_debug_dump_beep_status(const char *tag)
{
#if (WS63_BEEP_ENABLE == 1U)
    ws63_debug_log("[ws63 dbg] %s beep=%s freq=%uHz vol=%u%%\r\n",
        (tag == NULL) ? "beep" : tag,
        (ws63_task_buzzer_is_on() == 1U) ? "ON" : "OFF",
        (unsigned int)ws63_task_buzzer_get_freq_hz(),
        (unsigned int)ws63_task_buzzer_get_volume());
#else
    ws63_debug_log("[ws63 dbg] %s beep=DISABLED\r\n",
        (tag == NULL) ? "beep" : tag);
#endif
}

/**
 * @brief 输出当前 RGB 状态。
 */
static void ws63_debug_dump_rgb_status(const char *tag)
{
#if (WS63_RGB_ENABLE == 1U)
    ws63_debug_log("[ws63 dbg] %s rgb=%s demo=%s\r\n",
        (tag == NULL) ? "rgb" : tag,
        (ws63_task_rgb_is_ready() == 1U) ? "READY" : "NOT_READY",
        (ws63_task_rgb_is_demo_enable() == 1U) ? "ON" : "OFF");
#else
    ws63_debug_log("[ws63 dbg] %s rgb=DISABLED\r\n",
        (tag == NULL) ? "rgb" : tag);
#endif
}

/**
 * @brief 打印调试命令帮助。
 */
static void ws63_debug_print_help(void)
{
    ws63_debug_log("[ws63 dbg] command list:\\r\\n");
    ws63_debug_log("[ws63 dbg]   HELP\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR FWD <0-100>\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR REV <0-100>\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR DUTY <0-100>\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR STOP\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR BRAKE\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR RPM\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR STAT\\r\\n");
    ws63_debug_log("[ws63 dbg]   MOTOR WATCH ON|OFF\\r\\n");
    ws63_debug_log("[ws63 dbg]   ENCODER RESET\\r\\n");
    ws63_debug_log("[ws63 dbg]   BEEP ON [100-5000]\\r\\n");
    ws63_debug_log("[ws63 dbg]   BEEP FREQ <100-5000>\\r\\n");
    ws63_debug_log("[ws63 dbg]   BEEP VOL <0-100>\\r\\n");
    ws63_debug_log("[ws63 dbg]   BEEP OFF\\r\\n");
    ws63_debug_log("[ws63 dbg]   BEEP STAT\\r\\n");
    ws63_debug_log("[ws63 dbg]   RGB INIT\\r\\n");
    ws63_debug_log("[ws63 dbg]   RGB SET <R0-255> <G0-255> <B0-255>\\r\\n");
    ws63_debug_log("[ws63 dbg]   RGB OFF\\r\\n");
    ws63_debug_log("[ws63 dbg]   RGB DEMO ON|OFF\\r\\n");
    ws63_debug_log("[ws63 dbg]   RGB STAT\\r\\n");
    ws63_debug_log("[ws63 dbg]   LD2401 INIT (alias LD2402)\\r\\n");
    ws63_debug_log("[ws63 dbg]   LD2401 RAW <HEX...>\\r\\n");
    ws63_debug_log("[ws63 dbg]   LD2401 STAT\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 INIT|HANDSHAKE\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 RAW <HEX...>\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 STAT\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA HELP\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA ECHO\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCH <wait> <start> <count>\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHRES <buf1|2> <start> <count>\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHECHO <wait> <start> <count>\\r\\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA TERM\\r\\n");
}

/**
 * @brief 执行单条调试命令。
 */
static void ws63_debug_exec_command(const char *line)
{
    uint16_t i;
    uint16_t freq_hz;
    uint16_t raw_len;
    uint16_t za_page_id;
    uint16_t za_score;
    uint8_t za_ack;
    uint8_t za_argc;
    uint8_t rgb_argc;
    uint8_t duty;
    uint32_t za_args[6] = {0};
    uint32_t rgb_args[4] = {0};
    uint8_t raw_buf[WS63_DEBUG_RAW_CMD_MAX_BYTES] = {0};
    errcode_t ret;
    char cmd_buf[WS63_DEBUG_CMD_MAX_LEN] = {0};
    char *cmd;

    if (line == NULL) {
        return;
    }

    if (strncpy_s(cmd_buf, sizeof(cmd_buf), line, sizeof(cmd_buf) - 1U) != EOK) {
        return;
    }

    cmd = ws63_debug_trim(cmd_buf);
    for (i = 0U; cmd[i] != '\0'; i++) {
        cmd[i] = (char)toupper((unsigned char)cmd[i]);
    }

    if (cmd[0] == '\0') {
        return;
    }

    ws63_debug_log("[ws63 dbg] cmd<=%s\\r\\n", cmd);

    if ((strcmp(cmd, "HELP") == 0) || (strcmp(cmd, "?") == 0)) {
        ws63_debug_print_help();
        return;
    }

    if (strcmp(cmd, "MOTOR STOP") == 0) {
        ret = ws63_task_motor_coast_stop();
        ws63_debug_log("[ws63 dbg] MOTOR STOP ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_motor_status("stop");
        return;
    }

    if (strcmp(cmd, "MOTOR BRAKE") == 0) {
        ret = ws63_task_motor_brake_stop();
        ws63_debug_log("[ws63 dbg] MOTOR BRAKE ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_motor_status("brake");
        return;
    }

    if ((strcmp(cmd, "MOTOR RPM") == 0) || (strcmp(cmd, "MOTOR STAT") == 0)) {
        ws63_debug_dump_motor_status("query");
        return;
    }

    if (strcmp(cmd, "MOTOR WATCH ON") == 0) {
        g_ws63_debug_watch_enable = 1U;
        g_ws63_debug_last_watch_ms = 0U;
        ws63_debug_log("[ws63 dbg] watch enabled\\r\\n");
        return;
    }

    if (strcmp(cmd, "MOTOR WATCH OFF") == 0) {
        g_ws63_debug_watch_enable = 0U;
        ws63_debug_log("[ws63 dbg] watch disabled\\r\\n");
        return;
    }

    if (strcmp(cmd, "ENCODER RESET") == 0) {
        ws63_encoder_reset();
        ws63_debug_log("[ws63 dbg] encoder counter reset\\r\\n");
        ws63_debug_dump_motor_status("encoder-reset");
        return;
    }

    if (strcmp(cmd, "BEEP STAT") == 0) {
        ws63_debug_dump_beep_status("beep-query");
        return;
    }

    if (strcmp(cmd, "BEEP OFF") == 0) {
        ret = ws63_task_buzzer_off();
        ws63_debug_log("[ws63 dbg] BEEP OFF ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-off");
        return;
    }

    if (strcmp(cmd, "BEEP ON") == 0) {
        ret = ws63_task_buzzer_on(WS63_BEEP_DEFAULT_FREQ_HZ);
        ws63_debug_log("[ws63 dbg] BEEP ON %uHz ret=0x%x\\r\\n",
            (unsigned int)WS63_BEEP_DEFAULT_FREQ_HZ,
            (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-on");
        return;
    }

    if (strncmp(cmd, "BEEP ON ", 8) == 0) {
        if (!ws63_debug_parse_u16_range(cmd + 8,
            WS63_BEEP_MIN_FREQ_HZ,
            WS63_BEEP_MAX_FREQ_HZ,
            &freq_hz)) {
            ws63_debug_log("[ws63 dbg] invalid freq, expect %u~%u\\r\\n",
                (unsigned int)WS63_BEEP_MIN_FREQ_HZ,
                (unsigned int)WS63_BEEP_MAX_FREQ_HZ);
            return;
        }

        ret = ws63_task_buzzer_on(freq_hz);
        ws63_debug_log("[ws63 dbg] BEEP ON %uHz ret=0x%x\\r\\n",
            (unsigned int)freq_hz,
            (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-on");
        return;
    }

    if (strncmp(cmd, "BEEP FREQ ", 10) == 0) {
        if (!ws63_debug_parse_u16_range(cmd + 10,
            WS63_BEEP_MIN_FREQ_HZ,
            WS63_BEEP_MAX_FREQ_HZ,
            &freq_hz)) {
            ws63_debug_log("[ws63 dbg] invalid freq, expect %u~%u\\r\\n",
                (unsigned int)WS63_BEEP_MIN_FREQ_HZ,
                (unsigned int)WS63_BEEP_MAX_FREQ_HZ);
            return;
        }

        ret = ws63_task_buzzer_on(freq_hz);
        ws63_debug_log("[ws63 dbg] BEEP FREQ %uHz ret=0x%x\\r\\n",
            (unsigned int)freq_hz,
            (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-freq");
        return;
    }

    if (strncmp(cmd, "BEEP VOL ", 9) == 0) {
        if (!ws63_debug_parse_duty(cmd + 9, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid volume, expect 0~100\\r\\n");
            return;
        }

        ret = ws63_task_buzzer_set_volume(duty);
        ws63_debug_log("[ws63 dbg] BEEP VOL %u%% ret=0x%x\\r\\n",
            (unsigned int)duty,
            (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-vol");
        return;
    }

    if (strcmp(cmd, "RGB STAT") == 0) {
        ws63_debug_dump_rgb_status("rgb-query");
        return;
    }

    if (strcmp(cmd, "RGB INIT") == 0) {
        ret = ws63_task_rgb_reinit();
        ws63_debug_log("[ws63 dbg] RGB INIT ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-init");
        return;
    }

    if (strcmp(cmd, "RGB OFF") == 0) {
        ret = ws63_task_rgb_off();
        ws63_debug_log("[ws63 dbg] RGB OFF ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-off");
        return;
    }

    if (strcmp(cmd, "RGB DEMO ON") == 0) {
        ret = ws63_task_rgb_set_demo_enable(1U);
        ws63_debug_log("[ws63 dbg] RGB DEMO ON ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-demo");
        return;
    }

    if (strcmp(cmd, "RGB DEMO OFF") == 0) {
        ret = ws63_task_rgb_set_demo_enable(0U);
        ws63_debug_log("[ws63 dbg] RGB DEMO OFF ret=0x%x\\r\\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-demo");
        return;
    }

    if (strncmp(cmd, "RGB SET ", 8) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 8, rgb_args, 4U, &rgb_argc) || (rgb_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: RGB SET <R0-255> <G0-255> <B0-255>\\r\\n");
            return;
        }
        if ((rgb_args[0] > 255U) || (rgb_args[1] > 255U) || (rgb_args[2] > 255U)) {
            ws63_debug_log("[ws63 dbg] invalid rgb range, expect 0~255\\r\\n");
            return;
        }

        ret = ws63_task_rgb_set_color((uint8_t)rgb_args[0], (uint8_t)rgb_args[1], (uint8_t)rgb_args[2]);
        ws63_debug_log("[ws63 dbg] RGB SET (%u,%u,%u) ret=0x%x\\r\\n",
            (unsigned int)rgb_args[0],
            (unsigned int)rgb_args[1],
            (unsigned int)rgb_args[2],
            (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-set");
        return;
    }

    if ((strcmp(cmd, "LD2401 INIT") == 0) || (strcmp(cmd, "LD2402 INIT") == 0)) {
        ret = ws63_task_ld2402_reinit();
        ws63_debug_log("[ws63 dbg] LD2401 INIT ret=0x%x\\r\\n", (unsigned int)ret);
        return;
    }

    if ((strcmp(cmd, "LD2401 STAT") == 0) || (strcmp(cmd, "LD2402 STAT") == 0)) {
        ws63_debug_log("[ws63 dbg] LD2401(alias LD2402) subport=%u\\r\\n", (unsigned int)LD2402_SUBPORT);
        return;
    }

    if ((strncmp(cmd, "LD2401 RAW ", 10) == 0) || (strncmp(cmd, "LD2402 RAW ", 10) == 0)) {
        if (!ws63_debug_parse_hex_bytes(cmd + 10, raw_buf, sizeof(raw_buf), &raw_len)) {
            ws63_debug_log("[ws63 dbg] invalid LD2401 raw hex, example: FD FC FB FA\\r\\n");
            return;
        }

        ret = ws63_task_ld2402_send_raw(raw_buf, raw_len);
        ws63_debug_log("[ws63 dbg] LD2401 RAW len=%u ret=0x%x\\r\\n",
            (unsigned int)raw_len,
            (unsigned int)ret);
        return;
    }

    if ((strcmp(cmd, "ZW101 INIT") == 0) || (strcmp(cmd, "ZW101 HANDSHAKE") == 0)) {
        ret = ws63_task_zw101_reinit();
        ws63_debug_log("[ws63 dbg] ZW101 INIT ret=0x%x\\r\\n", (unsigned int)ret);
        return;
    }

    if (strcmp(cmd, "ZW101 ZA HELP") == 0) {
        ws63_debug_log("[ws63 dbg] ZA commands:\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA ECHO\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCH <wait> <start> <count>\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHRES <buf1|2> <start> <count>\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHECHO <wait> <start> <count>\\r\\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA TERM\\r\\n");
        return;
    }

    if (strcmp(cmd, "ZW101 ZA ECHO") == 0) {
        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_get_echo(&za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA ECHO ret=0x%x ack=0x%02x\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA LOGIN ", 15) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 15, za_args, 6U, &za_argc) || (za_argc != 5U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>\\r\\n");
            return;
        }
        if ((za_args[1] > 15U) || ((za_args[2] != 2U) && (za_args[2] != 3U)) ||
            (za_args[3] > 65535U) || (za_args[4] > 1U)) {
            ws63_debug_log("[ws63 dbg] invalid LOGIN args\\r\\n");
            return;
        }

        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_auto_login((uint8_t)za_args[0],
            (uint8_t)za_args[1],
            (uint8_t)za_args[2],
            (uint16_t)za_args[3],
            (uint8_t)za_args[4],
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA LOGIN ret=0x%x ack=0x%02x\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA SEARCH ", 16) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 16, za_args, 6U, &za_argc) || (za_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA SEARCH <wait> <start> <count>\\r\\n");
            return;
        }
        if ((za_args[1] > 65535U) || (za_args[2] > 65535U)) {
            ws63_debug_log("[ws63 dbg] invalid SEARCH args\\r\\n");
            return;
        }

        za_ack = 0xFFU;
        za_page_id = 0U;
        za_score = 0U;
        ret = ws63_task_zw101_za_auto_search((uint8_t)za_args[0],
            (uint16_t)za_args[1],
            (uint16_t)za_args[2],
            &za_page_id,
            &za_score,
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA SEARCH ret=0x%x ack=0x%02x id=%u score=%u\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack,
            (unsigned int)za_page_id,
            (unsigned int)za_score);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA SEARCHRES ", 19) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 19, za_args, 6U, &za_argc) || (za_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA SEARCHRES <buf1|2> <start> <count>\\r\\n");
            return;
        }
        if (((za_args[0] != 1U) && (za_args[0] != 2U)) || (za_args[1] > 65535U) || (za_args[2] > 65535U)) {
            ws63_debug_log("[ws63 dbg] invalid SEARCHRES args\\r\\n");
            return;
        }

        za_ack = 0xFFU;
        za_page_id = 0U;
        za_score = 0U;
        ret = ws63_task_zw101_za_search_res_back((uint8_t)za_args[0],
            (uint16_t)za_args[1],
            (uint16_t)za_args[2],
            &za_page_id,
            &za_score,
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA SEARCHRES ret=0x%x ack=0x%02x id=%u score=%u\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack,
            (unsigned int)za_page_id,
            (unsigned int)za_score);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA LOGINLIGHT ", 20) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 20, za_args, 6U, &za_argc) || (za_argc != 4U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>\\r\\n");
            return;
        }
        if (((za_args[1] != 2U) && (za_args[1] != 3U)) || (za_args[2] > 65535U) || (za_args[3] > 1U)) {
            ws63_debug_log("[ws63 dbg] invalid LOGINLIGHT args\\r\\n");
            return;
        }

        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_auto_login_stab((uint8_t)za_args[0],
            (uint8_t)za_args[1],
            (uint16_t)za_args[2],
            (uint8_t)za_args[3],
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA LOGINLIGHT ret=0x%x ack=0x%02x\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA SEARCHECHO ", 20) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 20, za_args, 6U, &za_argc) || (za_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA SEARCHECHO <wait> <start> <count>\\r\\n");
            return;
        }
        if ((za_args[1] > 65535U) || (za_args[2] > 65535U)) {
            ws63_debug_log("[ws63 dbg] invalid SEARCHECHO args\\r\\n");
            return;
        }

        za_ack = 0xFFU;
        za_page_id = 0U;
        za_score = 0U;
        ret = ws63_task_zw101_za_auto_search_echo((uint8_t)za_args[0],
            (uint16_t)za_args[1],
            (uint16_t)za_args[2],
            &za_page_id,
            &za_score,
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA SEARCHECHO ret=0x%x ack=0x%02x id=%u score=%u\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack,
            (unsigned int)za_page_id,
            (unsigned int)za_score);
        return;
    }

    if (strcmp(cmd, "ZW101 ZA TERM") == 0) {
        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_terminate(&za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA TERM ret=0x%x ack=0x%02x\\r\\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strcmp(cmd, "ZW101 STAT") == 0) {
        ws63_debug_log("[ws63 dbg] ZW101 subport=%u\\r\\n", (unsigned int)ZW101_SUBPORT);
        return;
    }

    if (strncmp(cmd, "ZW101 RAW ", 10) == 0) {
        if (!ws63_debug_parse_hex_bytes(cmd + 10, raw_buf, sizeof(raw_buf), &raw_len)) {
            ws63_debug_log("[ws63 dbg] invalid ZW101 raw hex, example: EF 01 FF FF\\r\\n");
            return;
        }

        ret = ws63_task_zw101_send_raw(raw_buf, raw_len);
        ws63_debug_log("[ws63 dbg] ZW101 RAW len=%u ret=0x%x\\r\\n",
            (unsigned int)raw_len,
            (unsigned int)ret);
        return;
    }

    if (strncmp(cmd, "MOTOR FWD ", 10) == 0) {
        if (!ws63_debug_parse_duty(cmd + 10, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid duty, expect 0~100\\r\\n");
            return;
        }
        ret = ws63_task_motor_forward(duty);
        ws63_debug_log("[ws63 dbg] MOTOR FWD %u ret=0x%x\\r\\n", (unsigned int)duty, (unsigned int)ret);
        ws63_debug_dump_motor_status("forward");
        return;
    }

    if (strncmp(cmd, "MOTOR REV ", 10) == 0) {
        if (!ws63_debug_parse_duty(cmd + 10, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid duty, expect 0~100\\r\\n");
            return;
        }
        ret = ws63_task_motor_reverse(duty);
        ws63_debug_log("[ws63 dbg] MOTOR REV %u ret=0x%x\\r\\n", (unsigned int)duty, (unsigned int)ret);
        ws63_debug_dump_motor_status("reverse");
        return;
    }

    if (strncmp(cmd, "MOTOR DUTY ", 11) == 0) {
        if (!ws63_debug_parse_duty(cmd + 11, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid duty, expect 0~100\\r\\n");
            return;
        }
        ret = ws63_task_motor_set_duty(duty);
        ws63_debug_log("[ws63 dbg] MOTOR DUTY %u ret=0x%x\\r\\n", (unsigned int)duty, (unsigned int)ret);
        ws63_debug_dump_motor_status("duty");
        return;
    }

    ws63_debug_log("[ws63 dbg] unknown command: %s\\r\\n", cmd);
    ws63_debug_log("[ws63 dbg] type HELP for command list\\r\\n");
}

/**
 * @brief 初始化调试串口命令能力。
 */
static void ws63_debug_uart_cmd_init(void)
{
    errcode_t ret;

    g_ws63_debug_uart_ready = 0U;
    g_ws63_debug_watch_enable = 0U;
    g_ws63_debug_cmd_line_len = 0U;
    g_ws63_debug_last_watch_ms = 0U;
    g_ws63_debug_cmd_q_head = 0U;
    g_ws63_debug_cmd_q_tail = 0U;
    g_ws63_debug_cmd_q_count = 0U;
    g_ws63_debug_cmd_overflow = 0U;
    g_ws63_debug_cmd_too_long = 0U;
    g_ws63_debug_uart_rx_error = 0U;
    g_ws63_debug_last_char_cr = 0U;

    ret = ws63_debug_uart_init(g_ws63_debug_uart_rx_buf, sizeof(g_ws63_debug_uart_rx_buf));
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] debug uart init fail, ret=0x%x\\r\\n", (unsigned int)ret);
        return;
    }

    ret = ws63_debug_uart_register_rx_callback(ws63_debug_uart_rx_callback, 1U);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] debug uart cb reg fail, ret=0x%x\\r\\n", (unsigned int)ret);
        return;
    }

    g_ws63_debug_uart_ready = 1U;
    ws63_debug_log("[ws63 dbg] uart ready bus=%u baud=%u tx=%u rx=%u\\r\\n",
        (unsigned int)WS63_DEBUG_UART_BUS,
        (unsigned int)WS63_DEBUG_UART_BAUD,
        (unsigned int)WS63_DEBUG_UART_TX_PIN,
        (unsigned int)WS63_DEBUG_UART_RX_PIN);
    ws63_debug_print_help();
}

/**
 * @brief 轮询调试串口并处理命令。
 */
static void ws63_debug_uart_cmd_process(uint32_t now_ms)
{
    uint8_t rx_error;
    uint8_t too_long;
    uint8_t overflow;
    char cmd_line[WS63_DEBUG_CMD_MAX_LEN] = {0};

    if (g_ws63_debug_uart_ready == 0U) {
        return;
    }

    ws63_debug_take_async_flags(&rx_error, &too_long, &overflow);
    if (rx_error == 1U) {
        ws63_debug_log("[ws63 dbg] uart rx error\\r\\n");
    }
    if (too_long == 1U) {
        ws63_debug_log("[ws63 dbg] command too long, dropped\\r\\n");
    }
    if (overflow == 1U) {
        ws63_debug_log("[ws63 dbg] command queue overflow, dropped\\r\\n");
    }

    while (ws63_debug_queue_pop_line(cmd_line, sizeof(cmd_line)) == 1U) {
        ws63_debug_exec_command(cmd_line);
    }

    if (g_ws63_debug_watch_enable == 0U) {
        return;
    }

    if ((now_ms - g_ws63_debug_last_watch_ms) < WS63_DEBUG_WATCH_PERIOD_MS) {
        return;
    }

    g_ws63_debug_last_watch_ms = now_ms;
    ws63_debug_dump_motor_status("watch");
}
#endif

void ws63_task_debug_init(void)
{
#if (WS63_DEBUG_UART_ENABLE == 1U)
    ws63_debug_uart_cmd_init();
#endif
}

void ws63_task_debug_process(uint32_t now_ms)
{
#if (WS63_DEBUG_UART_ENABLE == 1U)
    ws63_debug_uart_cmd_process(now_ms);
#else
    (void)now_ms;
#endif
}
