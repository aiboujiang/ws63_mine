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
#define WS63_DEBUG_SENSOR_LOG_GAP_MS_MAX 60000U

#if (WS63_DEBUG_UART_ENABLE == 1U)
static uint8_t g_ws63_debug_uart_ready = 0U;
/* 电机周期监控开关：仅控制 MOTOR WATCH ON|OFF。 */
static uint8_t g_ws63_debug_motor_watch_enable = 0U;
/* TTP229 周期监控开关：用于持续观察矩阵键盘按键位图变化。 */
static uint8_t g_ws63_debug_ttp229_watch_enable = 0U;
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
 * @brief 从命令行中提取一个 token（空格/制表符分隔）。
 */
static char *ws63_debug_next_token(char **cursor)
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
 * @brief 解析一个无符号整数字符串（支持十进制与 0x 前缀）。
 */
static uint8_t ws63_debug_parse_u32_value(const char *text, uint32_t *value_out)
{
    char *end = NULL;
    unsigned long value;

    if ((text == NULL) || (value_out == NULL)) {
        return 0U;
    }

    value = strtoul(text, &end, 0);
    if ((end == text) || (*ws63_debug_trim(end) != '\0')) {
        return 0U;
    }

    *value_out = (uint32_t)value;
    return 1U;
}

/**
 * @brief 解析距离门限参数（单位 0.1m）。
 */
static uint8_t ws63_debug_parse_distance_01m(const char *text, uint16_t *distance_01m_out)
{
    char *end = NULL;
    double value;

    if ((text == NULL) || (distance_01m_out == NULL)) {
        return 0U;
    }

    value = strtod(text, &end);
    if ((end == text) || (*ws63_debug_trim(end) != '\0')) {
        return 0U;
    }
    if ((value < 0.7) || (value > 10.0)) {
        return 0U;
    }

    *distance_01m_out = (uint16_t)(value * 10.0 + 0.5);
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

    ws63_debug_log("[ws63 dbg] %s dir=%s motor_rpm=%ld out_rps=%s%ld.%03lu\r\n",
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
 * @brief 输出当前 TTP229 状态。
 */
static void ws63_debug_dump_ttp229_status(const char *tag)
{
#if (WS63_TTP229_ENABLE == 1U)
    char key_text[64] = {0};

    if (ws63_task_ttp229_get_pressed_text(key_text, sizeof(key_text)) != ERRCODE_SUCC) {
        (void)strncpy_s(key_text, sizeof(key_text), "ERR", 3U);
    }

    ws63_debug_log("[ws63 dbg] %s ttp229=%s enable=%s alarm=%s active=%s raw=0x%04x mask=0x%04x count=%u keys=%s\r\n",
        (tag == NULL) ? "ttp229" : tag,
        (ws63_task_ttp229_is_ready() == 1U) ? "READY" : "NOT_READY",
        (ws63_task_ttp229_is_enabled() == 1U) ? "ON" : "OFF",
        (ws63_task_ttp229_is_multi_key_alarm_enable() == 1U) ? "ON" : "OFF",
        (ws63_task_ttp229_is_multi_key_active() == 1U) ? "ON" : "OFF",
        (unsigned int)ws63_task_ttp229_get_raw_code(),
        (unsigned int)ws63_task_ttp229_get_pressed_mask(),
        (unsigned int)ws63_task_ttp229_get_pressed_count(),
        key_text);
#else
    ws63_debug_log("[ws63 dbg] %s ttp229=DISABLED\r\n",
        (tag == NULL) ? "ttp229" : tag);
#endif
}

/**
 * @brief 输出当前 LD2402 距离状态。
 */
static void ws63_debug_dump_ld2402_distance_status(const char *tag)
{
    int32_t distance_mm;
    uint32_t tick_ms;
    uint32_t age_ms;
    uint32_t now_ms;

    distance_mm = ws63_task_ld2402_get_distance_mm();
    tick_ms = ws63_task_ld2402_get_distance_tick_ms();
    now_ms = ws63_os_tick_ms();
    age_ms = (tick_ms == 0U) ? 0U : (uint32_t)(now_ms - tick_ms);

    ws63_debug_log("[ws63 dbg] %s ld2402_distance=%ld tick=%u age=%ums\r\n",
        (tag == NULL) ? "ld2402-distance" : tag,
        (long)distance_mm,
        (unsigned int)tick_ms,
        (unsigned int)age_ms);
}

/**
 * @brief 输出当前 LD2402 协议状态。
 */
static void ws63_debug_dump_ld2402_runtime_status(const char *tag)
{
    ws63_debug_log("[ws63 dbg] %s ld2402=ready:%u cfg:%u log:%s gap=%ums\r\n",
        (tag == NULL) ? "ld2402-status" : tag,
        (unsigned int)ws63_task_ld2402_is_ready(),
        (unsigned int)ws63_task_ld2402_is_in_config_mode(),
        (ws63_task_ld2402_get_log_enable() == 1U) ? "ON" : "OFF",
        (unsigned int)ws63_task_ld2402_get_log_gap_ms());
}

    /**
     * @brief 打印调试命令帮助。
     */
    static void ws63_debug_print_help(void);

/**
 * @brief 执行 LD 前缀调试命令。
 */
static void ws63_debug_exec_ld_command(const char *cmd)
{
    char ld_buf[WS63_DEBUG_CMD_MAX_LEN] = {0};
    char *cursor;
    char *op;
    char *arg0;
    char *arg1;
    char *arg2;
    uint16_t distance_01m = 0U;
    uint16_t progress = 0U;
    uint16_t alarm_status = 0U;
    uint16_t gate_bitmap = 0U;
    uint16_t param_id = 0U;
    uint16_t param_value_16 = 0U;
    uint32_t param_value = 0U;
    uint32_t power_inter = 0U;
    uint8_t raw_buf[WS63_DEBUG_RAW_CMD_MAX_BYTES] = {0};
    uint16_t raw_len = 0U;
    uint8_t sn_buf[32] = {0};
    char sn_text[64] = {0};
    char version[32] = {0};
    int32_t sn_hex_len;
    errcode_t ret;

    if (cmd == NULL) {
        return;
    }

    if (strncpy_s(ld_buf, sizeof(ld_buf), cmd, sizeof(ld_buf) - 1U) != EOK) {
        return;
    }

    cursor = ld_buf;
    op = ws63_debug_next_token(&cursor);
    if (op == NULL) {
        return;
    }

    if (strcmp(op, "LD") == 0) {
        op = ws63_debug_next_token(&cursor);
        if (op == NULL) {
            ws63_debug_print_help();
            return;
        }
    } else {
        return;
    }

    if (strcmp(op, "HELP") == 0) {
        ws63_debug_print_help();
        return;
    }

    if (strcmp(op, "INIT") == 0) {
        ret = ws63_task_ld2402_reinit();
        ws63_debug_log("[ws63 dbg] LD INIT ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_ld2402_runtime_status("ld-init");
        return;
    }

    if (strcmp(op, "STAT") == 0) {
        ws63_debug_dump_ld2402_runtime_status("ld-status");
        ws63_debug_dump_ld2402_distance_status("ld-distance");
        return;
    }

    if (strcmp(op, "VERSION") == 0) {
        if (ws63_task_ld2402_get_version(version, sizeof(version)) == ERRCODE_SUCC) {
            ws63_debug_log("[ws63 dbg] LD VERSION:%s\r\n", version);
        } else {
            ws63_debug_log("[ws63 dbg] LD VERSION failed\r\n");
        }
        return;
    }

    if (strcmp(op, "SN") == 0) {
        if (ws63_task_ld2402_get_sn_char(sn_text, sizeof(sn_text)) == ERRCODE_SUCC) {
            ws63_debug_log("[ws63 dbg] LD SN(char):%s\r\n", sn_text);
            return;
        }

        sn_hex_len = ws63_task_ld2402_get_sn_hex(sn_buf, sizeof(sn_buf));
        if (sn_hex_len <= 0) {
            ws63_debug_log("[ws63 dbg] LD SN failed\r\n");
            return;
        }

        {
            uint16_t idx;
            uint16_t pos = 0U;

            for (idx = 0U; idx < (uint16_t)sn_hex_len; idx++) {
                int32_t write_len;

                if ((sizeof(sn_text) - pos) <= 1U) {
                    break;
                }

                write_len = snprintf_s(&sn_text[pos], sizeof(sn_text) - pos,
                    (sizeof(sn_text) - pos) - 1U, "%02X", sn_buf[idx]);
                if (write_len <= 0) {
                    break;
                }
                pos = (uint16_t)(pos + (uint16_t)write_len);
            }
            sn_text[pos] = '\0';
        }
        ws63_debug_log("[ws63 dbg] LD SN(hex):%s\r\n", sn_text);
        return;
    }

    if (strcmp(op, "MODE") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        if (arg0 == NULL) {
            ws63_debug_log("[ws63 dbg] usage: LD MODE NORMAL|ENGINEERING\r\n");
            return;
        }

        if ((strcmp(arg0, "NORMAL") == 0) || (strcmp(arg0, "N") == 0)) {
            ret = ws63_task_ld2402_set_normal_mode();
            ws63_debug_log("[ws63 dbg] LD MODE NORMAL ret=0x%x\r\n", (unsigned int)ret);
            return;
        }

        if ((strcmp(arg0, "ENGINEERING") == 0) || (strcmp(arg0, "ENG") == 0) || (strcmp(arg0, "E") == 0)) {
            ret = ws63_task_ld2402_set_engineering_mode();
            ws63_debug_log("[ws63 dbg] LD MODE ENGINEERING ret=0x%x\r\n", (unsigned int)ret);
            return;
        }

        ws63_debug_log("[ws63 dbg] usage: LD MODE NORMAL|ENGINEERING\r\n");
        return;
    }

    if (strcmp(op, "DIST") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        if (arg0 == NULL) {
            ws63_debug_dump_ld2402_distance_status("ld-distance");
            return;
        }

        if (!ws63_debug_parse_distance_01m(arg0, &distance_01m)) {
            ws63_debug_log("[ws63 dbg] usage: LD DIST <0.7~10.0>\r\n");
            return;
        }

        ret = ws63_task_ld2402_set_max_distance((float)distance_01m / 10.0f);
        ws63_debug_log("[ws63 dbg] LD DIST %u.%um ret=0x%x\r\n",
            (unsigned int)(distance_01m / 10U),
            (unsigned int)(distance_01m % 10U),
            (unsigned int)ret);
        return;
    }

    if (strcmp(op, "DELAY") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        if ((arg0 == NULL) || (!ws63_debug_parse_u16_range(arg0, 0U, 65535U, &param_value_16))) {
            ws63_debug_log("[ws63 dbg] usage: LD DELAY <0-65535>\r\n");
            return;
        }

        ret = ws63_task_ld2402_set_disappear_delay(param_value_16);
        ws63_debug_log("[ws63 dbg] LD DELAY %u ret=0x%x\r\n",
            (unsigned int)param_value_16,
            (unsigned int)ret);
        return;
    }

    if (strcmp(op, "GET") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        if ((arg0 == NULL) || (!ws63_debug_parse_u32_value(arg0, &param_value))) {
            ws63_debug_log("[ws63 dbg] usage: LD GET <param_id>\r\n");
            return;
        }

        param_id = (uint16_t)param_value;
        ret = ws63_task_ld2402_read_param(param_id, &param_value);
        ws63_debug_log("[ws63 dbg] LD GET 0x%04x ret=0x%x val=0x%08x\r\n",
            (unsigned int)param_id,
            (unsigned int)ret,
            (unsigned int)param_value);
        return;
    }

    if (strcmp(op, "SET") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        arg1 = ws63_debug_next_token(&cursor);
        if ((arg0 == NULL) || (arg1 == NULL) || (!ws63_debug_parse_u32_value(arg0, &param_value)) ||
            (!ws63_debug_parse_u32_value(arg1, &power_inter))) {
            ws63_debug_log("[ws63 dbg] usage: LD SET <param_id> <value>\r\n");
            return;
        }

        ret = ws63_task_ld2402_set_param((uint16_t)param_value, power_inter);
        ws63_debug_log("[ws63 dbg] LD SET 0x%04x=0x%08x ret=0x%x\r\n",
            (unsigned int)param_value,
            (unsigned int)power_inter,
            (unsigned int)ret);
        return;
    }

    if (strcmp(op, "SAVE") == 0) {
        ret = ws63_task_ld2402_save_params();
        ws63_debug_log("[ws63 dbg] LD SAVE ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    if (strcmp(op, "GAIN") == 0) {
        ret = ws63_task_ld2402_auto_gain_adjust();
        ws63_debug_log("[ws63 dbg] LD GAIN ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    if (strcmp(op, "AUTO") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        arg1 = ws63_debug_next_token(&cursor);
        arg2 = ws63_debug_next_token(&cursor);
        if ((arg0 == NULL) || (arg1 == NULL) ||
            (!ws63_debug_parse_u16_range(arg0, 10U, 200U, &distance_01m)) ||
            (!ws63_debug_parse_u16_range(arg1, 10U, 200U, &param_value_16))) {
            ws63_debug_log("[ws63 dbg] usage: LD AUTO <trig10x> <hold10x> [static10x]\r\n");
            return;
        }

        if (arg2 != NULL) {
            if (!ws63_debug_parse_u16_range(arg2, 10U, 200U, &alarm_status)) {
                ws63_debug_log("[ws63 dbg] usage: LD AUTO <trig10x> <hold10x> [static10x]\r\n");
                return;
            }
        } else {
            alarm_status = param_value_16;
        }

        ret = ws63_task_ld2402_start_auto_threshold(distance_01m, param_value_16, alarm_status);
        ws63_debug_log("[ws63 dbg] LD AUTO %u/%u/%u ret=0x%x\r\n",
            (unsigned int)distance_01m,
            (unsigned int)param_value_16,
            (unsigned int)alarm_status,
            (unsigned int)ret);
        return;
    }

    if (strcmp(op, "PROGRESS") == 0) {
        ret = ws63_task_ld2402_get_auto_threshold_progress(&progress);
        ws63_debug_log("[ws63 dbg] LD PROGRESS ret=0x%x progress=%u%%\r\n",
            (unsigned int)ret,
            (unsigned int)progress);
        return;
    }

    if (strcmp(op, "ALARM") == 0) {
        ret = ws63_task_ld2402_get_auto_threshold_alarm(&alarm_status, &gate_bitmap);
        ws63_debug_log("[ws63 dbg] LD ALARM ret=0x%x status=%u bitmap=0x%04x\r\n",
            (unsigned int)ret,
            (unsigned int)alarm_status,
            (unsigned int)gate_bitmap);
        return;
    }

    if (strcmp(op, "PWR") == 0) {
        ret = ws63_task_ld2402_get_power_interference(&power_inter);
        ws63_debug_log("[ws63 dbg] LD PWR ret=0x%x value=%u\r\n",
            (unsigned int)ret,
            (unsigned int)power_inter);
        return;
    }

    if (strcmp(op, "SAVE3F") == 0) {
        ret = ws63_task_ld2402_refresh_save_flag();
        if (ret == ERRCODE_SUCC) {
            ret = ws63_task_ld2402_save_params();
        }
        ws63_debug_log("[ws63 dbg] LD SAVE3F ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    if (strcmp(op, "RAW") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        if ((arg0 == NULL) || (!ws63_debug_parse_hex_bytes(arg0, raw_buf, sizeof(raw_buf), &raw_len))) {
            ws63_debug_log("[ws63 dbg] usage: LD RAW <HEX...>\r\n");
            return;
        }

        ret = ws63_task_ld2402_send_raw(raw_buf, raw_len);
        ws63_debug_log("[ws63 dbg] LD RAW len=%u ret=0x%x\r\n",
            (unsigned int)raw_len,
            (unsigned int)ret);
        return;
    }

    if ((strcmp(op, "LOG") == 0) || (strcmp(op, "LOGSTAT") == 0) || (strcmp(op, "DISTSTAT") == 0)) {
        arg0 = ws63_debug_next_token(&cursor);
        if ((strcmp(op, "LOGSTAT") == 0) || (arg0 == NULL)) {
            ws63_debug_dump_ld2402_runtime_status("ld-log");
            return;
        }

        if (strcmp(arg0, "ON") == 0) {
            ret = ws63_task_ld2402_set_log_enable(1U);
            ws63_debug_log("[ws63 dbg] LD LOG ON ret=0x%x\r\n", (unsigned int)ret);
            ws63_debug_dump_ld2402_runtime_status("ld-log");
            return;
        }

        if (strcmp(arg0, "OFF") == 0) {
            ret = ws63_task_ld2402_set_log_enable(0U);
            ws63_debug_log("[ws63 dbg] LD LOG OFF ret=0x%x\r\n", (unsigned int)ret);
            ws63_debug_dump_ld2402_runtime_status("ld-log");
            return;
        }

        ws63_debug_log("[ws63 dbg] usage: LD LOG ON|OFF|STAT\r\n");
        return;
    }

    if (strcmp(op, "LOGINT") == 0) {
        arg0 = ws63_debug_next_token(&cursor);
        if ((arg0 == NULL) || (!ws63_debug_parse_u32_value(arg0, &param_value)) ||
            (param_value > WS63_DEBUG_SENSOR_LOG_GAP_MS_MAX)) {
            ws63_debug_log("[ws63 dbg] usage: LD LOGINT <0-60000>\r\n");
            return;
        }

        ret = ws63_task_ld2402_set_log_gap_ms(param_value);
        ws63_debug_log("[ws63 dbg] LD LOGINT %ums ret=0x%x\r\n",
            (unsigned int)param_value,
            (unsigned int)ret);
        ws63_debug_dump_ld2402_runtime_status("ld-log");
        return;
    }

    ws63_debug_log("[ws63 dbg] usage: LD HELP\r\n");
}

/**
 * @brief 输出当前 SLE 上行 success 日志配置。
 */
static void ws63_debug_dump_sle_uplink_log_status(const char *tag)
{
    ws63_debug_log("[ws63 dbg] %s sle_uplink_log=%s gap=%ums\r\n",
        (tag == NULL) ? "sle-ulog" : tag,
        (ws63_task_sle_uplink_log_get_enable() == 1U) ? "ON" : "OFF",
        (unsigned int)ws63_task_sle_uplink_log_get_gap_ms());
}

/**
 * @brief 打印调试命令帮助。
 */
static void ws63_debug_print_help(void)
{
    ws63_debug_log("[ws63 dbg] command list:\r\n");
    ws63_debug_log("[ws63 dbg]   HELP\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR FWD <0-100>\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR REV <0-100>\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR DUTY <0-100>\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR STOP\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR BRAKE\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR RPM\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR STAT\r\n");
    ws63_debug_log("[ws63 dbg]   MOTOR WATCH ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   ENCODER RESET\r\n");
    ws63_debug_log("[ws63 dbg]   BEEP ON [100-5000]\r\n");
    ws63_debug_log("[ws63 dbg]   BEEP FREQ <100-5000>\r\n");
    ws63_debug_log("[ws63 dbg]   BEEP VOL <0-100>\r\n");
    ws63_debug_log("[ws63 dbg]   BEEP OFF\r\n");
    ws63_debug_log("[ws63 dbg]   BEEP STAT\r\n");
    ws63_debug_log("[ws63 dbg]   RGB INIT\r\n");
    ws63_debug_log("[ws63 dbg]   RGB SET <R0-255> <G0-255> <B0-255>\r\n");
    ws63_debug_log("[ws63 dbg]   RGB OFF\r\n");
    ws63_debug_log("[ws63 dbg]   RGB DEMO ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   RGB STAT\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 INIT\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 STAT\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 READ\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 MASK\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 WATCH ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 ENABLE ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   TTP229 ALARM ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   LD HELP\r\n");
    ws63_debug_log("[ws63 dbg]   LD INIT\r\n");
    ws63_debug_log("[ws63 dbg]   LD STAT\r\n");
    ws63_debug_log("[ws63 dbg]   LD VERSION\r\n");
    ws63_debug_log("[ws63 dbg]   LD SN\r\n");
    ws63_debug_log("[ws63 dbg]   LD MODE NORMAL|ENGINEERING\r\n");
    ws63_debug_log("[ws63 dbg]   LD DIST [0.7~10.0]\r\n");
    ws63_debug_log("[ws63 dbg]   LD DELAY <0-65535>\r\n");
    ws63_debug_log("[ws63 dbg]   LD GET <param_id>\r\n");
    ws63_debug_log("[ws63 dbg]   LD SET <param_id> <value>\r\n");
    ws63_debug_log("[ws63 dbg]   LD SAVE\r\n");
    ws63_debug_log("[ws63 dbg]   LD GAIN\r\n");
    ws63_debug_log("[ws63 dbg]   LD AUTO <trig10x> <hold10x> [static10x]\r\n");
    ws63_debug_log("[ws63 dbg]   LD PROGRESS\r\n");
    ws63_debug_log("[ws63 dbg]   LD ALARM\r\n");
    ws63_debug_log("[ws63 dbg]   LD PWR\r\n");
    ws63_debug_log("[ws63 dbg]   LD SAVE3F\r\n");
    ws63_debug_log("[ws63 dbg]   LD RAW <HEX...>\r\n");
    ws63_debug_log("[ws63 dbg]   LD LOG ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   LD LOGINT <0-60000ms>\r\n");
    ws63_debug_log("[ws63 dbg]   LD LOGSTAT\r\n");
    ws63_debug_log("[ws63 dbg]   SLE ULOG ON|OFF\r\n");
    ws63_debug_log("[ws63 dbg]   SLE ULOGINT <0-60000ms>\r\n");
    ws63_debug_log("[ws63 dbg]   SLE ULOGSTAT\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 INIT\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 HANDSHAKE\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 CHECKSENSOR\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 RAW <HEX...>\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 STAT\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA HELP\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA ECHO\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCH <wait> <start> <count>\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHRES <buf1|2> <start> <count>\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHECHO <wait> <start> <count>\r\n");
    ws63_debug_log("[ws63 dbg]   ZW101 ZA TERM\r\n");
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

    ws63_debug_log("[ws63 dbg] cmd<=%s\r\n", cmd);

    if ((strcmp(cmd, "HELP") == 0) || (strcmp(cmd, "?") == 0)) {
        ws63_debug_print_help();
        return;
    }

    if ((strncmp(cmd, "LD ", 3) == 0) || (strcmp(cmd, "LD") == 0)) {
        ws63_debug_exec_ld_command(cmd);
        return;
    }

    if (strcmp(cmd, "MOTOR STOP") == 0) {
        ret = ws63_task_motor_coast_stop();
        ws63_debug_log("[ws63 dbg] MOTOR STOP ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_motor_status("stop");
        return;
    }

    if (strcmp(cmd, "MOTOR BRAKE") == 0) {
        ret = ws63_task_motor_brake_stop();
        ws63_debug_log("[ws63 dbg] MOTOR BRAKE ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_motor_status("brake");
        return;
    }

    if ((strcmp(cmd, "MOTOR RPM") == 0) || (strcmp(cmd, "MOTOR STAT") == 0)) {
        ws63_debug_dump_motor_status("query");
        return;
    }

    if (strcmp(cmd, "MOTOR WATCH ON") == 0) {
        g_ws63_debug_motor_watch_enable = 1U;
        g_ws63_debug_last_watch_ms = 0U;
        ws63_debug_log("[ws63 dbg] MOTOR WATCH ON\r\n");
        return;
    }

    if (strcmp(cmd, "MOTOR WATCH OFF") == 0) {
        g_ws63_debug_motor_watch_enable = 0U;
        ws63_debug_log("[ws63 dbg] MOTOR WATCH OFF\r\n");
        return;
    }

    if (strcmp(cmd, "ENCODER RESET") == 0) {
        ws63_encoder_reset();
        ws63_debug_log("[ws63 dbg] encoder counter reset\r\n");
        ws63_debug_dump_motor_status("encoder-reset");
        return;
    }

    if (strcmp(cmd, "BEEP STAT") == 0) {
        ws63_debug_dump_beep_status("beep-query");
        return;
    }

    if (strcmp(cmd, "BEEP OFF") == 0) {
        ret = ws63_task_buzzer_off();
        ws63_debug_log("[ws63 dbg] BEEP OFF ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-off");
        return;
    }

    if (strcmp(cmd, "BEEP ON") == 0) {
        ret = ws63_task_buzzer_on(WS63_BEEP_DEFAULT_FREQ_HZ);
        ws63_debug_log("[ws63 dbg] BEEP ON %uHz ret=0x%x\r\n",
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
            ws63_debug_log("[ws63 dbg] invalid freq, expect %u~%u\r\n",
                (unsigned int)WS63_BEEP_MIN_FREQ_HZ,
                (unsigned int)WS63_BEEP_MAX_FREQ_HZ);
            return;
        }

        ret = ws63_task_buzzer_on(freq_hz);
        ws63_debug_log("[ws63 dbg] BEEP ON %uHz ret=0x%x\r\n",
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
            ws63_debug_log("[ws63 dbg] invalid freq, expect %u~%u\r\n",
                (unsigned int)WS63_BEEP_MIN_FREQ_HZ,
                (unsigned int)WS63_BEEP_MAX_FREQ_HZ);
            return;
        }

        ret = ws63_task_buzzer_on(freq_hz);
        ws63_debug_log("[ws63 dbg] BEEP FREQ %uHz ret=0x%x\r\n",
            (unsigned int)freq_hz,
            (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-freq");
        return;
    }

    if (strncmp(cmd, "BEEP VOL ", 9) == 0) {
        if (!ws63_debug_parse_duty(cmd + 9, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid volume, expect 0~100\r\n");
            return;
        }

        ret = ws63_task_buzzer_set_volume(duty);
        ws63_debug_log("[ws63 dbg] BEEP VOL %u%% ret=0x%x\r\n",
            (unsigned int)duty,
            (unsigned int)ret);
        ws63_debug_dump_beep_status("beep-vol");
        return;
    }

    if (strcmp(cmd, "RGB STAT") == 0) {
        ws63_debug_dump_rgb_status("rgb-query");
        return;
    }

    if (strcmp(cmd, "TTP229 INIT") == 0) {
        ret = ws63_task_ttp229_reinit();
        ws63_debug_log("[ws63 dbg] TTP229 INIT ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_ttp229_status("ttp229-init");
        return;
    }

    if (strcmp(cmd, "TTP229 STAT") == 0) {
        ws63_debug_dump_ttp229_status("ttp229-query");
        return;
    }

    if ((strcmp(cmd, "TTP229 READ") == 0) || (strcmp(cmd, "TTP229 MASK") == 0)) {
        char key_text[64] = {0};

        if (ws63_task_ttp229_get_pressed_text(key_text, sizeof(key_text)) != ERRCODE_SUCC) {
            (void)strncpy_s(key_text, sizeof(key_text), "ERR", 3U);
        }

        ws63_debug_log("[ws63 dbg] TTP229 raw=0x%04x mask=0x%04x count=%u keys=%s\r\n",
            (unsigned int)ws63_task_ttp229_get_raw_code(),
            (unsigned int)ws63_task_ttp229_get_pressed_mask(),
            (unsigned int)ws63_task_ttp229_get_pressed_count(),
            key_text);
        return;
    }

    if (strcmp(cmd, "TTP229 WATCH ON") == 0) {
        g_ws63_debug_ttp229_watch_enable = 1U;
        g_ws63_debug_last_watch_ms = 0U;
        ws63_debug_log("[ws63 dbg] TTP229 WATCH ON\r\n");
        return;
    }

    if (strcmp(cmd, "TTP229 WATCH OFF") == 0) {
        g_ws63_debug_ttp229_watch_enable = 0U;
        ws63_debug_log("[ws63 dbg] TTP229 WATCH OFF\r\n");
        return;
    }

    if (strcmp(cmd, "TTP229 ENABLE ON") == 0) {
        ret = ws63_task_ttp229_set_enable(1U);
        ws63_debug_log("[ws63 dbg] TTP229 ENABLE ON ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_ttp229_status("ttp229-enable");
        return;
    }

    if (strcmp(cmd, "TTP229 ENABLE OFF") == 0) {
        ret = ws63_task_ttp229_set_enable(0U);
        ws63_debug_log("[ws63 dbg] TTP229 ENABLE OFF ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_ttp229_status("ttp229-enable");
        return;
    }

    if (strcmp(cmd, "TTP229 ALARM ON") == 0) {
        ret = ws63_task_ttp229_set_multi_key_alarm(1U);
        ws63_debug_log("[ws63 dbg] TTP229 ALARM ON ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_ttp229_status("ttp229-alarm");
        return;
    }

    if (strcmp(cmd, "TTP229 ALARM OFF") == 0) {
        ret = ws63_task_ttp229_set_multi_key_alarm(0U);
        ws63_debug_log("[ws63 dbg] TTP229 ALARM OFF ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_ttp229_status("ttp229-alarm");
        return;
    }

    if (strcmp(cmd, "RGB INIT") == 0) {
        ret = ws63_task_rgb_reinit();
        ws63_debug_log("[ws63 dbg] RGB INIT ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-init");
        return;
    }

    if (strcmp(cmd, "RGB OFF") == 0) {
        ret = ws63_task_rgb_off();
        ws63_debug_log("[ws63 dbg] RGB OFF ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-off");
        return;
    }

    if (strcmp(cmd, "RGB DEMO ON") == 0) {
        ret = ws63_task_rgb_set_demo_enable(1U);
        ws63_debug_log("[ws63 dbg] RGB DEMO ON ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-demo");
        return;
    }

    if (strcmp(cmd, "RGB DEMO OFF") == 0) {
        ret = ws63_task_rgb_set_demo_enable(0U);
        ws63_debug_log("[ws63 dbg] RGB DEMO OFF ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-demo");
        return;
    }

    if (strncmp(cmd, "RGB SET ", 8) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 8, rgb_args, 4U, &rgb_argc) || (rgb_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: RGB SET <R0-255> <G0-255> <B0-255>\r\n");
            return;
        }
        if ((rgb_args[0] > 255U) || (rgb_args[1] > 255U) || (rgb_args[2] > 255U)) {
            ws63_debug_log("[ws63 dbg] invalid rgb range, expect 0~255\r\n");
            return;
        }

        ret = ws63_task_rgb_set_color((uint8_t)rgb_args[0], (uint8_t)rgb_args[1], (uint8_t)rgb_args[2]);
        ws63_debug_log("[ws63 dbg] RGB SET (%u,%u,%u) ret=0x%x\r\n",
            (unsigned int)rgb_args[0],
            (unsigned int)rgb_args[1],
            (unsigned int)rgb_args[2],
            (unsigned int)ret);
        ws63_debug_dump_rgb_status("rgb-set");
        return;
    }

    if (strcmp(cmd, "SLE ULOG ON") == 0) {
        ret = ws63_task_sle_uplink_log_set_enable(1U);
        ws63_debug_log("[ws63 dbg] SLE ULOG ON ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_sle_uplink_log_status("sle-ulog");
        return;
    }

    if (strcmp(cmd, "SLE ULOG OFF") == 0) {
        ret = ws63_task_sle_uplink_log_set_enable(0U);
        ws63_debug_log("[ws63 dbg] SLE ULOG OFF ret=0x%x\r\n", (unsigned int)ret);
        ws63_debug_dump_sle_uplink_log_status("sle-ulog");
        return;
    }

    if (strcmp(cmd, "SLE ULOGSTAT") == 0) {
        ws63_debug_dump_sle_uplink_log_status("sle-ulog");
        return;
    }

    if (strncmp(cmd, "SLE ULOGINT ", 11) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 11, rgb_args, 4U, &rgb_argc) || (rgb_argc != 1U) ||
            (rgb_args[0] > WS63_DEBUG_SENSOR_LOG_GAP_MS_MAX)) {
            ws63_debug_log("[ws63 dbg] usage: SLE ULOGINT <0-60000>\r\n");
            return;
        }

        ret = ws63_task_sle_uplink_log_set_gap_ms(rgb_args[0]);
        ws63_debug_log("[ws63 dbg] SLE ULOGINT %ums ret=0x%x\r\n",
            (unsigned int)rgb_args[0],
            (unsigned int)ret);
        ws63_debug_dump_sle_uplink_log_status("sle-ulog");
        return;
    }

    if (strncmp(cmd, "LD2402 RAW ", 10) == 0) {
        if (!ws63_debug_parse_hex_bytes(cmd + 10, raw_buf, sizeof(raw_buf), &raw_len)) {
            ws63_debug_log("[ws63 dbg] invalid LD2402 raw hex, example: FD FC FB FA\r\n");
            return;
        }

        ret = ws63_task_ld2402_send_raw(raw_buf, raw_len);
        ws63_debug_log("[ws63 dbg] LD2402 RAW len=%u ret=0x%x\r\n",
            (unsigned int)raw_len,
            (unsigned int)ret);
        return;
    }

    if (strcmp(cmd, "ZW101 INIT") == 0) {
        ret = ws63_task_zw101_reinit();
        ws63_debug_log("[ws63 dbg] ZW101 INIT ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    if (strcmp(cmd, "ZW101 HANDSHAKE") == 0) {
        za_ack = 0xFFU;
        ret = ws63_task_zw101_handshake(&za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 HANDSHAKE ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strcmp(cmd, "ZW101 CHECKSENSOR") == 0) {
        za_ack = 0xFFU;
        ret = ws63_task_zw101_check_sensor(&za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 CHECKSENSOR ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strcmp(cmd, "ZW101 ZA HELP") == 0) {
        ws63_debug_log("[ws63 dbg] ZA commands:\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA ECHO\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCH <wait> <start> <count>\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHRES <buf1|2> <start> <count>\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA SEARCHECHO <wait> <start> <count>\r\n");
        ws63_debug_log("[ws63 dbg]   ZW101 ZA TERM\r\n");
        return;
    }

    if (strcmp(cmd, "ZW101 ZA ECHO") == 0) {
        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_get_echo(&za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA ECHO ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA LOGIN ", 15) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 15, za_args, 6U, &za_argc) || (za_argc != 5U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>\r\n");
            return;
        }
        if ((za_args[1] > 15U) || ((za_args[2] != 2U) && (za_args[2] != 3U)) ||
            (za_args[3] > 65535U) || (za_args[4] > 1U)) {
            ws63_debug_log("[ws63 dbg] invalid LOGIN args\r\n");
            return;
        }

        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_auto_login((uint8_t)za_args[0],
            (uint8_t)za_args[1],
            (uint8_t)za_args[2],
            (uint16_t)za_args[3],
            (uint8_t)za_args[4],
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA LOGIN ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA SEARCH ", 16) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 16, za_args, 6U, &za_argc) || (za_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA SEARCH <wait> <start> <count>\r\n");
            return;
        }
        if ((za_args[1] > 65535U) || (za_args[2] > 65535U)) {
            ws63_debug_log("[ws63 dbg] invalid SEARCH args\r\n");
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
        ws63_debug_log("[ws63 dbg] ZW101 ZA SEARCH ret=0x%x ack=0x%02x id=%u score=%u\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack,
            (unsigned int)za_page_id,
            (unsigned int)za_score);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA SEARCHRES ", 19) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 19, za_args, 6U, &za_argc) || (za_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA SEARCHRES <buf1|2> <start> <count>\r\n");
            return;
        }
        if (((za_args[0] != 1U) && (za_args[0] != 2U)) || (za_args[1] > 65535U) || (za_args[2] > 65535U)) {
            ws63_debug_log("[ws63 dbg] invalid SEARCHRES args\r\n");
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
        ws63_debug_log("[ws63 dbg] ZW101 ZA SEARCHRES ret=0x%x ack=0x%02x id=%u score=%u\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack,
            (unsigned int)za_page_id,
            (unsigned int)za_score);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA LOGINLIGHT ", 20) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 20, za_args, 6U, &za_argc) || (za_argc != 4U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>\r\n");
            return;
        }
        if (((za_args[1] != 2U) && (za_args[1] != 3U)) || (za_args[2] > 65535U) || (za_args[3] > 1U)) {
            ws63_debug_log("[ws63 dbg] invalid LOGINLIGHT args\r\n");
            return;
        }

        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_auto_login_stab((uint8_t)za_args[0],
            (uint8_t)za_args[1],
            (uint16_t)za_args[2],
            (uint8_t)za_args[3],
            &za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA LOGINLIGHT ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strncmp(cmd, "ZW101 ZA SEARCHECHO ", 20) == 0) {
        if (!ws63_debug_parse_u32_tokens(cmd + 20, za_args, 6U, &za_argc) || (za_argc != 3U)) {
            ws63_debug_log("[ws63 dbg] usage: ZW101 ZA SEARCHECHO <wait> <start> <count>\r\n");
            return;
        }
        if ((za_args[1] > 65535U) || (za_args[2] > 65535U)) {
            ws63_debug_log("[ws63 dbg] invalid SEARCHECHO args\r\n");
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
        ws63_debug_log("[ws63 dbg] ZW101 ZA SEARCHECHO ret=0x%x ack=0x%02x id=%u score=%u\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack,
            (unsigned int)za_page_id,
            (unsigned int)za_score);
        return;
    }

    if (strcmp(cmd, "ZW101 ZA TERM") == 0) {
        za_ack = 0xFFU;
        ret = ws63_task_zw101_za_terminate(&za_ack);
        ws63_debug_log("[ws63 dbg] ZW101 ZA TERM ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)za_ack);
        return;
    }

    if (strcmp(cmd, "ZW101 STAT") == 0) {
        ws63_debug_log("[ws63 dbg] ZW101 subport=%u\r\n", (unsigned int)ZW101_SUBPORT);
        return;
    }

    if (strncmp(cmd, "ZW101 RAW ", 10) == 0) {
        if (!ws63_debug_parse_hex_bytes(cmd + 10, raw_buf, sizeof(raw_buf), &raw_len)) {
            ws63_debug_log("[ws63 dbg] invalid ZW101 raw hex, example: EF 01 FF FF\r\n");
            return;
        }

        ret = ws63_task_zw101_send_raw(raw_buf, raw_len);
        ws63_debug_log("[ws63 dbg] ZW101 RAW len=%u ret=0x%x\r\n",
            (unsigned int)raw_len,
            (unsigned int)ret);
        return;
    }

    if (strncmp(cmd, "MOTOR FWD ", 10) == 0) {
        if (!ws63_debug_parse_duty(cmd + 10, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid duty, expect 0~100\r\n");
            return;
        }
        ret = ws63_task_motor_forward(duty);
        ws63_debug_log("[ws63 dbg] MOTOR FWD %u ret=0x%x\r\n", (unsigned int)duty, (unsigned int)ret);
        ws63_debug_dump_motor_status("forward");
        return;
    }

    if (strncmp(cmd, "MOTOR REV ", 10) == 0) {
        if (!ws63_debug_parse_duty(cmd + 10, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid duty, expect 0~100\r\n");
            return;
        }
        ret = ws63_task_motor_reverse(duty);
        ws63_debug_log("[ws63 dbg] MOTOR REV %u ret=0x%x\r\n", (unsigned int)duty, (unsigned int)ret);
        ws63_debug_dump_motor_status("reverse");
        return;
    }

    if (strncmp(cmd, "MOTOR DUTY ", 11) == 0) {
        if (!ws63_debug_parse_duty(cmd + 11, &duty)) {
            ws63_debug_log("[ws63 dbg] invalid duty, expect 0~100\r\n");
            return;
        }
        ret = ws63_task_motor_set_duty(duty);
        ws63_debug_log("[ws63 dbg] MOTOR DUTY %u ret=0x%x\r\n", (unsigned int)duty, (unsigned int)ret);
        ws63_debug_dump_motor_status("duty");
        return;
    }

    ws63_debug_log("[ws63 dbg] unknown command: %s\r\n", cmd);
    ws63_debug_log("[ws63 dbg] type HELP for command list\r\n");
}

/**
 * @brief 初始化调试串口命令能力。
 */
static void ws63_debug_uart_cmd_init(void)
{
    errcode_t ret;

    g_ws63_debug_uart_ready = 0U;
    g_ws63_debug_motor_watch_enable = 0U;
    g_ws63_debug_ttp229_watch_enable = 0U;
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
        osal_printk("[wk2114 final task] debug uart init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    ret = ws63_debug_uart_register_rx_callback(ws63_debug_uart_rx_callback, 1U);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] debug uart cb reg fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_debug_uart_ready = 1U;
    ws63_debug_log("[ws63 dbg] uart ready bus=%u baud=%u tx=%u rx=%u\r\n",
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
        ws63_debug_log("[ws63 dbg] uart rx error\r\n");
    }
    if (too_long == 1U) {
        ws63_debug_log("[ws63 dbg] command too long, dropped\r\n");
    }
    if (overflow == 1U) {
        ws63_debug_log("[ws63 dbg] command queue overflow, dropped\r\n");
    }

    while (ws63_debug_queue_pop_line(cmd_line, sizeof(cmd_line)) == 1U) {
        ws63_debug_exec_command(cmd_line);
    }

    /* 周期监控统一节拍：任一 WATCH 开启都进入同一时间窗，避免多路日志抢占。 */
    if ((g_ws63_debug_motor_watch_enable == 0U) && (g_ws63_debug_ttp229_watch_enable == 0U)) {
        return;
    }

    if ((now_ms - g_ws63_debug_last_watch_ms) < WS63_DEBUG_WATCH_PERIOD_MS) {
        return;
    }

    g_ws63_debug_last_watch_ms = now_ms;
    if (g_ws63_debug_motor_watch_enable == 1U) {
        ws63_debug_dump_motor_status("watch");
    }
    if (g_ws63_debug_ttp229_watch_enable == 1U) {
        ws63_debug_dump_ttp229_status("watch");
    }
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
