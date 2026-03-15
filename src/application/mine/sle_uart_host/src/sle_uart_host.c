/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Host side UART0 <-> SLE bridge.
 */

#include "sle_uart_host.h"
#include "sle_uart_host_module.h"

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "app_init.h"
#include "common_def.h"
#include "pinctrl.h"
#include "securec.h"
#include "sle_errcode.h"
#include "soc_osal.h"
#include "uart.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/* Multi-UART RX buffers (indexed by UART bus 0/1/2). */
static uint8_t g_mine_uart_rx_buffer[MINE_UART_BUS_COUNT][MINE_UART_RX_BUFFER_SIZE] = {0};

/* UART callback -> task queue. */
static unsigned long g_mine_uart_msg_queue = 0;
static unsigned int g_mine_uart_msg_size = sizeof(mine_sle_uart_host_msg_t);

/* Keep original OSAL log sink and mirror to PRINT channel. */
static void (*g_mine_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/**
 * @brief 将日志同步镜像到 UART0，保证串口调试口持续可见。
 *
 * 采用“尽力发送”策略：不额外打印失败日志，避免日志回路递归。
 *
 * @param log_buf    日志缓冲区。
 * @param format_len 已格式化日志长度。
 */
static void mine_host_log_mirror_uart0(const char *log_buf, int32_t format_len)
{
    if ((log_buf == NULL) || (format_len <= 0)) {
        return;
    }

    /* 保持 UART0 与系统日志同步输出，不因串口未就绪中断主流程。 */
    (void)uapi_uart_write(UART_BUS_0, (const uint8_t *)log_buf, (uint16_t)format_len, 0);
}

/**
 * @brief Host 统一日志接口，双路输出到 OSAL 与 PRINT。
 *
 * 该函数用于替换模块内直接 osal_printk 调用，
 * 既保留系统日志，又在调试串口中打印同一份内容。
 *
 * @param fmt printf 风格格式串。
 */
void mine_host_log(const char *fmt, ...)
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
    mine_host_log_mirror_uart0(log_buf, format_len);
}

#define osal_printk mine_host_log

/**
 * @brief 判断某 UART 总线是否在当前掩码下启用。
 *
 * @param bus UART 总线号。
 * @return true  总线启用。
 * @return false 总线未启用或越界。
 */
bool mine_host_uart_bus_enabled(uart_bus_t bus)
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
const char *mine_host_uart_bus_name(uint8_t bus)
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
 * @brief 向所有已启用 UART 广播写入数据。
 *
 * @param data 待发送数据指针。
 * @param len  数据长度。
 */
void mine_host_uart_write_enabled_buses(const uint8_t *data, uint16_t len)
{
    uint8_t bus_index;
    int32_t write_ret;

    if ((data == NULL) || (len == 0)) {
        return;
    }

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_host_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }

        write_ret = uapi_uart_write((uart_bus_t)bus_index, data, len, 0);
        if (write_ret < 0) {
            osal_printk("[mine host] %s write failed, ret=%d\r\n",
                mine_host_uart_bus_name(bus_index), (int)write_ret);
        }
    }
}

/**
 * @brief 统一处理 UART 回调数据并投递到 Host 任务消息队列。
 *
 * 该函数会在已连接状态下复制一份接收数据，
 * 由任务线程异步执行 UART->SLE 发送，避免在中断回调中做重操作。
 *
 * @param bus    数据来源 UART 总线。
 * @param buffer 接收缓冲区。
 * @param length 接收长度。
 * @param error  回调错误标志（当前未使用）。
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

    /* Drop UART payload while not connected to avoid queue storms. */
    if (!mine_sle_uart_host_is_connected()) {
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
static void mine_sle_uart_host_read_handler_uart0(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_read_handler_common(UART_BUS_0, buffer, length, error);
}

/**
 * @brief UART1 接收回调包装，转发到统一处理函数。
 */
static void mine_sle_uart_host_read_handler_uart1(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_read_handler_common(UART_BUS_1, buffer, length, error);
}

/**
 * @brief UART2 接收回调包装，转发到统一处理函数。
 */
static void mine_sle_uart_host_read_handler_uart2(const void *buffer, uint16_t length, bool error)
{
    mine_sle_uart_host_read_handler_common(UART_BUS_2, buffer, length, error);
}

/**
 * @brief 初始化单路 UART 并注册对应 RX 回调。
 *
 * @param bus 目标 UART 总线。
 * @return true  初始化成功。
 * @return false 初始化失败或参数不支持。
 */
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
    void (*rx_cb)(const void *, uint16_t, bool) = NULL;
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
        osal_printk("[mine host] %s pin not configured, skip\r\n", mine_host_uart_bus_name(bus_index));
        return false;
    }

    uart_buffer_cfg.rx_buffer = g_mine_uart_rx_buffer[bus_index];
    uart_buffer_cfg.rx_buffer_size = MINE_UART_RX_BUFFER_SIZE;

    uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
    uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

    (void)uapi_uart_deinit(bus);
    ret = uapi_uart_init(bus, &pin_cfg, &attr, NULL, &uart_buffer_cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine host] %s init failed, ret=%x\r\n", mine_host_uart_bus_name(bus_index), ret);
        return false;
    }

    ret = uapi_uart_register_rx_callback(bus, UART_RX_CONDITION_MASK_IDLE, 1, rx_cb);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine host] %s rx cb failed, ret=%x\r\n", mine_host_uart_bus_name(bus_index), ret);
        return false;
    }

    return true;
}

/**
 * @brief 按使能掩码初始化 Host 侧所有 UART 通道。
 *
 * 初始化后会将结果写入 OLED 状态行，便于现场定位串口资源问题。
 */
void mine_sle_uart_host_uart_init(void)
{
    uint8_t bus_index;
    uint8_t enabled_count = 0;
    uint8_t ok_count = 0;

    for (bus_index = 0; bus_index < MINE_UART_BUS_COUNT; bus_index++) {
        if (!mine_host_uart_bus_enabled((uart_bus_t)bus_index)) {
            continue;
        }
        enabled_count++;
        if (mine_sle_uart_host_uart_init_one((uart_bus_t)bus_index)) {
            ok_count++;
        }
    }

    osal_printk("[mine host] uart init summary, enabled:%u ok:%u\r\n", enabled_count, ok_count);
    if (ok_count > 0) {
        mine_host_oled_push_text("UART INIT OK");
    } else {
        mine_host_oled_push_text("UART INIT FAIL");
    }
}

/**
 * @brief Host 主任务线程。
 *
 * 负责串口、OLED、SLE 初始化以及队列消费，
 * 将 UART 接收数据通过 SSAPS notify 分片发送至远端。
 *
 * @param arg 任务入参（当前未使用）。
 * @return void* 任务结束返回值。
 */
static void *mine_sle_uart_host_task(const char *arg)
{
    int read_ret;
    errcode_t send_ret;

    unused(arg);
    osal_msleep(MINE_INIT_DELAY_MS);

    osal_printk("[mine host] task start\r\n");
    mine_host_oled_init();
    mine_sle_uart_host_uart_init();
    mine_host_oled_push_text("SLE INIT...");
    mine_host_oled_flush_pending();
    if (mine_sle_uart_host_init() != ERRCODE_SLE_SUCCESS) {
        mine_host_oled_push_text("HOST INIT FAIL");
        mine_host_oled_flush_pending();
        return NULL;
    }

    mine_host_oled_flush_pending();

    while (1) {
        mine_sle_uart_host_msg_t msg = {0};

        read_ret = osal_msg_queue_read_copy(g_mine_uart_msg_queue, &msg,
            &g_mine_uart_msg_size, MINE_TASK_LOOP_WAIT_MS);
        mine_host_oled_flush_pending();
        if (read_ret != OSAL_SUCCESS) {
            continue;
        }

        if ((msg.value != NULL) && (msg.value_len > 0)) {
            osal_printk("[mine host] %s rx queue len:%u\r\n",
                mine_host_uart_bus_name(msg.uart_bus), msg.value_len);
            send_ret = mine_sle_uart_host_send_by_handle(&msg);
            if (send_ret == ERRCODE_SLE_SUCCESS) {
                mine_host_oled_push_data_event(msg.uart_bus, "UART TX", msg.value, msg.value_len);
            } else if (mine_sle_uart_host_is_connected()) {
                mine_host_oled_push_text("SLE SEND FAIL");
            }
            osal_vfree(msg.value);
        }
    }
}

/**
 * @brief Host 应用入口。
 *
 * 负责创建 UART 消息队列与主任务线程，
 * 并设置线程优先级后交给系统调度执行。
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

app_run(mine_sle_uart_host_entry);
