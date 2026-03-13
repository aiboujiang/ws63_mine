/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Slave side UART0 <-> SLE bridge.
 */

#include "sle_uart_slave.h"
#include "sle_uart_slave_module.h"

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

#include "LD2402/LD2402.h"
#include "ZW101/zw101_protocol.h"

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
static unsigned int g_mine_uart_msg_size = sizeof(mine_sle_uart_slave_msg_t);

#if MINE_LD2402_ENABLE
static LD2402_Handle_t g_ld2402_handle;
static bool g_ld2402_ready = false;
static uart_bus_t g_ld2402_bus = MINE_LD2402_UART_BUS;
static volatile bool g_ld2402_status_dirty = false;
static char g_ld2402_status_text[MINE_LD2402_STATUS_TEXT_LEN] = "RADAR:OFF";
#endif

#if MINE_ZW101_ENABLE
static zw101_context_t g_zw101_ctx;
static bool g_zw101_ready = false;
static uart_bus_t g_zw101_bus = MINE_ZW101_UART_BUS;
static volatile bool g_zw101_status_dirty = false;
static char g_zw101_status_text[MINE_ZW101_STATUS_TEXT_LEN] = "ZW101:OFF";

#define MINE_ZW101_HANDSHAKE_RETRY 3
#define MINE_ZW101_HANDSHAKE_RETRY_GAP_MS 40
#endif

/* Keep original OSAL log sink and mirror to PRINT channel. */
static void (*g_mine_raw_osal_printk)(const char *fmt, ...) = osal_printk;

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
    PRINT("%s", log_buf);
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

#if MINE_LD2402_ENABLE
/**
 * @brief LD2402 HAL 层串口发送实现。
 *
 * @param data 发送数据。
 * @param len  数据长度。
 * @return int 0 成功，-1 失败。
 */
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

/**
 * @brief 提供 LD2402 使用的毫秒时间基。
 *
 * @return uint32_t 当前系统毫秒计数。
 */
static uint32_t ld2402_get_tick_ms(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

/**
 * @brief 提供 LD2402 使用的阻塞延时。
 *
 * @param ms 延时毫秒数。
 */
static void ld2402_delay_ms(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 更新 LD2402 状态字符串并置脏标记。
 *
 * @param text 状态文本。
 */
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

/**
 * @brief LD2402 数据帧回调。
 *
 * 将运动状态与距离压缩为 OLED 可显示文本。
 *
 * @param data 解析后的雷达数据帧。
 */
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

/**
 * @brief 初始化 LD2402 模块并读取基础信息。
 *
 * @param bus 雷达所在 UART 总线。
 * @return true  初始化成功。
 * @return false 初始化失败。
 */
static bool ld2402_init(uart_bus_t bus)
{
    LD2402_HAL_t hal = {0};
    char version[24] = {0};
    uint8_t sn_buf[MINE_LD2402_SN_MAX_LEN] = {0};
    int sn_len;

    g_ld2402_ready = false;
    g_ld2402_bus = bus;

    if (!mine_slave_uart_bus_enabled(bus)) {
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

/**
 * @brief 将 UART 原始字节流喂给 LD2402 协议解析器。
 *
 * @param bus  数据来源总线。
 * @param data 字节流数据。
 * @param len  数据长度。
 */
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

/**
 * @brief 读取一份待刷新的 LD2402 状态文本。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return true  读取成功且有新状态。
 * @return false 无新状态或读取失败。
 */
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

#if MINE_ZW101_ENABLE
/**
 * @brief ZW101 HAL 串口发送适配。
 */
static int zw101_uart_send_adapter(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0)) {
        return -1;
    }

    if (uapi_uart_write(g_zw101_bus, data, len, 0) < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief ZW101 HAL 毫秒计时适配。
 */
static uint32_t zw101_get_tick_ms_adapter(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

/**
 * @brief ZW101 HAL 延时适配。
 */
static void zw101_delay_ms_adapter(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 更新 ZW101 状态文本并置脏。
 */
static void zw101_set_status(const char *text)
{
    if (text == NULL) {
        return;
    }

    if (snprintf_s(g_zw101_status_text, sizeof(g_zw101_status_text),
        sizeof(g_zw101_status_text) - 1, "%s", text) > 0) {
        g_zw101_status_dirty = true;
    }
}

/**
 * @brief ZW101 ACK 回调。
 */
static void zw101_ack_callback(const zw101_ack_evt_t *evt)
{
    if (evt == NULL) {
        return;
    }

    if (evt->ack_code == ZW101_ACK_SUCCESS) {
        zw101_set_status("ZW101:ACK OK");
    } else if (evt->ack_code == ZW101_ACK_ERR_NO_FINGER) {
        zw101_set_status("ZW101:NO FINGER");
    } else {
        char status[MINE_ZW101_STATUS_TEXT_LEN] = {0};
        if (snprintf_s(status, sizeof(status), sizeof(status) - 1,
            "ZW101:C%02X E%02X", evt->cmd, evt->ack_code) > 0) {
            zw101_set_status(status);
        }
    }
}

/**
 * @brief 初始化 ZW101 模块并执行握手。
 */
static bool zw101_module_init(uart_bus_t bus)
{
    zw101_hal_t hal = {0};
    uint8_t ack_code = 0xFF;
    uint8_t retry_idx;

    g_zw101_ready = false;
    g_zw101_bus = bus;

    if (!mine_slave_uart_bus_enabled(bus)) {
        zw101_set_status("ZW101:BUS OFF");
        return false;
    }

    hal.uart_send = zw101_uart_send_adapter;
    hal.get_tick_ms = zw101_get_tick_ms_adapter;
    hal.delay_ms = zw101_delay_ms_adapter;

    zw101_init(&g_zw101_ctx, &hal);
    zw101_set_callbacks(&g_zw101_ctx, zw101_ack_callback, NULL);
    zw101_reset_protocol_parse(&g_zw101_ctx);

    /*
     * 手册建议上电后等待模组初始化完成（无 0x55 信号时 M 系列建议 80ms）。
     * 这里先等待一个上电窗口，再做握手重试，减少冷启动偶发失败。
     */
    (void)osal_msleep(ZW101_PWRON_WAIT_PERIOD);

    for (retry_idx = 0; retry_idx < MINE_ZW101_HANDSHAKE_RETRY; retry_idx++) {
        if (zw101_cmd_handshake(&g_zw101_ctx, &ack_code) == 0) {
            /* 校验传感器失败码 0x29 按手册定义为模块异常。 */
            if ((zw101_cmd_check_sensor(&g_zw101_ctx) != 0) &&
                (g_zw101_ctx.ack_code == 0x29)) {
                zw101_set_status("ZW101:SENSOR ERR");
                return false;
            }

            zw101_set_status("ZW101:READY");
            g_zw101_ready = true;
            return true;
        }

        (void)osal_msleep(MINE_ZW101_HANDSHAKE_RETRY_GAP_MS);
    }

    zw101_set_status("ZW101:WAIT HS");
    return false;
}

/**
 * @brief 向 ZW101 协议解析器喂入串口数据。
 */
static void zw101_feed(uart_bus_t bus, const uint8_t *data, uint16_t len)
{
    if ((bus != g_zw101_bus) || (data == NULL) || (len == 0)) {
        return;
    }

    zw101_protocol_parse(&g_zw101_ctx, data, len);
}

/**
 * @brief 获取待刷新的 ZW101 状态文本。
 */
static bool zw101_get_status(char *buf, uint16_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0) || (!g_zw101_status_dirty)) {
        return false;
    }

    if (snprintf_s(buf, buf_len, buf_len - 1, "%s", g_zw101_status_text) <= 0) {
        return false;
    }

    g_zw101_status_dirty = false;
    return true;
}
#endif

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
    int write_ret;

    unused(error);

    if ((buffer == NULL) || (length == 0)) {
        return;
    }

#if MINE_LD2402_ENABLE
    ld2402_feed(bus, (const uint8_t *)buffer, length);
#endif
#if MINE_ZW101_ENABLE
    zw101_feed(bus, (const uint8_t *)buffer, length);
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
    char radar_status[24] = {0};
    char zw101_status[24] = {0};

    unused(arg);
    osal_msleep(MINE_INIT_DELAY_MS);

    osal_printk("[mine slave] task start\r\n");
    mine_slave_oled_init();
    mine_sle_uart_slave_uart_init();
#if MINE_LD2402_ENABLE
    if (ld2402_init(MINE_LD2402_UART_BUS)) {
        mine_slave_oled_push_state("LD2402 READY");
    } else {
        mine_slave_oled_push_state("LD2402 WAIT");
    }
#endif
#if MINE_ZW101_ENABLE
    if (zw101_module_init(MINE_ZW101_UART_BUS)) {
        mine_slave_oled_push_state("ZW101 READY");
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
        if (ld2402_get_status(radar_status, sizeof(radar_status))) {
            mine_slave_oled_push_state(radar_status);
        }
#endif
#if MINE_ZW101_ENABLE
    if (zw101_get_status(zw101_status, sizeof(zw101_status))) {
        mine_slave_oled_push_state(zw101_status);
    }
#endif
        mine_slave_oled_flush_pending();
        if (read_ret != OSAL_SUCCESS) {
            continue;
        }

        if ((msg.value != NULL) && (msg.value_len > 0)) {
            osal_printk("[mine slave] %s rx queue len:%u\r\n",
                mine_slave_uart_bus_name(msg.uart_bus), msg.value_len);
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
