/**
 * @file ws63_debug_uart.h
 * @brief WS63 调试 UART 驱动层接口。
 *
 * 说明：
 * 1) 对上提供调试串口能力接口；
 * 2) 对下通过 BSP 访问具体 UART 硬件；
 * 3) 应用层禁止直接依赖 BSP 调试 UART 接口。
 */

#ifndef WS63_DEBUG_UART_H
#define WS63_DEBUG_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 调试 UART 接收回调类型。
 *
 * @param buffer 接收缓冲区。
 * @param length 本次接收长度。
 * @param error  是否发生接收错误。
 */
typedef void (*ws63_debug_uart_rx_callback_t)(const void *buffer, uint16_t length, bool error);

/**
 * @brief 初始化调试 UART。
 *
 * @param rx_buffer     接收缓冲区。
 * @param rx_buffer_len 接收缓冲区长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_debug_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len);

/**
 * @brief 注册调试 UART 接收回调。
 *
 * @param callback 回调函数。
 * @param min_len  触发回调最小长度，0 表示默认 1。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_debug_uart_register_rx_callback(ws63_debug_uart_rx_callback_t callback, uint16_t min_len);

/**
 * @brief 通过调试 UART 发送数据。
 *
 * @param data       发送缓冲区。
 * @param len        发送长度。
 * @param timeout_ms 超时毫秒。
 * @return int32_t >=0 表示写入字节数，<0 表示失败。
 */
int32_t ws63_debug_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
