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
#include "soc_osal.h"

#include "securec.h"

#include "ws63_final_config.h"
#include "ws63_final_bsp.h"
#include "ws63_motor.h"
#include "ws63_encoder.h"

#define WS63_DEBUG_LOG_BUF_MAX 192U
#define WS63_DEBUG_CMD_QUEUE_DEPTH 8U

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
    return osal_irq_lock();
}

/**
 * @brief 调试命令队列临界区解锁。
 */
static void ws63_debug_irq_unlock(unsigned int irq_status)
{
    osal_irq_restore(irq_status);
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

    return rpm_abs;
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

    (void)ws63_bsp_debug_uart_write((const uint8_t *)text, (uint16_t)text_len, 0U);
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
}

/**
 * @brief 执行单条调试命令。
 */
static void ws63_debug_exec_command(const char *line)
{
    uint16_t i;
    uint8_t duty;
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

    ret = ws63_bsp_debug_uart_init(g_ws63_debug_uart_rx_buf, sizeof(g_ws63_debug_uart_rx_buf));
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] debug uart init fail, ret=0x%x\\r\\n", (unsigned int)ret);
        return;
    }

    ret = ws63_bsp_debug_uart_register_rx_callback(ws63_debug_uart_rx_callback, 1U);
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
