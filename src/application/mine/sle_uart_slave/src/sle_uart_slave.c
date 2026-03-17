/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 UART0 <-> SLE 桥接。
 */

#include "sle_uart_slave.h"
#include "sle_uart_slave_ld2402.h"
#include "sle_uart_slave_module.h"
#include "sle_uart_slave_zw101.h"

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

/* 是否启用 PRINT 通道日志（通常会带 APP| 前缀，默认关闭以避免重复输出）。 */
#ifndef MINE_LOG_PRINT_CHANNEL_ENABLE
#define MINE_LOG_PRINT_CHANNEL_ENABLE 0
#endif

/* 多路 UART 接收缓冲区（按 UART0/1/2 索引）。 */
static uint8_t g_mine_uart_rx_buffer[MINE_UART_BUS_COUNT][MINE_UART_RX_BUFFER_SIZE] = {0};

/* UART 回调写入静态环形槽位，任务线程通过索引消息消费。 */
typedef struct {
    uint8_t uart_bus;
    uint16_t value_len;
    uint8_t value[MINE_UART_RX_BUFFER_SIZE];
    bool in_use;
} mine_sle_uart_ring_slot_t;

/* 任务队列仅传递槽位索引，避免大块内存在队列中重复复制。 */
typedef struct {
    uint8_t slot_index;
} mine_sle_uart_ring_msg_t;

static mine_sle_uart_ring_slot_t g_mine_uart_ring_slots[MINE_UART_RING_SLOT_COUNT] = {0};
static uint8_t g_mine_uart_ring_alloc_cursor = 0;
static uint32_t g_mine_uart_ring_drop_count = 0;
static uint32_t g_mine_uart_ring_drop_last_log_ms = 0;
static bool g_mine_uart_ring_drop_log_started = false;

static unsigned long g_mine_uart_msg_queue = 0;
static unsigned int g_mine_uart_msg_size = sizeof(mine_sle_uart_ring_msg_t);

/* 保留原 OSAL 日志出口，并镜像到 PRINT 通道。 */
static void (*g_mine_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/* UART2 接收日志最大显示字节数，避免长报文刷屏影响联调体验。 */
#define MINE_UART_RX_LOG_SHOW_MAX_BYTES 64
/* 文本日志缓冲区：预留转义字符空间（如\n、\r），末尾保留字符串结束符。 */
#define MINE_UART_RX_LOG_TEXT_MAX_LEN ((MINE_UART_RX_LOG_SHOW_MAX_BYTES * 2) + 1)

/**
 * @brief 将日志同步镜像到 UART0，保证串口调试口持续可见。
 *
 *
 * @param log_buf    日志缓冲区。
 * @param format_len 已格式化日志长度。
 */
static void mine_slave_log_mirror_uart0(const char *log_buf, int32_t format_len)
{
#if (MINE_LOG_UART0_MIRROR_ENABLE == 0)
    unused(log_buf);
    unused(format_len);
    return;
#else
    if ((log_buf == NULL) || (format_len <= 0)) {
        return;
    }

    /* 中断上下文禁用镜像直写，避免在 UART ISR 内触发重型日志路径。 */
    if (osal_in_interrupt() != 0) {
        return;
    }

    /* 保持 UART0 与系统日志同步输出，不因串口未就绪中断主流程。 */
    (void)uapi_uart_write(UART_BUS_0, (const uint8_t *)log_buf, (uint16_t)format_len, 0);
#endif
}

/**
 * @brief Slave 统一日志接口，双路输出到 OSAL 与 PRINT。
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
    /* 如需 APP| 前缀通道可打开该开关，默认关闭以减少重复日志。 */
    PRINT("%s", log_buf);
#endif
    mine_slave_log_mirror_uart0(log_buf, format_len);
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

/**
 * @brief 限频输出环形槽位耗尽日志。
 *
 * @param bus    数据来源 UART 总线。
 * @param length 丢弃的数据长度。
 */
static void mine_uart_ring_log_drop_rate_limited(uart_bus_t bus, uint16_t length)
{
    uint32_t now_ms = (uint32_t)uapi_systick_get_ms();

    g_mine_uart_ring_drop_count++;
    if (g_mine_uart_ring_drop_log_started &&
        ((uint32_t)(now_ms - g_mine_uart_ring_drop_last_log_ms) < MINE_UART_RING_DROP_LOG_INTERVAL_MS)) {
        return;
    }

    osal_printk("[mine slave] ring full drop %s len:%u dropped:%lu\r\n",
        mine_slave_uart_bus_name((uint8_t)bus), (unsigned int)length,
        (unsigned long)g_mine_uart_ring_drop_count);

    g_mine_uart_ring_drop_count = 0;
    g_mine_uart_ring_drop_last_log_ms = now_ms;
    g_mine_uart_ring_drop_log_started = true;
}

/**
 * @brief 申请一个空闲环形槽位。
 *
 * @return int32_t >=0 为槽位索引，<0 表示无空闲槽位。
 */
static int32_t mine_uart_ring_alloc_slot(void)
{
    uint8_t offset;
    uint8_t slot_index;
    uint32_t irq_sts = osal_irq_lock();

    for (offset = 0; offset < MINE_UART_RING_SLOT_COUNT; offset++) {
        slot_index = (uint8_t)((g_mine_uart_ring_alloc_cursor + offset) % MINE_UART_RING_SLOT_COUNT);
        if (!g_mine_uart_ring_slots[slot_index].in_use) {
            g_mine_uart_ring_slots[slot_index].in_use = true;
            g_mine_uart_ring_alloc_cursor = (uint8_t)((slot_index + 1) % MINE_UART_RING_SLOT_COUNT);
            osal_irq_restore(irq_sts);
            return (int32_t)slot_index;
        }
    }

    osal_irq_restore(irq_sts);
    return -1;
}

/**
 * @brief 释放已消费的环形槽位。
 *
 * @param slot_index 槽位索引。
 */
static void mine_uart_ring_free_slot(uint8_t slot_index)
{
    uint32_t irq_sts;

    if (slot_index >= MINE_UART_RING_SLOT_COUNT) {
        return;
    }

    irq_sts = osal_irq_lock();
    g_mine_uart_ring_slots[slot_index].in_use = false;
    g_mine_uart_ring_slots[slot_index].value_len = 0;
    g_mine_uart_ring_slots[slot_index].uart_bus = MINE_UART_BUS_INVALID;
    osal_irq_restore(irq_sts);
}

/**
 * @brief 重置环形槽位状态。
 */
static void mine_uart_ring_reset(void)
{
    uint8_t slot_index;

    for (slot_index = 0; slot_index < MINE_UART_RING_SLOT_COUNT; slot_index++) {
        g_mine_uart_ring_slots[slot_index].in_use = false;
        g_mine_uart_ring_slots[slot_index].value_len = 0;
        g_mine_uart_ring_slots[slot_index].uart_bus = MINE_UART_BUS_INVALID;
    }

    g_mine_uart_ring_alloc_cursor = 0;
    g_mine_uart_ring_drop_count = 0;
    g_mine_uart_ring_drop_last_log_ms = 0;
    g_mine_uart_ring_drop_log_started = false;
}

/**
 * @brief 将 UART2 接收数据转换为可读文本并打印到串口日志。
 *
 * 仅展示前 MINE_UART_RX_LOG_SHOW_MAX_BYTES 字节，避免长数据帧导致日志阻塞。
 * 可打印 ASCII 字节直接输出；\r/\n/\t 转义显示；其余不可打印字节显示为 '.'。
 *
 * @param buffer UART2 接收缓冲区。
 * @param length 接收字节数。
 */
#if (MINE_UART2_RX_ISR_DUMP_ENABLE == 1)
static void mine_sle_uart_slave_dump_uart2_rx(const uint8_t *buffer, uint16_t length)
{
    static uint32_t s_last_dump_ms = 0;
    static bool s_dump_started = false;
    char log_text[MINE_UART_RX_LOG_TEXT_MAX_LEN] = {0};
    uint16_t show_len;
    uint16_t idx;
    uint16_t pos = 0;
    bool truncated = false;
    uint8_t ch;
    uint32_t now_ms;

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

    /* 对 LD2402 原始文本流做限频，目标是每秒最多 2 次日志输出。 */
    now_ms = (uint32_t)uapi_systick_get_ms();
    if (s_dump_started && ((uint32_t)(now_ms - s_last_dump_ms) < MINE_UART2_RX_DUMP_INTERVAL_MS)) {
        return;
    }
    s_last_dump_ms = now_ms;
    s_dump_started = true;

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
            /* 直接显示可打印 ASCII，便于定位上位机文本协议内容。 */
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

        /* 对不可打印字节给出占位符，避免日志出现乱码控制符。 */
        log_text[pos++] = '.';
    }

    log_text[pos] = '\0';

    if (pos == 0) {
        osal_printk("[mine slave] UART2 rx len:%u (dump failed)\r\n", (unsigned int)length);
        return;
    }

    if (truncated) {
        osal_printk("[mine slave] UART2 rx len:%u show:%u data:%s ...\r\n",
            (unsigned int)length, (unsigned int)show_len, log_text);
    } else {
        osal_printk("[mine slave] UART2 rx len:%u data:%s\r\n", (unsigned int)length, log_text);
    }
}
#endif

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
    mine_sle_uart_ring_msg_t ring_msg = {0};
    mine_sle_uart_ring_slot_t *slot = NULL;
    int32_t slot_index;
    int write_ret;

    unused(error);

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

#if (MINE_UART2_RX_ISR_DUMP_ENABLE == 1)
    if (bus == UART_BUS_2) {
        /* RX 回调通常运行在中断上下文，仅在任务上下文允许详细串口转储。 */
        if (osal_in_interrupt() == 0) {
            mine_sle_uart_slave_dump_uart2_rx((const uint8_t *)buffer, length);
        }
    }
#endif

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
#endif

    /*
     * 链路未就绪时直接丢弃透传数据，不做 vmalloc/入队，避免在建链窗口堆积无效负载。
     * 调试命令路径已在前面优先消费，不受该策略影响。
     */
    if (!mine_sle_uart_slave_link_ready()) {
        return;
    }

    if (length > MINE_UART_RX_BUFFER_SIZE) {
        mine_uart_ring_log_drop_rate_limited(bus, length);
        return;
    }

    slot_index = mine_uart_ring_alloc_slot();
    if (slot_index < 0) {
        mine_uart_ring_log_drop_rate_limited(bus, length);
        return;
    }

    slot = &g_mine_uart_ring_slots[(uint8_t)slot_index];
    slot->uart_bus = (uint8_t)bus;
    slot->value_len = length;
    if (memcpy_s(slot->value, sizeof(slot->value), buffer, length) != EOK) {
        mine_uart_ring_free_slot((uint8_t)slot_index);
        return;
    }

    ring_msg.slot_index = (uint8_t)slot_index;

    write_ret = osal_msg_queue_write_copy(g_mine_uart_msg_queue, &ring_msg, g_mine_uart_msg_size, 0);
    if (write_ret != OSAL_SUCCESS) {
        mine_uart_ring_free_slot((uint8_t)slot_index);
        osal_printk("[mine slave] ring enqueue failed:%d\r\n", write_ret);
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
#if MINE_UART2_PASSTHROUGH_ENABLE
    mine_slave_oled_push_state("UART2 NORMAL");
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
        mine_sle_uart_ring_msg_t ring_msg = {0};
        mine_sle_uart_ring_slot_t *slot = NULL;
        mine_sle_uart_slave_msg_t msg = {0};
        unsigned int read_size = g_mine_uart_msg_size;

        read_ret = osal_msg_queue_read_copy(g_mine_uart_msg_queue, &ring_msg,
            &read_size, MINE_TASK_LOOP_WAIT_MS);
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
        }
#endif
        mine_slave_oled_flush_pending();
        if (read_ret != OSAL_SUCCESS) {
            continue;
        }

        if (ring_msg.slot_index >= MINE_UART_RING_SLOT_COUNT) {
            continue;
        }

        slot = &g_mine_uart_ring_slots[ring_msg.slot_index];
        if ((!slot->in_use) || (slot->value_len == 0)) {
            continue;
        }

        msg.uart_bus = slot->uart_bus;
        msg.value = slot->value;
        msg.value_len = slot->value_len;

        if ((msg.value != NULL) && (msg.value_len > 0)) {
#if (MINE_UART_LINK_TRACE_ENABLE == 1)
            osal_printk("[mine slave] %s rx queue len:%u\r\n",
                mine_slave_uart_bus_name(msg.uart_bus), msg.value_len);
#endif
            /* 发送函数内部已按场景分类打印，主循环避免重复错误日志刷屏。 */
            (void)mine_sle_uart_slave_send_to_host(&msg);
        }

        mine_uart_ring_free_slot(ring_msg.slot_index);
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

    mine_uart_ring_reset();

    create_ret = osal_msg_queue_create("mine_sle_slave_msg", MINE_UART_RING_SLOT_COUNT,
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
