/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Slave side OLED module.
 */

#include "sle_uart_slave_module.h"

#include <stdarg.h>
#include <string.h>

#include "securec.h"
#include "ssd1306/hal_bsp_ssd1306.h"

#define osal_printk mine_slave_log

static bool g_mine_oled_ready = false;
static volatile bool g_mine_oled_dirty = false;
static volatile uint8_t g_mine_oled_dirty_mask = 0;
static char g_mine_oled_lines[MINE_OLED_LINE_COUNT][MINE_OLED_LINE_CHARS + 1] = {0};
static uint32_t g_mine_oled_rx_count = 0;
static uint32_t g_mine_oled_tx_count = 0;
static uint16_t g_mine_oled_rx_last_len = 0;
static uint16_t g_mine_oled_tx_last_len = 0;
static uint8_t g_mine_oled_rx_last_uart = MINE_UART_BUS_INVALID;
static uint8_t g_mine_oled_tx_last_uart = MINE_UART_BUS_INVALID;

/**
 * @brief 将 UART 总线号转换为 OLED 单字符标记。
 *
 * @param bus UART 总线号。
 * @return char 标记字符，未知总线返回 '*'.
 */
static char mine_slave_uart_bus_mark(uint8_t bus)
{
    if (bus == UART_BUS_0) {
        return '0';
    }
    if (bus == UART_BUS_1) {
        return '1';
    }
    if (bus == UART_BUS_2) {
        return '2';
    }
    return '*';
}

/**
 * @brief 清空一行 OLED 缓冲区并填充空格。
 *
 * @param line_buf 行缓冲区指针。
 */
static void mine_oled_clear_line(char *line_buf)
{
    if (line_buf == NULL) {
        return;
    }

    (void)memset_s(line_buf, MINE_OLED_LINE_CHARS + 1, ' ', MINE_OLED_LINE_CHARS);
    line_buf[MINE_OLED_LINE_CHARS] = '\0';
}

/**
 * @brief 按脏位掩码刷新 OLED 指定行。
 *
 * @param dirty_mask 行脏位掩码。
 */
static void mine_oled_refresh(uint8_t dirty_mask)
{
    uint32_t line_index;

    if (!g_mine_oled_ready) {
        return;
    }

    for (line_index = 0; line_index < MINE_OLED_LINE_COUNT; line_index++) {
        if ((dirty_mask & (uint8_t)(1U << line_index)) == 0U) {
            continue;
        }
        SSD1306_ShowStr(0, (uint8_t)line_index, g_mine_oled_lines[line_index], MINE_OLED_FONT_SIZE);
    }
}

/**
 * @brief 将指定行标记为脏并等待统一刷新。
 *
 * @param line_index 行号。
 */
static void mine_oled_mark_line_dirty(uint32_t line_index)
{
    if (line_index >= MINE_OLED_LINE_COUNT) {
        return;
    }

    g_mine_oled_dirty_mask |= (uint8_t)(1U << line_index);
    g_mine_oled_dirty = true;
}

/**
 * @brief 刷新当前累计的 OLED 脏行。
 */
void mine_slave_oled_flush_pending(void)
{
    uint8_t dirty_mask;

    if ((!g_mine_oled_ready) || (!g_mine_oled_dirty)) {
        return;
    }

    dirty_mask = g_mine_oled_dirty_mask;
    g_mine_oled_dirty_mask = 0;
    g_mine_oled_dirty = false;

    if (dirty_mask == 0U) {
        return;
    }

    mine_oled_refresh(dirty_mask);
}

/**
 * @brief 直接设置指定 OLED 行文本（超长自动截断）。
 *
 * @param line_index 行号。
 * @param text       文本内容。
 */
static void mine_oled_set_line_raw(uint32_t line_index, const char *text)
{
    uint32_t char_index;
    char new_line[MINE_OLED_LINE_CHARS + 1] = {0};
    char *target_line;

    if (line_index >= MINE_OLED_LINE_COUNT) {
        return;
    }

    mine_oled_clear_line(new_line);

    if (text != NULL) {
        for (char_index = 0; char_index < MINE_OLED_LINE_CHARS; char_index++) {
            if (text[char_index] == '\0') {
                break;
            }
            new_line[char_index] = text[char_index];
        }
    }

    target_line = g_mine_oled_lines[line_index];
    if (memcmp(target_line, new_line, sizeof(new_line)) == 0) {
        return;
    }

    if (memcpy_s(target_line, MINE_OLED_LINE_CHARS + 1, new_line, sizeof(new_line)) != EOK) {
        return;
    }

    mine_oled_mark_line_dirty(line_index);
}

/**
 * @brief 使用格式化字符串更新 OLED 行内容。
 *
 * @param line_index 行号。
 * @param fmt        printf 风格格式串。
 */
static void mine_oled_set_linef(uint32_t line_index, const char *fmt, ...)
{
    char line_buf[MINE_OLED_EVENT_BUFFER_LEN] = {0};
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    if (vsnprintf_s(line_buf, sizeof(line_buf), sizeof(line_buf) - 1, fmt, args) <= 0) {
        va_end(args);
        return;
    }
    va_end(args);

    mine_oled_set_line_raw(line_index, line_buf);
}

/**
 * @brief 更新 OLED 的状态行文本。
 *
 * @param text 状态文本。
 */
void mine_slave_oled_push_state(const char *text)
{
    if ((!g_mine_oled_ready) || (text == NULL)) {
        return;
    }

    mine_oled_set_linef(MINE_OLED_LINE_STATE, "STATE:%s", text);
}

/**
 * @brief 将预览文本拆分成三行写入 OLED 数据区域。
 *
 * @param text 连续预览文本。
 */
static void mine_oled_set_data_lines(const char *text)
{
    const uint32_t data_lines[MINE_OLED_DATA_LINE_COUNT] = {
        MINE_OLED_LINE_DATA0, MINE_OLED_LINE_DATA1, MINE_OLED_LINE_DATA2
    };
    uint32_t line_idx;
    uint32_t text_len;

    if (text == NULL) {
        for (line_idx = 0; line_idx < MINE_OLED_DATA_LINE_COUNT; line_idx++) {
            mine_oled_set_line_raw(data_lines[line_idx], "");
        }
        return;
    }

    text_len = (uint32_t)strlen(text);
    for (line_idx = 0; line_idx < MINE_OLED_DATA_LINE_COUNT; line_idx++) {
        uint32_t offset = line_idx * MINE_OLED_LINE_CHARS;
        uint32_t char_index;
        char line_buf[MINE_OLED_LINE_CHARS + 1] = {0};

        if (offset >= text_len) {
            mine_oled_set_line_raw(data_lines[line_idx], "");
            continue;
        }

        for (char_index = 0; char_index < MINE_OLED_LINE_CHARS; char_index++) {
            char current_char = text[offset + char_index];
            if (current_char == '\0') {
                break;
            }
            line_buf[char_index] = current_char;
        }

        mine_oled_set_line_raw(data_lines[line_idx], line_buf);
    }
}

/**
 * @brief 上报一条数据事件到 OLED（方向、UART、计数与内容预览）。
 *
 * @param uart_bus UART 来源总线。
 * @param prefix   方向前缀（用于识别 RX/TX）。
 * @param data     事件数据。
 * @param len      数据长度。
 */
void mine_slave_oled_push_data_event(uint8_t uart_bus, const char *prefix, const uint8_t *data, uint16_t len)
{
    char preview_line[MINE_OLED_DATA_TOTAL_CHARS + 1] = {0};
    char current_char;
    char uart_mark;
    uint16_t preview_len;
    uint16_t max_payload_chars;
    char direction = '?';
    uint32_t prefix_len = 0;
    uint16_t byte_index;
    int32_t append_len;

    if ((!g_mine_oled_ready) || (prefix == NULL)) {
        return;
    }

    uart_mark = mine_slave_uart_bus_mark(uart_bus);

    if (strstr(prefix, "RX") != NULL) {
        direction = 'R';
        g_mine_oled_rx_last_len = len;
        g_mine_oled_rx_count++;
        g_mine_oled_rx_last_uart = uart_bus;
    } else if (strstr(prefix, "TX") != NULL) {
        direction = 'T';
        g_mine_oled_tx_last_len = len;
        g_mine_oled_tx_count++;
        g_mine_oled_tx_last_uart = uart_bus;
    }

    mine_oled_set_linef(MINE_OLED_LINE_RX, "RX%c:%u C:%lu",
        mine_slave_uart_bus_mark(g_mine_oled_rx_last_uart),
        g_mine_oled_rx_last_len, (unsigned long)g_mine_oled_rx_count);
    mine_oled_set_linef(MINE_OLED_LINE_TX, "TX%c:%u C:%lu",
        mine_slave_uart_bus_mark(g_mine_oled_tx_last_uart),
        g_mine_oled_tx_last_len, (unsigned long)g_mine_oled_tx_count);

    if ((data == NULL) || (len == 0)) {
        (void)snprintf_s(preview_line, sizeof(preview_line),
            sizeof(preview_line) - 1, "DATA:%c%c --", direction, uart_mark);
        mine_oled_set_data_lines(preview_line);
        return;
    }

    append_len = snprintf_s(preview_line, sizeof(preview_line),
        sizeof(preview_line) - 1, "DATA:%c%c ", direction, uart_mark);
    if (append_len > 0) {
        prefix_len = (uint32_t)append_len;
    }

    if (prefix_len >= MINE_OLED_DATA_TOTAL_CHARS) {
        mine_oled_set_data_lines(preview_line);
        return;
    }

    max_payload_chars = (uint16_t)(MINE_OLED_DATA_TOTAL_CHARS - prefix_len);
    preview_len = len;
    if (preview_len > max_payload_chars) {
        if (max_payload_chars > 3) {
            preview_len = (uint16_t)(max_payload_chars - 3);
        } else {
            preview_len = max_payload_chars;
        }
    }

    for (byte_index = 0; byte_index < preview_len; byte_index++) {
        current_char = (char)data[byte_index];
        if ((current_char < ' ') || (current_char > '~')) {
            current_char = '.';
        }
        if ((prefix_len + byte_index) >= (sizeof(preview_line) - 1)) {
            break;
        }
        preview_line[prefix_len + byte_index] = current_char;
    }
    prefix_len += preview_len;

    if ((len > preview_len) && (max_payload_chars > 3) && (prefix_len + 3 < sizeof(preview_line))) {
        preview_line[prefix_len++] = '.';
        preview_line[prefix_len++] = '.';
        preview_line[prefix_len++] = '.';
    }

    preview_line[prefix_len] = '\0';
    mine_oled_set_data_lines(preview_line);
}

/**
 * @brief 将 UUID 显示恢复为默认服务/特征组合。
 */
void mine_slave_oled_set_uuid_default(void)
{
    if (!g_mine_oled_ready) {
        return;
    }

    mine_oled_set_linef(MINE_OLED_LINE_UUID, "UUID:S%04X P%04X",
        MINE_SLE_UART_SERVICE_UUID, MINE_SLE_UART_PROPERTY_UUID);
}

/**
 * @brief 在 UUID 行显示已发现特征 UUID 与句柄。
 *
 * @param uuid16  16 位 UUID。
 * @param handle  句柄值。
 */
void mine_slave_oled_set_uuid_property(uint16_t uuid16, uint16_t handle)
{
    if (!g_mine_oled_ready) {
        return;
    }

    mine_oled_set_linef(MINE_OLED_LINE_UUID, "UUID:P%04X H:%u", uuid16, handle);
}

/**
 * @brief 初始化 Slave OLED 页面与默认显示内容。
 */
void mine_slave_oled_init(void)
{
    uint32_t line_index;
    uint32_t ret;

    if (g_mine_oled_ready) {
        return;
    }

    ret = SSD1306_Init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] oled init failed, ret=%x\r\n", ret);
        return;
    }

    SSD1306_CLS();
    for (line_index = 0; line_index < MINE_OLED_LINE_COUNT; line_index++) {
        mine_oled_clear_line(g_mine_oled_lines[line_index]);
    }

    g_mine_oled_rx_count = 0;
    g_mine_oled_tx_count = 0;
    g_mine_oled_rx_last_len = 0;
    g_mine_oled_tx_last_len = 0;
    g_mine_oled_rx_last_uart = MINE_UART_BUS_INVALID;
    g_mine_oled_tx_last_uart = MINE_UART_BUS_INVALID;
    g_mine_oled_dirty_mask = 0;

    g_mine_oled_ready = true;
    mine_oled_set_line_raw(MINE_OLED_LINE_TITLE, "SLAVE UART<->SLE");
    mine_oled_set_line_raw(MINE_OLED_LINE_STATE, "STATE:BOOT");
    mine_oled_set_data_lines("DATA:--");
    mine_slave_oled_set_uuid_default();
    mine_oled_set_line_raw(MINE_OLED_LINE_RX, "RX*:0 C:0");
    mine_oled_set_line_raw(MINE_OLED_LINE_TX, "TX*:0 C:0");
    mine_slave_oled_flush_pending();
}
