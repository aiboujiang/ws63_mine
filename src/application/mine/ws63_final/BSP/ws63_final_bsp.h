/**
 * @file ws63_final_bsp.h
 * @brief WK2114 最终版 BSP/HAL 层接口。
 *
 * 约束：
 * 1) 本层是唯一允许直接访问 uapi_gpio/uapi_uart 的层；
 * 2) 上层通过本层接口访问硬件，不允许跨层直接触摸寄存器或外设 API。
 */

#ifndef MINE_WS63_FINAL_BSP_H
#define MINE_WS63_FINAL_BSP_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化 WK2114 主口使用的底层 UART。
 *
 * @param rx_buffer     接收缓冲区。
 * @param rx_buffer_len 缓冲区长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t mine_ws63_final_bsp_host_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len);

/**
 * @brief 通过主口 UART 发送数据。
 *
 * @param data       数据缓冲区。
 * @param len        发送长度。
 * @param timeout_ms 发送超时。
 * @return int32_t >=0 表示写入字节数，<0 表示失败。
 */
int32_t mine_ws63_final_bsp_host_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 通过主口 UART 读取数据。
 *
 * @param data       输出缓冲区。
 * @param len        期望读取长度。
 * @param timeout_ms 读取超时。
 * @return int32_t >0 表示读取字节数，<=0 表示无数据或失败。
 */
int32_t mine_ws63_final_bsp_host_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 初始化 WK2114 复位引脚。
 */
void mine_ws63_final_bsp_reset_init(void);

/**
 * @brief 拉高/拉低 WK2114 复位引脚。
 *
 * @param level_high 1=高电平，0=低电平。
 */
void mine_ws63_final_bsp_reset_set(uint8_t level_high);

/**
 * @brief 初始化 WK2114 IRQ 引脚。
 */
void mine_ws63_final_bsp_irq_init(void);

/**
 * @brief 毫秒延时。
 *
 * @param ms 延时毫秒。
 */
void mine_ws63_final_bsp_sleep_ms(uint32_t ms);

/**
 * @brief 获取系统毫秒计时。
 *
 * @return uint32_t 当前毫秒值。
 */
uint32_t mine_ws63_final_bsp_get_tick_ms(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
