/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Slave side UART0 <-> SLE bridge.
 */

#include "sle_uart_slave.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "app_init.h"
#include "common_def.h"
#include "mac_addr.h"
#include "pinctrl.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"

#include "LD2402/LD2402.h"
#include "ssd1306/hal_bsp_ssd1306.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/* ========================= 参数配置 ========================= */

/* UART 接收缓存长度。 */
#define MINE_UART_RX_BUFFER_SIZE 512

/* UART 总线数量（UART0/1/2）。 */
#define MINE_UART_BUS_COUNT 3

/* 非法 UART 标记。 */
#define MINE_UART_BUS_INVALID 0xFF

/* SLE 默认 MTU。 */
#define MINE_SLE_DEFAULT_MTU_SIZE 512

/* 单次写命令发送的数据分片长度。 */
#define MINE_SLE_SAFE_CHUNK_LEN 200

/* 扫描参数。 */
#define MINE_SLE_SEEK_INTERVAL_DEFAULT 100
#define MINE_SLE_SEEK_WINDOW_DEFAULT 100

/* UUID 解析用索引（16 位 UUID 映射在 [14],[15]）。 */
#define MINE_UUID_BASE_INDEX_14 14
#define MINE_UUID_BASE_INDEX_15 15

/* 初始化延迟。 */
#define MINE_INIT_DELAY_MS 500

/* 从机主任务轮询 OLED 脏标记的等待时间（毫秒）。 */
#define MINE_TASK_LOOP_WAIT_MS 80

/* OLED 文本显示配置：0.96 寸 128x64 屏，在 8 像素字体下共 8 行、每行约 21 字符。 */
#define MINE_OLED_LINE_COUNT 8
#define MINE_OLED_LINE_CHARS 21
#define MINE_OLED_DATA_LINE_COUNT 3
#define MINE_OLED_DATA_TOTAL_CHARS (MINE_OLED_LINE_CHARS * MINE_OLED_DATA_LINE_COUNT)
#define MINE_OLED_FONT_SIZE TEXT_SIZE_8
#define MINE_OLED_DATA_PREVIEW_BYTES 4
#define MINE_OLED_EVENT_BUFFER_LEN 64

/* OLED 固定信息面板行定义 */
#define MINE_OLED_LINE_TITLE 0
#define MINE_OLED_LINE_STATE 1
#define MINE_OLED_LINE_DATA0 2
#define MINE_OLED_LINE_DATA1 3
#define MINE_OLED_LINE_DATA2 4
#define MINE_OLED_LINE_UUID 5
#define MINE_OLED_LINE_RX 6
#define MINE_OLED_LINE_TX 7

/* 日志镜像缓冲区长度。 */
#define MINE_LOG_BUFFER_LEN 192

/* SLE MAC 地址长度。 */
#define MINE_SLE_MAC_ADDR_LEN 6

/* LD2402 状态缓存长度。 */
#define MINE_LD2402_STATUS_TEXT_LEN 48
#define MINE_LD2402_SN_MAX_LEN 16

/* ========================= 全局状态区 ========================= */

/* 多 UART 接收缓存（按 bus 索引 0/1/2）。 */
static uint8_t g_mine_uart_rx_buffer[MINE_UART_BUS_COUNT][MINE_UART_RX_BUFFER_SIZE] = {0};

/* UART 回调 -> 任务 的消息队列。 */
static unsigned long g_mine_uart_msg_queue = 0;
static unsigned int g_mine_uart_msg_size = sizeof(mine_sle_uart_slave_msg_t);

/* 连接与发现状态。 */
static volatile uint16_t g_mine_conn_id = 0;
static volatile bool g_mine_peer_connected = false;
static volatile bool g_mine_property_ready = false;

/* 扫描/连接流程防抖状态，避免重复 stop seek / connect 触发。 */
static bool g_mine_seek_started = false;
static bool g_mine_seek_stop_pending = false;
static bool g_mine_connecting_pending = false;

/* 目标设备地址（扫描命中后保存）。 */
static sle_addr_t g_mine_remote_addr = {0};

/* 写命令参数模板（发现到可写特征后填充 handle/type）。 */
static ssapc_write_param_t g_mine_write_param = {0};

/* 当芯片没有烧录 SLE MAC 时，Slave 侧使用该回退地址保障 demo 可用。 */
static const uint8_t g_mine_slave_fallback_sle_mac[MINE_SLE_MAC_ADDR_LEN] = MINE_SLAVE_FALLBACK_SLE_MAC;

#if MINE_LD2402_ENABLE
static LD2402_Handle_t g_ld2402_handle;
static bool g_ld2402_ready = false;
static uart_bus_t g_ld2402_bus = MINE_LD2402_UART_BUS;
static volatile bool g_ld2402_status_dirty = false;
static char g_ld2402_status_text[MINE_LD2402_STATUS_TEXT_LEN] = "RADAR:OFF";
#endif

/* OLED 运行状态与显示缓冲。 */
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

/* 各模块回调结构体。 */
static sle_announce_seek_callbacks_t g_mine_seek_cbks = {0};
static sle_connection_callbacks_t g_mine_conn_cbks = {0};
static ssapc_callbacks_t g_mine_ssapc_cbks = {0};

/* 保存 OSAL 原始日志函数入口，用于“osal_printk + PRINT”双通道镜像输出。 */
static void (*g_mine_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/**
 * @brief 同时输出到 OSAL 日志口与 APP 调试口。
 */
static void mine_dual_log(const char *fmt, ...)
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
    PRINT("%s", log_buf);
}

#define osal_printk mine_dual_log

/* ========================= 工具函数区 ========================= */

/**
 * @brief 判断指定 UART 总线是否启用。
 */
static bool mine_uart_bus_enabled(uart_bus_t bus)
{
    uint32_t bus_index = (uint32_t)bus;
    if (bus_index >= MINE_UART_BUS_COUNT) {
        return false;
    }
    return ((MINE_UART_ENABLE_MASK & (1U << bus_index)) != 0U);
}

/**
 * @brief 返回 UART 文本标识（用于日志/OLED）。
 */
static const char *mine_uart_bus_name(uint8_t bus)
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

/**
 * @brief 返回 UART 标识字符（0/1/2，未知为 *）。
 */
static char mine_uart_bus_mark(uint8_t bus)
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
 * @brief 把一行 OLED 文本缓冲重置为“空格填充 + 字符串结尾”。
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
 * @brief 按脏行掩码刷新 OLED。
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
 * @brief 标记指定 OLED 行为脏行，等待 mine 任务线程统一刷新。
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
 * @brief 在 mine 任务线程中刷新 OLED（避免在 bt_service 回调里直接走 I2C）。
 */
static void mine_oled_flush_pending(void)
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
 * @brief 写入 OLED 指定行（固定面板模式，不滚动）。
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
 * @brief 使用格式化字符串写入 OLED 指定行。
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
 * @brief 更新状态行（固定行：STATE）。
 */
static void mine_oled_push_state(const char *text)
{
    if ((!g_mine_oled_ready) || (text == NULL)) {
        return;
    }

    mine_oled_set_linef(MINE_OLED_LINE_STATE, "STATE:%s", text);
}

/**
 * @brief 将 DATA 文本拆分到 3 行显示，提升数据可见长度。
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
 * @brief 把数据事件（方向+长度+前几个字节）输出到 OLED。
 */
static void mine_oled_push_data_event(uint8_t uart_bus, const char *prefix, const uint8_t *data, uint16_t len)
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

    uart_mark = mine_uart_bus_mark(uart_bus);

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
        mine_uart_bus_mark(g_mine_oled_rx_last_uart),
        g_mine_oled_rx_last_len, (unsigned long)g_mine_oled_rx_count);
    mine_oled_set_linef(MINE_OLED_LINE_TX, "TX%c:%u C:%lu",
        mine_uart_bus_mark(g_mine_oled_tx_last_uart),
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
 * @brief 初始化 OLED，并显示从机固定信息面板。
 */
static void mine_oled_init(void)
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
    mine_oled_set_linef(MINE_OLED_LINE_UUID, "UUID:S%04X P%04X",
        MINE_SLE_UART_SERVICE_UUID, MINE_SLE_UART_PROPERTY_UUID);
    mine_oled_set_line_raw(MINE_OLED_LINE_RX, "RX*:0 C:0");
    mine_oled_set_line_raw(MINE_OLED_LINE_TX, "TX*:0 C:0");
    mine_oled_flush_pending();
}

/**
 * @brief 把接收到的 SLE 数据输出到所有启用的 UART。
 */
static void mine_uart_write_enabled_buses(const uint8_t *data, uint16_t len)
{
    uint8_t bus_index;
    int32_t write_ret;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }

        write_ret = uapi_uart_write((uart_bus_t)bus_index, data, len, 0);
        if (write_ret < 0) {
            osal_printk("[mine slave] %s write failed, ret=%d\r\n",
                mine_uart_bus_name(bus_index), (int)write_ret);
        }
    }
}

/**
 * @brief 判断扫描结果数据中是否包含目标广播名。
 *
 * @param data     广播原始字节流。
 * @param data_len 字节流长度。
 * @param name     目标名称字符串。
 * @return true  命中。
 * @return false 未命中。
 */
static bool mine_adv_data_contains_name(const uint8_t *data, uint8_t data_len, const char *name)
{
    size_t name_len;
    uint8_t i;

    if ((data == NULL) || (name == NULL) || (data_len == 0)) {
        return false;
    }

    name_len = strlen(name);
    if ((name_len == 0) || (data_len < name_len)) {
        return false;
    }

    for (i = 0; i + name_len <= data_len; i++) {
        if (memcmp(&data[i], name, name_len) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 从 sle_uuid_t 中提取 16 位 UUID。
 *
 * @param uuid uuid 结构。
 * @return uint16_t 16 位 UUID，提取失败时返回 0。
 */
static uint16_t mine_get_uuid_u16(const sle_uuid_t *uuid)
{
    if ((uuid == NULL) || (uuid->len != 2)) {
        return 0;
    }

    return (uint16_t)(((uint16_t)uuid->uuid[MINE_UUID_BASE_INDEX_15] << 8) |
        uuid->uuid[MINE_UUID_BASE_INDEX_14]);
}

/**
 * @brief 准备 SLE 本地 MAC。
 *
 * 场景：若系统未从 NV/EFUSE 读到 SLE MAC（日志中会出现 init_sle_mac failed），
 *       则写入 demo 回退地址，避免 enable_sle 失败。
 */
static void mine_prepare_sle_mac(void)
{
    uint8_t sle_mac[SLE_ADDR_LEN] = {0};
    errcode_t ret;

    ret = get_dev_addr(sle_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[mine slave] use system sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            sle_mac[0], sle_mac[1], sle_mac[2], sle_mac[3], sle_mac[4], sle_mac[5]);
        return;
    }

    ret = set_dev_addr(g_mine_slave_fallback_sle_mac, MINE_SLE_MAC_ADDR_LEN, IFTYPE_SLE);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[mine slave] set fallback sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            g_mine_slave_fallback_sle_mac[0], g_mine_slave_fallback_sle_mac[1], g_mine_slave_fallback_sle_mac[2],
            g_mine_slave_fallback_sle_mac[3], g_mine_slave_fallback_sle_mac[4], g_mine_slave_fallback_sle_mac[5]);
    } else {
        osal_printk("[mine slave] set fallback sle mac failed, ret:%x\r\n", ret);
    }
}

/**
 * @brief 按 samples/bt/sle 方式显式设置本地地址/本地名称。
 *
 * 说明：客户端样例常见流程为 enable_sle 后调用 sle_set_local_addr。
 */
static errcode_t mine_apply_local_addr_and_name(void)
{
    sle_addr_t local_addr = {0};
    uint8_t local_mac[SLE_ADDR_LEN] = {0};
    errcode_t ret;

    ret = get_dev_addr(local_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    if (ret != ERRCODE_SUCC) {
        if (memcpy_s(local_mac, sizeof(local_mac), g_mine_slave_fallback_sle_mac,
            MINE_SLE_MAC_ADDR_LEN) != EOK) {
            return ERRCODE_SLE_FAIL;
        }
    }

    local_addr.type = 0;
    if (memcpy_s(local_addr.addr, SLE_ADDR_LEN, local_mac, SLE_ADDR_LEN) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    ret = sle_set_local_addr(&local_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] sle_set_local_addr failed:%x\r\n", ret);
        return ret;
    }

    ret = sle_set_local_name((const uint8_t *)MINE_SLE_UART_SLAVE_NAME,
        (uint8_t)strlen(MINE_SLE_UART_SLAVE_NAME));
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] sle_set_local_name failed:%x\r\n", ret);
        return ret;
    }

    return ERRCODE_SLE_SUCCESS;
}

#if MINE_LD2402_ENABLE
static int ld2402_uart_send(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return -1;
    }

    if (uapi_uart_write(g_ld2402_bus, data, len, 0) < 0) {
        return -1;
    }

    return 0;
}

static uint32_t ld2402_get_tick_ms(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

static void ld2402_delay_ms(uint32_t ms)
{
    (void)osal_msleep(ms);
}

static void ld2402_set_status(const char *text)
{
    if (text == NULL) {
        return;
    }

    if (snprintf_s(g_ld2402_status_text, sizeof(g_ld2402_status_text),
        sizeof(g_ld2402_status_text) - 1, "%s", text) > 0) {
        g_ld2402_status_dirty = true;
    }
}

static void ld2402_data_callback(LD2402_DataFrame_t *data)
{
    if (data == NULL) {
        return;
    }

    if (snprintf_s(g_ld2402_status_text, sizeof(g_ld2402_status_text),
        sizeof(g_ld2402_status_text) - 1, "RADAR:S%u D:%u",
        (unsigned int)data->status, (unsigned int)data->distance_cm) > 0) {
        g_ld2402_status_dirty = true;
    }
}

static bool ld2402_init(uart_bus_t bus)
{
    LD2402_HAL_t hal = {0};
    char version[24] = {0};
    uint8_t sn_buf[MINE_LD2402_SN_MAX_LEN] = {0};
    int sn_len;

    g_ld2402_ready = false;
    g_ld2402_bus = bus;

    if (!mine_uart_bus_enabled(bus)) {
        ld2402_set_status("RADAR:BUS OFF");
        return false;
    }

    hal.uart_send = ld2402_uart_send;
    hal.get_tick_ms = ld2402_get_tick_ms;
    hal.delay_ms = ld2402_delay_ms;
    hal.uart_rx_irq_ctrl = NULL;

    LD2402_Init(&g_ld2402_handle, &hal);
    g_ld2402_handle.on_data_received = ld2402_data_callback;
    g_ld2402_ready = true;

    if (LD2402_GetVersion(&g_ld2402_handle, version, sizeof(version)) == 0) {
        osal_printk("[mine slave] ld2402 version:%s\r\n", version);
        ld2402_set_status("RADAR:READY");
    } else {
        osal_printk("[mine slave] ld2402 version query timeout\r\n");
        ld2402_set_status("RADAR:NO ACK");
        return false;
    }

    sn_len = LD2402_GetSN_Hex(&g_ld2402_handle, sn_buf, sizeof(sn_buf));
    if (sn_len > 0) {
        osal_printk("[mine slave] ld2402 sn(hex) len:%d\r\n", sn_len);
    }

    return true;
}

static void ld2402_feed(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    uint16_t idx;

    if ((!g_ld2402_ready) || (bus != g_ld2402_bus) || (data == NULL) || (len == 0)) {
        return;
    }

    for (idx = 0; idx < len; idx++) {
        LD2402_InputByte(&g_ld2402_handle, data[idx]);
    }
}

static bool ld2402_get_status(char *buf, uint16_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0) || (!g_ld2402_status_dirty)) {
        return false;
    }

    if (snprintf_s(buf, buf_len, buf_len - 1, "%s", g_ld2402_status_text) <= 0) {
        return false;
    }

    g_ld2402_status_dirty = false;
    return true;
}
#endif

/**
 * @brief 将 UART 数据分片后通过 SLE 写命令发给 Host。
 *
 * @param data 数据指针。
 * @param len  数据长度。
 * @return errcode_t
 */
static errcode_t mine_sle_uart_slave_send_to_host(const mine_sle_uart_slave_msg_t *msg)
{
    uint16_t offset = 0;
    uint16_t chunk_len;
    uint16_t conn_id_snapshot;
    uint16_t property_handle_snapshot;
    uint8_t uart_bus_snapshot;
    bool peer_connected_snapshot;
    bool property_ready_snapshot;
    errcode_t ret;

    if ((msg == NULL) || (msg->value == NULL) || (msg->value_len == 0)) {
        osal_printk("[mine slave] invalid uart->sle msg\r\n");
        mine_oled_push_state("SEND INVALID");
        return ERRCODE_SLE_FAIL;
    }

    peer_connected_snapshot = g_mine_peer_connected;
    property_ready_snapshot = g_mine_property_ready;
    conn_id_snapshot = g_mine_conn_id;
    property_handle_snapshot = g_mine_write_param.handle;
    uart_bus_snapshot = msg->uart_bus;

    if ((!peer_connected_snapshot) || (!property_ready_snapshot) || (property_handle_snapshot == 0)) {
        osal_printk("[mine slave] drop %s data, link:%u prop:%u cid:%u handle:%u\r\n",
            mine_uart_bus_name(uart_bus_snapshot),
            (unsigned int)peer_connected_snapshot, (unsigned int)property_ready_snapshot,
            conn_id_snapshot, property_handle_snapshot);
        mine_oled_push_state("SEND DROP");
        return ERRCODE_SLE_FAIL;
    }

    while (offset < msg->value_len) {
        chunk_len = (uint16_t)((msg->value_len - offset) > MINE_SLE_SAFE_CHUNK_LEN ?
            MINE_SLE_SAFE_CHUNK_LEN : (msg->value_len - offset));

        g_mine_write_param.data = (uint8_t *)(msg->value + offset);
        g_mine_write_param.data_len = chunk_len;

        ret = ssapc_write_cmd(0, conn_id_snapshot, &g_mine_write_param);
        if (ret != ERRCODE_SLE_SUCCESS) {
            osal_printk("[mine slave] write cmd failed, ret=%x\r\n", ret);
            mine_oled_push_state("SEND FAIL");
            return ret;
        }

        offset = (uint16_t)(offset + chunk_len);
    }

    osal_printk("[mine slave] %s->sle write len:%u\r\n",
        mine_uart_bus_name(uart_bus_snapshot), msg->value_len);
    mine_oled_push_data_event(uart_bus_snapshot, "UART TX", msg->value, msg->value_len);

    return ERRCODE_SLE_SUCCESS;
}

/* ========================= UART 模块 ========================= */

/**
 * @brief UART 接收回调公共处理（中断上下文）。
 *
 * @param bus    UART 总线号。
 * @param buffer 接收数据指针。
 * @param length 接收数据长度。
 * @param error  串口错误标记。
 */
static void mine_sle_uart_slave_read_handler_common(uart_bus_t bus, const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_msg_t msg = {0};
    void *buffer_copy = NULL;
    int write_ret;

    unused(error);

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

#if MINE_LD2402_ENABLE
    ld2402_feed(bus, (const uint8_t *)buffer, length);
#endif

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

static void mine_sle_uart_slave_read_handler_uart0(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_read_handler_common(UART_BUS_0, buffer, length, error);
}

static void mine_sle_uart_slave_read_handler_uart1(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_read_handler_common(UART_BUS_1, buffer, length, error);
}

static void mine_sle_uart_slave_read_handler_uart2(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_slave_read_handler_common(UART_BUS_2, buffer, length, error);
}

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
    uart_rx_callback_t rx_cb = NULL;
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
        osal_printk("[mine slave] %s pin not configured, skip\r\n", mine_uart_bus_name(bus_index));
        return false;
    }

    uart_buffer_cfg.rx_buffer = g_mine_uart_rx_buffer[bus_index];
    uart_buffer_cfg.rx_buffer_size = MINE_UART_RX_BUFFER_SIZE;

    uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
    uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

    (void)uapi_uart_deinit(bus);
    ret = uapi_uart_init(bus, &pin_cfg, &attr, NULL, &uart_buffer_cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] %s init failed, ret=%x\r\n", mine_uart_bus_name(bus_index), ret);
        return false;
    }

    ret = uapi_uart_register_rx_callback(bus, UART_RX_CONDITION_MASK_IDLE, 1, rx_cb);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] %s rx cb failed, ret=%x\r\n", mine_uart_bus_name(bus_index), ret);
        return false;
    }

    return true;
}

void mine_sle_uart_slave_uart_init(void)
{
    uint8_t bus_index;
    uint8_t enabled_count = 0;
    uint8_t ok_count = 0;

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }
        enabled_count++;
        if (mine_sle_uart_slave_uart_init_one((uart_bus_t)bus_index)) {
            ok_count++;
        }
    }

    osal_printk("[mine slave] uart init summary, enabled:%u ok:%u\r\n", enabled_count, ok_count);
    if (ok_count > 0) {
        mine_oled_push_state("UART INIT OK");
    } else {
        mine_oled_push_state("UART INIT FAIL");
    }
}

/* ========================= 扫描与连接管理 ========================= */

void mine_sle_uart_slave_start_scan(void)
{
    sle_seek_param_t seek_param = {0};
    errcode_t ret;

    if (g_mine_peer_connected || g_mine_connecting_pending) {
        return;
    }

    if (g_mine_seek_started || g_mine_seek_stop_pending) {
        return;
    }

    seek_param.own_addr_type = 0;
    seek_param.filter_duplicates = 0;
    seek_param.seek_filter_policy = 0;
    seek_param.seek_phys = 1;
    seek_param.seek_type[0] = 1;
    seek_param.seek_interval[0] = MINE_SLE_SEEK_INTERVAL_DEFAULT;
    seek_param.seek_window[0] = MINE_SLE_SEEK_WINDOW_DEFAULT;

    ret = sle_set_seek_param(&seek_param);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] set seek param failed:%x\r\n", ret);
        mine_oled_push_state("SCAN PARAM FAIL");
        return;
    }

    ret = sle_start_seek();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] start seek failed:%x\r\n", ret);
        mine_oled_push_state("SCAN START FAIL");
        return;
    }

    g_mine_seek_started = true;
    g_mine_seek_stop_pending = false;
    mine_oled_push_state("SCANNING");
    osal_printk("[mine slave] start scan, interval:%u window:%u\r\n",
        MINE_SLE_SEEK_INTERVAL_DEFAULT, MINE_SLE_SEEK_WINDOW_DEFAULT);
}

/**
 * @brief SLE 使能完成回调。
 */
static void mine_sle_enable_cb(errcode_t status)
{
    if (status == ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] sle enabled\r\n");
        mine_oled_push_state("SLE ON");
    } else {
        osal_printk("[mine slave] sle enable failed:%x\r\n", status);
        mine_oled_push_state("SLE FAIL");
    }
}

/**
 * @brief 扫描使能回调。
 */
static void mine_seek_enable_cb(errcode_t status)
{
    if (status != ERRCODE_SLE_SUCCESS) {
        g_mine_seek_started = false;
        osal_printk("[mine slave] seek enable failed:%x\r\n", status);
        mine_oled_push_state("SCAN FAIL");
    } else {
        g_mine_seek_started = true;
        osal_printk("[mine slave] seek enabled\r\n");
        mine_oled_push_state("SCAN ON");
    }
}

/**
 * @brief 扫描结果回调。
 *
 * @param seek_result_data 扫描到的设备信息。
 */
static void mine_seek_result_cb(sle_seek_result_info_t *seek_result_data)
{
    errcode_t ret;

    if ((seek_result_data == NULL) || (seek_result_data->data == NULL) || (seek_result_data->data_length == 0)) {
        return;
    }

    if (g_mine_seek_stop_pending || g_mine_connecting_pending || g_mine_peer_connected) {
        return;
    }

    if (mine_adv_data_contains_name(seek_result_data->data, seek_result_data->data_length, MINE_SLE_UART_HOST_NAME)) {
        if (memcpy_s(&g_mine_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t)) != EOK) {
            return;
        }

        g_mine_seek_stop_pending = true;
        osal_printk("[mine slave] found target adv, stop seek\r\n");
        mine_oled_push_state("TARGET FOUND");

        ret = sle_stop_seek();
        if (ret != ERRCODE_SLE_SUCCESS) {
            g_mine_seek_stop_pending = false;
            osal_printk("[mine slave] stop seek failed:%x\r\n", ret);
            mine_oled_push_state("STOP FAIL");
        }
    }
}

/**
 * @brief 扫描停止回调。
 */
static void mine_seek_disable_cb(errcode_t status)
{
    errcode_t ret;

    g_mine_seek_started = false;

    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] seek disable failed:%x\r\n", status);
        mine_oled_push_state("STOP SCAN FAIL");
        g_mine_seek_stop_pending = false;
        return;
    }

    if (!g_mine_seek_stop_pending) {
        osal_printk("[mine slave] seek stopped (no target)\r\n");
        return;
    }

    g_mine_seek_stop_pending = false;
    g_mine_connecting_pending = true;

    osal_printk("[mine slave] seek stopped, try connect\r\n");
    mine_oled_push_state("CONNECTING");
    ret = sle_remove_paired_remote_device(&g_mine_remote_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] remove pair failed:%x\r\n", ret);
    }

    ret = sle_connect_remote_device(&g_mine_remote_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        g_mine_connecting_pending = false;
        osal_printk("[mine slave] connect request failed:%x\r\n", ret);
        mine_oled_push_state("CONN REQ FAIL");
        mine_sle_uart_slave_start_scan();
    }
}

/**
 * @brief 连接状态回调。
 */
static void mine_connect_state_changed_cb(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    unused(disc_reason);

    if (addr != NULL) {
        osal_printk("[mine slave] remote:%02x:**:**:**:%02x:%02x state:%x\r\n",
            addr->addr[0], addr->addr[4], addr->addr[5], conn_state);
    }

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_mine_seek_started = false;
        g_mine_seek_stop_pending = false;
        g_mine_connecting_pending = false;
        g_mine_conn_id = conn_id;
        g_mine_peer_connected = true;
        osal_printk("[mine slave] connected, conn_id:%u pair_state:%u\r\n", conn_id, pair_state);
        if (pair_state == SLE_PAIR_NONE) {
            osal_printk("[mine slave] start pair\r\n");
            mine_oled_push_state("PAIRING");
            (void)sle_pair_remote_device(&g_mine_remote_addr);
        } else {
            mine_oled_push_state("CONNECTED");
        }
        return;
    }

    if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_mine_seek_started = false;
        g_mine_seek_stop_pending = false;
        g_mine_connecting_pending = false;
        g_mine_conn_id = 0;
        g_mine_peer_connected = false;
        g_mine_property_ready = false;
        g_mine_write_param.handle = 0;
        osal_printk("[mine slave] disconnected, restart scan\r\n");
        mine_oled_set_linef(MINE_OLED_LINE_UUID, "UUID:S%04X P%04X",
            MINE_SLE_UART_SERVICE_UUID, MINE_SLE_UART_PROPERTY_UUID);
        mine_oled_push_state("DISCONN SCAN");
        mine_sle_uart_slave_start_scan();
    }
}

/**
 * @brief 配对完成回调。
 */
static void mine_pair_complete_cb(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    ssap_exchange_info_t exchange_info = {0};

    unused(addr);

    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] pair failed:%x\r\n", status);
        mine_oled_push_state("PAIR FAIL");
        return;
    }

    osal_printk("[mine slave] pair ok, request exchange mtu\r\n");
    mine_oled_push_state("PAIR OK");

    exchange_info.mtu_size = MINE_SLE_DEFAULT_MTU_SIZE;
    exchange_info.version = 1;
    (void)ssapc_exchange_info_req(0, conn_id, &exchange_info);
}

/* ========================= SSAPC（客户端）回调 ========================= */

/**
 * @brief MTU 交换完成回调。
 */
static void mine_exchange_info_cb(uint8_t client_id, uint16_t conn_id,
    ssap_exchange_info_t *param, errcode_t status)
{
    ssapc_find_structure_param_t find_param = {0};

    unused(client_id);

    if ((status != ERRCODE_SLE_SUCCESS) || (param == NULL)) {
        osal_printk("[mine slave] exchange info failed:%x\r\n", status);
        mine_oled_push_state("MTU FAIL");
        return;
    }

    osal_printk("[mine slave] exchange ok, mtu:%u\r\n", param->mtu_size);
    mine_oled_push_state("MTU OK");

    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    (void)ssapc_find_structure(0, conn_id, &find_param);
}

/**
 * @brief 发现结构回调（本 demo 仅打印日志，可用于教学观察流程）。
 */
static void mine_find_structure_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_service_result_t *service, errcode_t status)
{
    unused(client_id);
    unused(conn_id);

    if ((status == ERRCODE_SLE_SUCCESS) && (service != NULL)) {
        osal_printk("[mine slave] find service start:%u end:%u\r\n", service->start_hdl, service->end_hdl);
    }
}

/**
 * @brief 发现特征回调。
 *
 * @param client_id 客户端 ID。
 * @param conn_id   连接 ID。
 * @param property  发现到的特征信息。
 * @param status    回调状态。
 */
static void mine_find_property_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    uint16_t uuid16;

    unused(client_id);
    unused(conn_id);

    if ((status != ERRCODE_SLE_SUCCESS) || (property == NULL)) {
        return;
    }

    uuid16 = mine_get_uuid_u16(&property->uuid);
    osal_printk("[mine slave] find property handle:%u uuid:0x%04x\r\n", property->handle, uuid16);
    if (uuid16 == MINE_SLE_UART_PROPERTY_UUID) {
        g_mine_write_param.handle = property->handle;
        g_mine_write_param.type = SSAP_PROPERTY_TYPE_VALUE;
        g_mine_property_ready = true;
        osal_printk("[mine slave] property ready, handle:%u\r\n", property->handle);
        mine_oled_set_linef(MINE_OLED_LINE_UUID, "UUID:P%04X H:%u", uuid16, property->handle);
        mine_oled_push_state("PROP READY");
    }
}

/**
 * @brief 发现流程结束回调。
 */
static void mine_find_structure_cmp_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(structure_result);
    osal_printk("[mine slave] find structure complete, status:%x\r\n", status);
    if (status != ERRCODE_SLE_SUCCESS) {
        mine_oled_push_state("DISC FAIL");
    }
}

/**
 * @brief 写命令确认回调。
 */
static void mine_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_write_result_t *write_result, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(write_result);
    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] write confirm failed:%x\r\n", status);
        mine_oled_push_state("WRITE FAIL");
    }
}

/**
 * @brief 收到 notify（主机 -> 从机）回调。
 */
static void mine_notification_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);

    if ((data == NULL) || (data->data == NULL) || (data->data_len == 0)) {
        return;
    }

    osal_printk("[mine slave] sle notify rx len:%u\r\n", data->data_len);
    mine_uart_write_enabled_buses(data->data, data->data_len);
    mine_oled_push_data_event(MINE_UART_BUS_INVALID, "SLE RX", data->data, data->data_len);
}

/**
 * @brief 收到 indication（主机 -> 从机）回调。
 */
static void mine_indication_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);

    if ((data == NULL) || (data->data == NULL) || (data->data_len == 0)) {
        return;
    }

    osal_printk("[mine slave] sle indication rx len:%u\r\n", data->data_len);
    mine_uart_write_enabled_buses(data->data, data->data_len);
    mine_oled_push_data_event(MINE_UART_BUS_INVALID, "SLE RX", data->data, data->data_len);
}

/* ========================= 回调注册与初始化 ========================= */

/**
 * @brief 填充并注册扫描回调。
 */
static errcode_t mine_register_seek_callbacks(void)
{
    g_mine_seek_cbks.sle_enable_cb = mine_sle_enable_cb;
    g_mine_seek_cbks.seek_enable_cb = mine_seek_enable_cb;
    g_mine_seek_cbks.seek_result_cb = mine_seek_result_cb;
    g_mine_seek_cbks.seek_disable_cb = mine_seek_disable_cb;

    return sle_announce_seek_register_callbacks(&g_mine_seek_cbks);
}

/**
 * @brief 填充并注册连接回调。
 */
static errcode_t mine_register_conn_callbacks(void)
{
    g_mine_conn_cbks.connect_state_changed_cb = mine_connect_state_changed_cb;
    g_mine_conn_cbks.pair_complete_cb = mine_pair_complete_cb;

    return sle_connection_register_callbacks(&g_mine_conn_cbks);
}

/**
 * @brief 填充并注册 SSAPC 回调。
 */
static errcode_t mine_register_ssapc_callbacks(void)
{
    g_mine_ssapc_cbks.exchange_info_cb = mine_exchange_info_cb;
    g_mine_ssapc_cbks.find_structure_cb = mine_find_structure_cb;
    g_mine_ssapc_cbks.ssapc_find_property_cbk = mine_find_property_cb;
    g_mine_ssapc_cbks.find_structure_cmp_cb = mine_find_structure_cmp_cb;
    g_mine_ssapc_cbks.write_cfm_cb = mine_write_cfm_cb;
    g_mine_ssapc_cbks.notification_cb = mine_notification_cb;
    g_mine_ssapc_cbks.indication_cb = mine_indication_cb;

    return ssapc_register_callbacks(&g_mine_ssapc_cbks);
}

errcode_t mine_sle_uart_slave_init(void)
{
    errcode_t ret;

    g_mine_seek_started = false;
    g_mine_seek_stop_pending = false;
    g_mine_connecting_pending = false;

    mine_prepare_sle_mac();

    ret = mine_register_seek_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] register seek callbacks failed:%x\r\n", ret);
        mine_oled_push_state("SEEK CB FAIL");
        return ret;
    }

    ret = mine_register_conn_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] register conn callbacks failed:%x\r\n", ret);
        mine_oled_push_state("CONN CB FAIL");
        return ret;
    }

    ret = mine_register_ssapc_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] register ssapc callbacks failed:%x\r\n", ret);
        mine_oled_push_state("SSAPC CB FAIL");
        return ret;
    }

    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] enable sle failed:%x\r\n", ret);
        mine_oled_push_state("ENABLE FAIL");
        return ret;
    }

    ret = mine_apply_local_addr_and_name();
    if (ret != ERRCODE_SLE_SUCCESS) {
        mine_oled_push_state("SET LOCAL FAIL");
        return ret;
    }

    mine_sle_uart_slave_start_scan();

    osal_printk("[mine slave] init ok\r\n");
    mine_oled_push_state("SLE INIT OK");
    return ERRCODE_SLE_SUCCESS;
}

/* ========================= 任务与入口 ========================= */

/**
 * @brief 从机主任务。
 *
 * @param arg 线程参数（未使用）。
 * @return void* 固定返回 NULL。
 */
static void *mine_sle_uart_slave_task(const char *arg)
{
    int read_ret;
    errcode_t send_ret;
    char radar_status[24] = {0};

    unused(arg);
    osal_msleep(MINE_INIT_DELAY_MS);

    osal_printk("[mine slave] task start\r\n");
    mine_oled_init();
    mine_sle_uart_slave_uart_init();
#if MINE_LD2402_ENABLE
    if (ld2402_init(MINE_LD2402_UART_BUS)) {
        mine_oled_push_state("LD2402 READY");
    } else {
        mine_oled_push_state("LD2402 WAIT");
    }
#endif
    mine_oled_push_state("SLE INIT...");
    if (mine_sle_uart_slave_init() != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] init failed\r\n");
        mine_oled_push_state("INIT FAIL");
        return NULL;
    }

    while (1) {
        mine_sle_uart_slave_msg_t msg = {0};

        read_ret = osal_msg_queue_read_copy(g_mine_uart_msg_queue, &msg,
            &g_mine_uart_msg_size, MINE_TASK_LOOP_WAIT_MS);
#if MINE_LD2402_ENABLE
        if (ld2402_get_status(radar_status, sizeof(radar_status))) {
            mine_oled_push_state(radar_status);
        }
#endif
        mine_oled_flush_pending();
        if (read_ret != OSAL_SUCCESS) {
            continue;
        }

        if ((msg.value != NULL) && (msg.value_len > 0)) {
            osal_printk("[mine slave] %s rx queue len:%u\r\n",
                mine_uart_bus_name(msg.uart_bus), msg.value_len);
            send_ret = mine_sle_uart_slave_send_to_host(&msg);
            if (send_ret != ERRCODE_SLE_SUCCESS) {
                osal_printk("[mine slave] uart->sle send failed:%x\r\n", send_ret);
            }
            osal_vfree(msg.value);
        }
    }
}

/**
 * @brief 应用入口：创建消息队列并启动从机任务。
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

/* 从机应用注册入口。 */
app_run(mine_sle_uart_slave_entry);
