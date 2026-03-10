/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Host side UART0 <-> SLE bridge.
 */

#include "sle_uart_host.h"

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "app_init.h"
#include "common_def.h"
#include "mac_addr.h"
#include "pinctrl.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "soc_osal.h"
#include "uart.h"

#include "ssd1306/hal_bsp_ssd1306.h"
#include "sle_uart_host_adv.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/* ========================= 参数配置区 ========================= */

/* UART 接收缓存长度。该缓存用于驱动层接收中断搬运。 */
#define MINE_UART_RX_BUFFER_SIZE 512

/* UART 总线数量（UART0/1/2）。 */
#define MINE_UART_BUS_COUNT 3

/* 非法 UART 标记。 */
#define MINE_UART_BUS_INVALID 0xFF

/* SLE 默认 MTU。示例中同时在主从两端都配置为该值。 */
#define MINE_SLE_DEFAULT_MTU_SIZE 512

/* 单次通过 SLE 发送的数据分片大小。 */
#define MINE_SLE_SAFE_CHUNK_LEN 200

/* 连接建立后，向协议栈申请的数据长度参数（经验值：MTU - 12）。 */
#define MINE_SLE_DATA_LEN_AFTER_CONNECTED (MINE_SLE_DEFAULT_MTU_SIZE - 12)

/* 广播句柄，需与广播模块保持一致。 */
#define MINE_SLE_ADV_HANDLE_DEFAULT 1

/* UUID 处理用常量。 */
#define MINE_UUID_APP_LEN 2
#define MINE_UUID_BASE_INDEX_14 14
#define MINE_SHIFT_8_BITS 8

/* 初始化延迟，给系统底层留出启动缓冲时间。 */
#define MINE_INIT_DELAY_MS 500

/* Host 主任务轮询 OLED 脏标记的等待时间（毫秒）。 */
#define MINE_TASK_LOOP_WAIT_MS 80

/* SLE 服务创建时的初始特征值长度。 */
#define MINE_PROPERTY_INIT_VALUE_LEN 8

/* OLED 文本显示配置：0.96 寸 128x64 屏，在 8 像素字体下共 8 行、每行约 21 字符。 */
#define MINE_OLED_LINE_COUNT 8
#define MINE_OLED_LINE_CHARS 21
#define MINE_OLED_DATA_LINE_COUNT 3
#define MINE_OLED_DATA_TOTAL_CHARS (MINE_OLED_LINE_CHARS * MINE_OLED_DATA_LINE_COUNT)
#define MINE_OLED_FONT_SIZE TEXT_SIZE_8
#define MINE_OLED_DATA_PREVIEW_BYTES 4
#define MINE_OLED_EVENT_BUFFER_LEN 64

/* OLED 固定信息面板行定义（每一行显示同一类信息）。 */
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

/* ========================= 全局状态区 ========================= */

/* 多 UART 接收缓冲区（按 bus 索引 0/1/2）。 */
static uint8_t g_mine_uart_rx_buffer[MINE_UART_BUS_COUNT][MINE_UART_RX_BUFFER_SIZE] = {0};

/* 任务间消息队列：UART 回调中入队，主任务中出队并通过 SLE 发送。 */
static unsigned long g_mine_uart_msg_queue = 0;
static unsigned int g_mine_uart_msg_size = sizeof(mine_sle_uart_host_msg_t);

/* 当前连接状态。 */
static volatile bool g_mine_peer_connected = false;
static volatile uint16_t g_mine_conn_id = 0;

/* SLE Server 相关句柄。 */
static uint8_t g_mine_server_id = 0;
static uint16_t g_mine_service_handle = 0;
static uint16_t g_mine_property_handle = 0;

/* SLE app uuid（2字节形式）。 */
static char g_mine_sle_app_uuid[MINE_UUID_APP_LEN] = {0x00, 0x00};

/* 特征初始值。 */
static char g_mine_property_init_value[MINE_PROPERTY_INIT_VALUE_LEN] = {0};

/* 当芯片没有烧录 SLE MAC 时，Host 侧使用该回退地址保障 demo 可用。 */
static const uint8_t g_mine_host_fallback_sle_mac[MINE_SLE_MAC_ADDR_LEN] = MINE_HOST_FALLBACK_SLE_MAC;

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

/* 16 位 UUID 映射到 128 位 UUID 的基础模板。 */
static uint8_t g_mine_sle_uuid_base[SLE_UUID_LEN] = {
    0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 保存 OSAL 原始日志函数入口，用于“osal_printk + PRINT”双通道镜像输出。 */
static void (*g_mine_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/**
 * @brief 同时输出到 OSAL 日志口与 APP 调试口。
 *
 * @param fmt 日志格式串。
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
 * @brief 标记指定 OLED 行为脏行，等待 host 任务线程统一刷新。
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
 * @brief 在 host 任务线程中刷新 OLED（避免在 bt 回调线程里直接走 I2C）。
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
 *
 * @param line_index 行号。
 * @param text       待显示文本。
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
static void mine_oled_push_text(const char *text)
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
 *
 * @param prefix 事件前缀，例如 "UART TX"、"SLE RX"。
 * @param data   数据指针。
 * @param len    数据长度。
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
 * @brief 初始化 OLED，并显示启动提示信息。
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
        osal_printk("[mine host] oled init failed, ret=%x\r\n", ret);
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
    mine_oled_set_line_raw(MINE_OLED_LINE_TITLE, "HOST UART<->SLE");
    mine_oled_set_line_raw(MINE_OLED_LINE_STATE, "STATE:BOOT");
    mine_oled_set_data_lines("DATA:--");
    mine_oled_set_linef(MINE_OLED_LINE_UUID, "UUID:S%04X P%04X",
        MINE_SLE_UART_SERVICE_UUID, MINE_SLE_UART_PROPERTY_UUID);
    mine_oled_set_line_raw(MINE_OLED_LINE_RX, "RX*:0 C:0");
    mine_oled_set_line_raw(MINE_OLED_LINE_TX, "TX*:0 C:0");
    mine_oled_flush_pending();
}

/**
 * @brief 设置 16 位 UUID（写入到基础 UUID 的最后 2 字节）。
 *
 * @param uuid16 16 位 UUID。
 * @param out    输出 UUID。
 */
static void mine_set_uuid_u16(uint16_t uuid16, sle_uuid_t *out)
{
    if (out == NULL) {
        return;
    }

    if (memcpy_s(out->uuid, SLE_UUID_LEN, g_mine_sle_uuid_base, SLE_UUID_LEN) != EOK) {
        out->len = 0;
        return;
    }

    out->len = 2;
    out->uuid[MINE_UUID_BASE_INDEX_14] = (uint8_t)uuid16;
    out->uuid[MINE_UUID_BASE_INDEX_14 + 1] = (uint8_t)(uuid16 >> MINE_SHIFT_8_BITS);
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
        osal_printk("[mine host] use system sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            sle_mac[0], sle_mac[1], sle_mac[2], sle_mac[3], sle_mac[4], sle_mac[5]);
        return;
    }

    ret = set_dev_addr(g_mine_host_fallback_sle_mac, MINE_SLE_MAC_ADDR_LEN, IFTYPE_SLE);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[mine host] set fallback sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            g_mine_host_fallback_sle_mac[0], g_mine_host_fallback_sle_mac[1], g_mine_host_fallback_sle_mac[2],
            g_mine_host_fallback_sle_mac[3], g_mine_host_fallback_sle_mac[4], g_mine_host_fallback_sle_mac[5]);
    } else {
        osal_printk("[mine host] set fallback sle mac failed, ret:%x\r\n", ret);
    }
}

/**
 * @brief 按 samples/bt/sle 方式设置本地地址与本地名称。
 *
 * 流程与样例保持一致：enable_sle 之后显式 sle_set_local_addr / sle_set_local_name。
 */
static errcode_t mine_apply_local_addr_and_name(void)
{
    sle_addr_t local_addr = {0};
    uint8_t local_mac[SLE_ADDR_LEN] = {0};
    errcode_t ret;

    ret = get_dev_addr(local_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    if (ret != ERRCODE_SUCC) {
        if (memcpy_s(local_mac, sizeof(local_mac), g_mine_host_fallback_sle_mac,
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
        osal_printk("[mine host] sle_set_local_addr failed:%x\r\n", ret);
        return ret;
    }

    ret = sle_set_local_name((const uint8_t *)MINE_SLE_UART_HOST_NAME,
        (uint8_t)strlen(MINE_SLE_UART_HOST_NAME));
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] sle_set_local_name failed:%x\r\n", ret);
        return ret;
    }

    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief 将接收到的 SLE 数据输出到所有启用的 UART。
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
            osal_printk("[mine host] %s write failed, ret=%d\r\n",
                mine_uart_bus_name(bus_index), (int)write_ret);
        }
    }
}

/**
 * @brief 将一条消息分片后通过 notify 发送给从机。
 *
 * @param msg 消息结构体。
 * @return errcode_t
 */
errcode_t mine_sle_uart_host_send_by_handle(const mine_sle_uart_host_msg_t *msg)
{
    ssaps_ntf_ind_t notify_param = {0};
    uint16_t offset = 0;
    uint16_t chunk_len;
    uint16_t conn_id_snapshot;
    uint8_t uart_bus_snapshot;
    bool peer_connected_snapshot;
    errcode_t ret;

    if ((msg == NULL) || (msg->value == NULL) || (msg->value_len == 0)) {
        osal_printk("[mine host] invalid uart->sle msg\r\n");
        return ERRCODE_SLE_FAIL;
    }

    peer_connected_snapshot = g_mine_peer_connected;
    conn_id_snapshot = g_mine_conn_id;
    uart_bus_snapshot = msg->uart_bus;

    if (!peer_connected_snapshot) {
        osal_printk("[mine host] drop %s data, link:%u cid:%u\r\n",
            mine_uart_bus_name(uart_bus_snapshot),
            (unsigned int)peer_connected_snapshot, conn_id_snapshot);
        return ERRCODE_SLE_FAIL;
    }

    notify_param.handle = g_mine_property_handle;
    notify_param.type = SSAP_PROPERTY_TYPE_VALUE;

    while (offset < msg->value_len) {
        chunk_len = (uint16_t)((msg->value_len - offset) > MINE_SLE_SAFE_CHUNK_LEN ?
            MINE_SLE_SAFE_CHUNK_LEN : (msg->value_len - offset));

        notify_param.value = (uint8_t *)(msg->value + offset);
        notify_param.value_len = chunk_len;

        ret = ssaps_notify_indicate(g_mine_server_id, conn_id_snapshot, &notify_param);
        if (ret != ERRCODE_SLE_SUCCESS) {
            osal_printk("[mine host] notify failed, ret=%x\r\n", ret);
            return ret;
        }
        offset = (uint16_t)(offset + chunk_len);
    }

    osal_printk("[mine host] %s->sle notify len:%u\r\n",
        mine_uart_bus_name(uart_bus_snapshot), msg->value_len);

    return ERRCODE_SLE_SUCCESS;
}

/* ========================= UART 模块 ========================= */

/**
 * @brief UART 接收回调公共处理（中断上下文）。
 */
static void mine_sle_uart_host_read_handler_common(uart_bus_t bus, const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_msg_t msg = {0};
    void *buffer_copy = NULL;
    int write_ret;

    unused(error);

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

    /* 未连接时直接丢弃 UART 输入，避免队列/日志/OLED 风暴拖垮系统。 */
    if (!g_mine_peer_connected) {
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

static void mine_sle_uart_host_read_handler_uart0(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_read_handler_common(UART_BUS_0, buffer, length, error);
}

static void mine_sle_uart_host_read_handler_uart1(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_read_handler_common(UART_BUS_1, buffer, length, error);
}

static void mine_sle_uart_host_read_handler_uart2(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_read_handler_common(UART_BUS_2, buffer, length, error);
}

static bool mine_sle_uart_host_uart_init_one(uart_bus_t bus)
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
        rx_cb = mine_sle_uart_host_read_handler_uart0;
    } else if (bus == UART_BUS_1) {
        rx_cb = mine_sle_uart_host_read_handler_uart1;
    } else if (bus == UART_BUS_2) {
        rx_cb = mine_sle_uart_host_read_handler_uart2;
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
        osal_printk("[mine host] %s pin not configured, skip\r\n", mine_uart_bus_name(bus_index));
        return false;
    }

    uart_buffer_cfg.rx_buffer = g_mine_uart_rx_buffer[bus_index];
    uart_buffer_cfg.rx_buffer_size = MINE_UART_RX_BUFFER_SIZE;

    uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
    uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

    (void)uapi_uart_deinit(bus);
    ret = uapi_uart_init(bus, &pin_cfg, &attr, NULL, &uart_buffer_cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine host] %s init failed, ret=%x\r\n", mine_uart_bus_name(bus_index), ret);
        return false;
    }

    ret = uapi_uart_register_rx_callback(bus, UART_RX_CONDITION_MASK_IDLE, 1, rx_cb);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine host] %s rx cb failed, ret=%x\r\n", mine_uart_bus_name(bus_index), ret);
        return false;
    }

    return true;
}

void mine_sle_uart_host_uart_init(void)
{
    uint8_t bus_index;
    uint8_t enabled_count = 0;
    uint8_t ok_count = 0;

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }
        enabled_count++;
        if (mine_sle_uart_host_uart_init_one((uart_bus_t)bus_index)) {
            ok_count++;
        }
    }

    osal_printk("[mine host] uart init summary, enabled:%u ok:%u\r\n", enabled_count, ok_count);
    if (ok_count > 0) {
        mine_oled_push_text("UART INIT OK");
    } else {
        mine_oled_push_text("UART INIT FAIL");
    }
}

/* ========================= SSAPS（服务端）回调 ========================= */

/**
 * @brief 读请求回调。
 */
static void mine_ssaps_read_request_cb(uint8_t server_id, uint16_t conn_id,
    ssaps_req_read_cb_t *read_cb_param, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(read_cb_param);
    unused(status);
}

/**
 * @brief 写请求回调（从机 -> 主机数据路径）。
 *
 * @param server_id      服务端 ID。
 * @param conn_id        连接 ID。
 * @param write_cb_param 写入参数，包含 value 与 length。
 * @param status         回调状态。
 */
static void mine_ssaps_write_request_cb(uint8_t server_id, uint16_t conn_id,
    ssaps_req_write_cb_t *write_cb_param, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(status);

    if ((write_cb_param == NULL) || (write_cb_param->value == NULL) || (write_cb_param->length == 0)) {
        return;
    }

    osal_printk("[mine host] sle->uart len:%u\r\n", write_cb_param->length);
    mine_uart_write_enabled_buses(write_cb_param->value, write_cb_param->length);
    mine_oled_push_data_event(MINE_UART_BUS_INVALID, "SLE RX", write_cb_param->value, write_cb_param->length);
}

/**
 * @brief MTU 变化回调。
 */
static void mine_ssaps_mtu_changed_cb(uint8_t server_id, uint16_t conn_id,
    ssap_exchange_info_t *mtu_info, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(status);

    if (mtu_info != NULL) {
        osal_printk("[mine host] mtu changed: %u\r\n", mtu_info->mtu_size);
    }
}

/**
 * @brief 服务启动回调。
 */
static void mine_ssaps_start_service_cb(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("[mine host] start service cb, server:%u handle:%u status:%x\r\n",
        server_id, handle, status);
}

/**
 * @brief 注册 SSAPS 回调。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_host_register_ssaps_callbacks(void)
{
    ssaps_callbacks_t ssaps_cb = {0};

    ssaps_cb.start_service_cb = mine_ssaps_start_service_cb;
    ssaps_cb.mtu_changed_cb = mine_ssaps_mtu_changed_cb;
    ssaps_cb.read_request_cb = mine_ssaps_read_request_cb;
    ssaps_cb.write_request_cb = mine_ssaps_write_request_cb;

    return ssaps_register_callbacks(&ssaps_cb);
}

/* ========================= SSAPS（服务端）资源创建 ========================= */

/**
 * @brief 添加服务（Service）。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_host_add_service(void)
{
    sle_uuid_t service_uuid = {0};

    mine_set_uuid_u16(MINE_SLE_UART_SERVICE_UUID, &service_uuid);
    return ssaps_add_service_sync(g_mine_server_id, &service_uuid, true, &g_mine_service_handle);
}

/**
 * @brief 添加特征（Property）和描述符（Descriptor）。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_host_add_property(void)
{
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t descriptor_value[] = {0x01, 0x00};
    errcode_t ret;

    property.permissions = (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE);
    property.operate_indication = (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY);
    mine_set_uuid_u16(MINE_SLE_UART_PROPERTY_UUID, &property.uuid);

    property.value = osal_vmalloc(sizeof(g_mine_property_init_value));
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }

    if (memcpy_s(property.value, sizeof(g_mine_property_init_value), g_mine_property_init_value,
        sizeof(g_mine_property_init_value)) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_add_property_sync(g_mine_server_id, g_mine_service_handle, &property, &g_mine_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_vfree(property.value);
        return ret;
    }

    descriptor.permissions = (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE);
    descriptor.operate_indication = (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE);
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = descriptor_value;
    descriptor.value_len = sizeof(descriptor_value);

    ret = ssaps_add_descriptor_sync(g_mine_server_id, g_mine_service_handle, g_mine_property_handle, &descriptor);
    osal_vfree(property.value);

    return ret;
}

/**
 * @brief 注册 Server 并完成 Service/Property 创建。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_host_add_server(void)
{
    sle_uuid_t app_uuid = {0};
    errcode_t ret;

    app_uuid.len = sizeof(g_mine_sle_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_mine_sle_app_uuid, sizeof(g_mine_sle_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_register_server(&app_uuid, &g_mine_server_id);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }

    ret = mine_sle_uart_host_add_service();
    if (ret != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_mine_server_id);
        return ret;
    }

    ret = mine_sle_uart_host_add_property();
    if (ret != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_mine_server_id);
        return ret;
    }

    ret = ssaps_start_service(g_mine_server_id, g_mine_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_mine_server_id);
        return ret;
    }

    return ERRCODE_SLE_SUCCESS;
}

/* ========================= 连接管理回调 ========================= */

/**
 * @brief 连接状态变化回调。
 */
static void mine_sle_uart_host_connect_state_cb(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    unused(pair_state);
    unused(disc_reason);

    if (addr != NULL) {
        osal_printk("[mine host] remote:%02x:**:**:**:%02x:%02x state:%x\r\n",
            addr->addr[0], addr->addr[4], addr->addr[5], conn_state);
    }

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_mine_peer_connected = true;
        g_mine_conn_id = conn_id;
        osal_printk("[mine host] connected, conn_id:%u\r\n", conn_id);
        (void)sle_set_data_len(conn_id, MINE_SLE_DATA_LEN_AFTER_CONNECTED);
        mine_oled_push_text("CONNECTED");
        return;
    }

    if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_mine_peer_connected = false;
        g_mine_conn_id = 0;
        osal_printk("[mine host] disconnected, restart advertise\r\n");
        (void)sle_start_announce(MINE_SLE_ADV_HANDLE_DEFAULT);
        mine_oled_push_text("DISCONNECTED");
    }
}

/**
 * @brief 配对完成回调。
 */
static void mine_sle_uart_host_pair_complete_cb(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    unused(addr);
    osal_printk("[mine host] pair complete cid:%u status:%x\r\n", conn_id, status);
    if (status == ERRCODE_SLE_SUCCESS) {
        mine_oled_push_text("PAIR OK");
    } else {
        mine_oled_push_text("PAIR FAIL");
    }
}

/**
 * @brief 注册连接管理回调。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_host_register_conn_callbacks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};

    conn_cbks.connect_state_changed_cb = mine_sle_uart_host_connect_state_cb;
    conn_cbks.pair_complete_cb = mine_sle_uart_host_pair_complete_cb;

    return sle_connection_register_callbacks(&conn_cbks);
}

/* ========================= Host 初始化与任务入口 ========================= */

errcode_t mine_sle_uart_host_init(void)
{
    ssap_exchange_info_t exchange_info = {0};
    errcode_t ret;

    mine_prepare_sle_mac();

    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine host] enable sle failed:%x\r\n", ret);
        mine_oled_push_text("ENABLE SLE FAIL");
        return ret;
    }

    ret = mine_apply_local_addr_and_name();
    if (ret != ERRCODE_SLE_SUCCESS) {
        mine_oled_push_text("SET LOCAL FAIL");
        return ret;
    }

    ret = mine_sle_uart_host_register_conn_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] register conn callbacks failed:%x\r\n", ret);
        mine_oled_push_text("CONN CB FAIL");
        return ret;
    }

    ret = mine_sle_uart_host_register_ssaps_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] register ssaps callbacks failed:%x\r\n", ret);
        mine_oled_push_text("SSAPS CB FAIL");
        return ret;
    }

    ret = mine_sle_uart_host_add_server();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] add server failed:%x\r\n", ret);
        mine_oled_push_text("ADD SERVER FAIL");
        return ret;
    }

    exchange_info.mtu_size = MINE_SLE_DEFAULT_MTU_SIZE;
    exchange_info.version = 1;
    (void)ssaps_set_info(g_mine_server_id, &exchange_info);

    ret = mine_sle_uart_host_adv_init();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] adv init failed:%x\r\n", ret);
        mine_oled_push_text("ADV START FAIL");
        return ret;
    }

    osal_printk("[mine host] init ok\r\n");
    mine_oled_push_text("SLE INIT OK");
    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief Host 主任务。
 *
 * @param arg 线程参数（本示例未使用）。
 * @return void* 固定返回 NULL。
 */
static void *mine_sle_uart_host_task(const char *arg)
{
    int read_ret;
    errcode_t send_ret;

    unused(arg);
    osal_msleep(MINE_INIT_DELAY_MS);

    osal_printk("[mine host] task start\r\n");
    mine_oled_init();
    mine_sle_uart_host_uart_init();
    mine_oled_push_text("SLE INIT...");
    mine_oled_flush_pending();
    if (mine_sle_uart_host_init() != ERRCODE_SLE_SUCCESS) {
        mine_oled_push_text("HOST INIT FAIL");
        mine_oled_flush_pending();
        return NULL;
    }

    mine_oled_flush_pending();

    while (1) {
        mine_sle_uart_host_msg_t msg = {0};

        read_ret = osal_msg_queue_read_copy(g_mine_uart_msg_queue, &msg,
            &g_mine_uart_msg_size, MINE_TASK_LOOP_WAIT_MS);
        mine_oled_flush_pending();
        if (read_ret != OSAL_SUCCESS) {
            continue;
        }

        if ((msg.value != NULL) && (msg.value_len > 0)) {
            osal_printk("[mine host] %s rx queue len:%u\r\n",
                mine_uart_bus_name(msg.uart_bus), msg.value_len);
            send_ret = mine_sle_uart_host_send_by_handle(&msg);
            if (send_ret == ERRCODE_SLE_SUCCESS) {
                mine_oled_push_data_event(msg.uart_bus, "UART TX", msg.value, msg.value_len);
            } else if (g_mine_peer_connected) {
                /* 掉线态失败无需反复刷 OLED，避免 I2C 占用过高。 */
                mine_oled_push_text("SLE SEND FAIL");
            }
            osal_vfree(msg.value);
        }
    }
}

/**
 * @brief 应用入口：创建消息队列并启动 Host 任务。
 */
static void mine_sle_uart_host_entry(void)
{
    osal_task *task_handle = NULL;
    int create_ret;

    osal_kthread_lock();

    create_ret = osal_msg_queue_create("mine_sle_host_msg", (unsigned short)g_mine_uart_msg_size,
        &g_mine_uart_msg_queue, 0, g_mine_uart_msg_size);
    if (create_ret != OSAL_SUCCESS) {
        osal_printk("[mine host] create queue failed:%x\r\n", create_ret);
        osal_kthread_unlock();
        return;
    }
    osal_printk("[mine host] queue created\r\n");

    task_handle = osal_kthread_create((osal_kthread_handler)mine_sle_uart_host_task,
        0, "mine_sle_host", MINE_SLE_UART_HOST_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MINE_SLE_UART_HOST_TASK_PRIO);
        osal_kfree(task_handle);
        osal_printk("[mine host] task created\r\n");
    } else {
        osal_printk("[mine host] task create failed\r\n");
    }

    osal_kthread_unlock();
}

/* Host 应用注册入口。 */
app_run(mine_sle_uart_host_entry);
