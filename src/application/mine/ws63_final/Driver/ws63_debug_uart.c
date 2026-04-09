/**
 * @file ws63_debug_uart.c
 * @brief WS63 调试 UART 驱动层实现。
 */

#include "ws63_debug_uart.h"

#include "ws63_final_bsp.h"

/**
 * @brief 初始化调试 UART。
 */
errcode_t ws63_debug_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len)
{
    return ws63_bsp_debug_uart_init(rx_buffer, rx_buffer_len);
}

/**
 * @brief 注册调试 UART 接收回调。
 */
errcode_t ws63_debug_uart_register_rx_callback(ws63_debug_uart_rx_callback_t callback, uint16_t min_len)
{
    return ws63_bsp_debug_uart_register_rx_callback(callback, min_len);
}

/**
 * @brief 通过调试 UART 发送数据。
 */
int32_t ws63_debug_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return ws63_bsp_debug_uart_write(data, len, timeout_ms);
}
