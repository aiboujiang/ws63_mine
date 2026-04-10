/**
 * @file ws63_final_bsp_uart.c
 * @brief WK2114 最终版 BSP UART 子模块实现。
 */

#include "ws63_final_bsp.h"

#include "osal_debug.h"
#include "pinctrl.h"
#include "uart.h"

#include "ws63_final_config.h"

/*
 * 软轮询读参数：
 * 1) 每次仅按 1 字节读取，避免底层读接口在“目标长度未满足”时长时间自旋；
 * 2) 每轮空闲时短睡眠 1ms，让出 CPU，防止任务长期占满导致看门狗/NMI。
 */
#define WS63_BSP_UART_POLL_SLEEP_MS      1U
#define WS63_BSP_UART_DRAIN_MAX_BYTES    512U

/**
 * @brief 安全读取单字节：仅在 FIFO 非空时触发底层读。
 */
static int32_t ws63_bsp_uart_read_one_if_ready(uart_bus_t bus, uint8_t *byte)
{
    if (byte == NULL) {
        return -1;
    }

    /*
     * 先看 FIFO 状态再读，避免直接调用 uapi_uart_read 在某些配置下进入长时间轮询。
     */
    if (uapi_uart_rx_fifo_is_empty(bus)) {
        return 0;
    }

    return uapi_uart_read(bus, byte, 1U, 0U);
}

/**
 * @brief BSP 层安全 UART 读取封装（按字节轮询 + 软超时）。
 */
static int32_t ws63_bsp_uart_read_safe(uart_bus_t bus, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint16_t read_count = 0U;
    uint32_t waited_ms = 0U;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    while (read_count < len) {
        int32_t one_ret = ws63_bsp_uart_read_one_if_ready(bus, &data[read_count]);
        if (one_ret > 0) {
            read_count++;
            continue;
        }

        /* timeout=0 表示非阻塞读取：当前无数据立即返回。 */
        if (timeout_ms == 0U) {
            break;
        }

        ws63_bsp_sleep_ms(WS63_BSP_UART_POLL_SLEEP_MS);
        waited_ms += WS63_BSP_UART_POLL_SLEEP_MS;
        if (waited_ms >= timeout_ms) {
            break;
        }
    }

    return (int32_t)read_count;
}

/**
 * @brief 初始化主口 UART。
 */
errcode_t ws63_bsp_host_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len)
{
    uart_attr_t attr = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };
    uart_pin_config_t pin_cfg = {
        .tx_pin = WS63_HOST_UART_TX_PIN,
        .rx_pin = WS63_HOST_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uart_buffer_config_t rx_cfg;

    if ((rx_buffer == NULL) || (rx_buffer_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    uapi_pin_set_mode(WS63_HOST_UART_TX_PIN, WS63_HOST_UART_PIN_MODE);
    uapi_pin_set_mode(WS63_HOST_UART_RX_PIN, WS63_HOST_UART_PIN_MODE);

    rx_cfg.rx_buffer = rx_buffer;
    rx_cfg.rx_buffer_size = rx_buffer_len;

    (void)uapi_uart_deinit(WS63_HOST_UART_BUS);
    if (uapi_uart_init(WS63_HOST_UART_BUS, &pin_cfg, &attr, NULL, &rx_cfg) != ERRCODE_SUCC) {
        osal_printk("[wk2114 final bsp] host uart init fail\r\n");
        return ERRCODE_FAIL;
    }

    osal_printk("[wk2114 final bsp] host uart init ok\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 主口 UART 发送适配。
 */
int32_t ws63_bsp_host_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return uapi_uart_write(WS63_HOST_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 主口 UART 读取适配。
 */
int32_t ws63_bsp_host_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return ws63_bsp_uart_read_safe(WS63_HOST_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 清空主口 UART 接收缓冲区中的残留数据。
 */
errcode_t ws63_bsp_host_uart_flush_rx(void)
{
    uint8_t dummy = 0U;
    uint16_t drain_cnt;

    /*
     * 兼容实现：循环非阻塞读取直到无数据。
     * 避免依赖某些构建配置下未导出的 uapi_uart_flush_rx_data 符号。
     */
    for (drain_cnt = 0U; drain_cnt < WS63_BSP_UART_DRAIN_MAX_BYTES; drain_cnt++) {
        if (ws63_bsp_uart_read_safe(WS63_HOST_UART_BUS, &dummy, 1U, 0U) <= 0) {
            break;
        }
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 初始化调试串口。
 */
errcode_t ws63_bsp_debug_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len)
{
    uart_attr_t attr = {
        .baud_rate = WS63_DEBUG_UART_BAUD,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };
    uart_pin_config_t pin_cfg = {
        .tx_pin = WS63_DEBUG_UART_TX_PIN,
        .rx_pin = WS63_DEBUG_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uart_buffer_config_t rx_cfg;

    if ((rx_buffer == NULL) || (rx_buffer_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    (void)uapi_pin_set_mode(WS63_DEBUG_UART_TX_PIN, WS63_DEBUG_UART_PIN_MODE);
    (void)uapi_pin_set_mode(WS63_DEBUG_UART_RX_PIN, WS63_DEBUG_UART_PIN_MODE);

    rx_cfg.rx_buffer = rx_buffer;
    rx_cfg.rx_buffer_size = rx_buffer_len;

    /*
     * 先 deinit 再 init：
     * 避免调试命令口与系统其他 UART 用户（如 AT）共享同一总线时发生并发抢读。
     */
    (void)uapi_uart_deinit(WS63_DEBUG_UART_BUS);

    if (uapi_uart_init(WS63_DEBUG_UART_BUS, &pin_cfg, &attr, NULL, &rx_cfg) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 调试串口写数据。
 */
int32_t ws63_bsp_debug_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return uapi_uart_write(WS63_DEBUG_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 调试串口读数据。
 */
int32_t ws63_bsp_debug_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return ws63_bsp_uart_read_safe(WS63_DEBUG_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 注册调试串口接收回调。
 */
errcode_t ws63_bsp_debug_uart_register_rx_callback(ws63_bsp_uart_rx_callback_t callback, uint16_t min_len)
{
    uint16_t threshold;

    if (callback == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    threshold = (min_len == 0U) ? 1U : min_len;
    if (uapi_uart_register_rx_callback(WS63_DEBUG_UART_BUS,
        UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE,
        threshold,
        callback) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}
