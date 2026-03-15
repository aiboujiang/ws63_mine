/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块 OLED 调试输出。
 */

#include "mine_wk2114_uart2_ext_module.h"

#include <stdarg.h>
#include <string.h>

#include "securec.h"
#include "ssd1306/hal_bsp_ssd1306.h"

#define osal_printk mine_wk2114_log

static bool g_mine_wk2114_oled_ready = false;
static volatile bool g_mine_wk2114_oled_dirty = false;
static volatile uint8_t g_mine_wk2114_oled_dirty_mask = 0;
static char g_mine_wk2114_oled_lines[MINE_WK2114_OLED_LINE_COUNT][MINE_WK2114_OLED_LINE_CHARS + 1] = {0};
static uint32_t g_mine_wk2114_oled_rx_count = 0;
static uint32_t g_mine_wk2114_oled_tx_count = 0;
static uint16_t g_mine_wk2114_oled_rx_last_len = 0;
static uint16_t g_mine_wk2114_oled_tx_last_len = 0;

/**
 * @brief 清空一行 OLED 文本缓冲区并填充空格。
 *
 * @param line_buf 行缓冲区。
 */
static void mine_wk2114_oled_clear_line(char *line_buf)
{
    if (line_buf == NULL) {
        return;
    }

    (void)memset_s(line_buf, MINE_WK2114_OLED_LINE_CHARS + 1, ' ', MINE_WK2114_OLED_LINE_CHARS);
    line_buf[MINE_WK2114_OLED_LINE_CHARS] = '\0';
}

/**
 * @brief 标记指定行为脏并等待统一刷新。
 *
 * @param line_index 行号。
 */
static void mine_wk2114_oled_mark_line_dirty(uint32_t line_index)
{
    if (line_index >= MINE_WK2114_OLED_LINE_COUNT) {
        return;
    }

    g_mine_wk2114_oled_dirty_mask |= (uint8_t)(1U << line_index);
    g_mine_wk2114_oled_dirty = true;
}

/**
 * @brief 按脏位掩码刷新 OLED 行。
 *
 * @param dirty_mask 脏位掩码。
 */
static void mine_wk2114_oled_refresh(uint8_t dirty_mask)
{
    uint32_t line_index;

    if (!g_mine_wk2114_oled_ready) {
        return;
    }

    for (line_index = 0; line_index < MINE_WK2114_OLED_LINE_COUNT; line_index++) {
        if ((dirty_mask & (uint8_t)(1U << line_index)) == 0U) {
            continue;
        }
        SSD1306_ShowStr(0, (uint8_t)line_index, g_mine_wk2114_oled_lines[line_index], TEXT_SIZE_8);
    }
}

/**
 * @brief 刷新当前累计脏行到 OLED。
 */
void mine_wk2114_oled_flush_pending(void)
{
    uint8_t dirty_mask;

    if ((!g_mine_wk2114_oled_ready) || (!g_mine_wk2114_oled_dirty)) {
        return;
    }

    dirty_mask = g_mine_wk2114_oled_dirty_mask;
    g_mine_wk2114_oled_dirty_mask = 0;
    g_mine_wk2114_oled_dirty = false;

    if (dirty_mask == 0U) {
        return;
    }

    mine_wk2114_oled_refresh(dirty_mask);
}

/**
 * @brief 直接设置 OLED 指定行文本（超长自动截断）。
 *
 * @param line_index 行号。
 * @param text       文本内容。
 */
static void mine_wk2114_oled_set_line_raw(uint32_t line_index, const char *text)
{
    uint32_t char_index;
    char new_line[MINE_WK2114_OLED_LINE_CHARS + 1] = {0};
    char *target_line;

    if (line_index >= MINE_WK2114_OLED_LINE_COUNT) {
        return;
    }

    mine_wk2114_oled_clear_line(new_line);

    if (text != NULL) {
        for (char_index = 0; char_index < MINE_WK2114_OLED_LINE_CHARS; char_index++) {
            if (text[char_index] == '\0') {
                break;
            }
            new_line[char_index] = text[char_index];
        }
    }

    target_line = g_mine_wk2114_oled_lines[line_index];
    if (memcmp(target_line, new_line, sizeof(new_line)) == 0) {
        return;
    }

    if (memcpy_s(target_line, MINE_WK2114_OLED_LINE_CHARS + 1, new_line, sizeof(new_line)) != EOK) {
        return;
    }

    mine_wk2114_oled_mark_line_dirty(line_index);
}

/**
 * @brief 按格式化字符串更新 OLED 行。
 *
 * @param line_index 行号。
 * @param fmt        格式串。
 */
static void mine_wk2114_oled_set_linef(uint32_t line_index, const char *fmt, ...)
{
    char line_buf[MINE_WK2114_OLED_EVENT_BUFFER_LEN] = {0};
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

    mine_wk2114_oled_set_line_raw(line_index, line_buf);
}

/**
 * @brief 将连续预览文本拆分为三行显示。
 *
 * @param text 连续文本。
 */
static void mine_wk2114_oled_set_data_lines(const char *text)
{
    const uint32_t data_lines[MINE_WK2114_OLED_DATA_LINE_COUNT] = {
        MINE_WK2114_OLED_LINE_DATA0,
        MINE_WK2114_OLED_LINE_DATA1,
        MINE_WK2114_OLED_LINE_DATA2,
    };
    uint32_t line_idx;
    uint32_t text_len;

    if (text == NULL) {
        for (line_idx = 0; line_idx < MINE_WK2114_OLED_DATA_LINE_COUNT; line_idx++) {
            mine_wk2114_oled_set_line_raw(data_lines[line_idx], "");
        }
        return;
    }

    text_len = (uint32_t)strlen(text);
    for (line_idx = 0; line_idx < MINE_WK2114_OLED_DATA_LINE_COUNT; line_idx++) {
        uint32_t offset = line_idx * MINE_WK2114_OLED_LINE_CHARS;
        uint32_t char_index;
        char line_buf[MINE_WK2114_OLED_LINE_CHARS + 1] = {0};

        if (offset >= text_len) {
            mine_wk2114_oled_set_line_raw(data_lines[line_idx], "");
            continue;
        }

        for (char_index = 0; char_index < MINE_WK2114_OLED_LINE_CHARS; char_index++) {
            char current_char = text[offset + char_index];
            if (current_char == '\0') {
                break;
            }
            line_buf[char_index] = current_char;
        }

        mine_wk2114_oled_set_line_raw(data_lines[line_idx], line_buf);
    }
}

/**
 * @brief 更新 OLED 状态文本。
 *
 * @param text 状态文本。
 */
void mine_wk2114_oled_push_state(const char *text)
{
    if ((!g_mine_wk2114_oled_ready) || (text == NULL)) {
        return;
    }

    mine_wk2114_oled_set_linef(MINE_WK2114_OLED_LINE_STATE, "STATE:%s", text);
}

/**
 * @brief 更新 OLED 当前通道/波特率行。
 *
 * @param channel   子串口号。
 * @param baud_rate 波特率。
 */
void mine_wk2114_oled_set_channel(uint8_t channel, uint32_t baud_rate)
{
    if (!g_mine_wk2114_oled_ready) {
        return;
    }

    if ((channel < MINE_WK2114_SUBUART_MIN) || (channel > MINE_WK2114_SUBUART_MAX)) {
        mine_wk2114_oled_set_linef(MINE_WK2114_OLED_LINE_CFG, "CH:-- B:%lu", (unsigned long)baud_rate);
        return;
    }

    mine_wk2114_oled_set_linef(MINE_WK2114_OLED_LINE_CFG, "CH:U%u B:%lu",
        (unsigned int)channel, (unsigned long)baud_rate);
}

/**
 * @brief 上报一条收发事件到 OLED（计数与内容预览）。
 *
 * @param prefix 方向前缀（含 RX/TX）。
 * @param data   数据缓冲区。
 * @param len    数据长度。
 */
void mine_wk2114_oled_push_data_event(const char *prefix, const uint8_t *data, uint16_t len)
{
    char preview_line[MINE_WK2114_OLED_DATA_TOTAL_CHARS + 1] = {0};
    char direction = '?';
    uint16_t preview_len;
    uint16_t max_payload_chars;
    uint32_t prefix_len = 0;
    uint16_t idx;
    int32_t append_len;

    if ((!g_mine_wk2114_oled_ready) || (prefix == NULL)) {
        return;
    }

    if (strstr(prefix, "RX") != NULL) {
        direction = 'R';
        g_mine_wk2114_oled_rx_last_len = len;
        g_mine_wk2114_oled_rx_count++;
    } else if (strstr(prefix, "TX") != NULL) {
        direction = 'T';
        g_mine_wk2114_oled_tx_last_len = len;
        g_mine_wk2114_oled_tx_count++;
    }

    mine_wk2114_oled_set_linef(MINE_WK2114_OLED_LINE_RX, "RX:%u C:%lu",
        g_mine_wk2114_oled_rx_last_len, (unsigned long)g_mine_wk2114_oled_rx_count);
    mine_wk2114_oled_set_linef(MINE_WK2114_OLED_LINE_TX, "TX:%u C:%lu",
        g_mine_wk2114_oled_tx_last_len, (unsigned long)g_mine_wk2114_oled_tx_count);

    if ((data == NULL) || (len == 0)) {
        (void)snprintf_s(preview_line, sizeof(preview_line),
            sizeof(preview_line) - 1, "DATA:%c --", direction);
        mine_wk2114_oled_set_data_lines(preview_line);
        return;
    }

    append_len = snprintf_s(preview_line, sizeof(preview_line),
        sizeof(preview_line) - 1, "DATA:%c ", direction);
    if (append_len > 0) {
        prefix_len = (uint32_t)append_len;
    }

    if (prefix_len >= MINE_WK2114_OLED_DATA_TOTAL_CHARS) {
        mine_wk2114_oled_set_data_lines(preview_line);
        return;
    }

    max_payload_chars = (uint16_t)(MINE_WK2114_OLED_DATA_TOTAL_CHARS - prefix_len);
    preview_len = len;
    if (preview_len > max_payload_chars) {
        if (max_payload_chars > 3) {
            preview_len = (uint16_t)(max_payload_chars - 3);
        } else {
            preview_len = max_payload_chars;
        }
    }

    for (idx = 0; idx < preview_len; idx++) {
        char current_char = (char)data[idx];
        if ((current_char < ' ') || (current_char > '~')) {
            current_char = '.';
        }
        if ((prefix_len + idx) >= (sizeof(preview_line) - 1)) {
            break;
        }
        preview_line[prefix_len + idx] = current_char;
    }
    prefix_len += preview_len;

    if ((len > preview_len) && (max_payload_chars > 3) && (prefix_len + 3 < sizeof(preview_line))) {
        preview_line[prefix_len++] = '.';
        preview_line[prefix_len++] = '.';
        preview_line[prefix_len++] = '.';
    }

    preview_line[prefix_len] = '\0';
    mine_wk2114_oled_set_data_lines(preview_line);
}

/**
 * @brief 初始化 WK2114 模块 OLED 页面。
 */
void mine_wk2114_oled_init(void)
{
    uint32_t line_index;
    uint32_t ret;

    if (g_mine_wk2114_oled_ready) {
        return;
    }

    ret = SSD1306_Init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] oled init failed, ret=%x\\r\\n", ret);
        return;
    }

    SSD1306_CLS();
    for (line_index = 0; line_index < MINE_WK2114_OLED_LINE_COUNT; line_index++) {
        mine_wk2114_oled_clear_line(g_mine_wk2114_oled_lines[line_index]);
    }

    g_mine_wk2114_oled_rx_count = 0;
    g_mine_wk2114_oled_tx_count = 0;
    g_mine_wk2114_oled_rx_last_len = 0;
    g_mine_wk2114_oled_tx_last_len = 0;
    g_mine_wk2114_oled_dirty_mask = 0;

    g_mine_wk2114_oled_ready = true;
    mine_wk2114_oled_set_line_raw(MINE_WK2114_OLED_LINE_TITLE, "WK2114 UART2 EXT");
    mine_wk2114_oled_set_line_raw(MINE_WK2114_OLED_LINE_STATE, "STATE:BOOT");
    mine_wk2114_oled_set_data_lines("DATA:--");
    mine_wk2114_oled_set_channel(0, MINE_WK2114_HOST_UART_BAUD);
    mine_wk2114_oled_set_line_raw(MINE_WK2114_OLED_LINE_RX, "RX:0 C:0");
    mine_wk2114_oled_set_line_raw(MINE_WK2114_OLED_LINE_TX, "TX:0 C:0");
    mine_wk2114_oled_flush_pending();
}
