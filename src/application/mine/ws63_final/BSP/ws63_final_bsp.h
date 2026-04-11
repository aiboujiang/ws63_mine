/**
 * @file ws63_final_bsp.h
 * @brief WK2114 最终版 BSP/HAL 层接口。
 *
 * 约束：
 * 1) 本层是唯一允许直接访问 uapi_gpio/uapi_uart 的层；
 * 2) 上层通过本层接口访问硬件，不允许跨层直接触摸寄存器或外设 API。
 */

#ifndef WS63_BSP_H
#define WS63_BSP_H

#include <stdint.h>
#include <stdbool.h>

#include "errcode.h"
#include "platform_core.h"

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
errcode_t ws63_bsp_host_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len);

/**
 * @brief 通过主口 UART 发送数据。
 *
 * @param data       数据缓冲区。
 * @param len        发送长度。
 * @param timeout_ms 发送超时。
 * @return int32_t >=0 表示写入字节数，<0 表示失败。
 */
int32_t ws63_bsp_host_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 通过主口 UART 读取数据。
 *
 * @param data       输出缓冲区。
 * @param len        期望读取长度。
 * @param timeout_ms 读取超时。
 * @return int32_t >0 表示读取字节数，<=0 表示无数据或失败。
 */
int32_t ws63_bsp_host_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 清空主口 UART 接收缓冲区中的残留数据。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_host_uart_flush_rx(void);

/**
 * @brief 初始化 WK2114 复位引脚。
 */
void ws63_bsp_reset_init(void);

/**
 * @brief 拉高/拉低 WK2114 复位引脚。
 *
 * @param level_high 1=高电平，0=低电平。
 */
void ws63_bsp_reset_set(uint8_t level_high);

/**
 * @brief 初始化 WK2114 IRQ 引脚。
 */
void ws63_bsp_irq_init(void);

/**
 * @brief 毫秒延时。
 *
 * @param ms 延时毫秒。
 */
void ws63_bsp_sleep_ms(uint32_t ms);

/**
 * @brief 获取系统毫秒计时。
 *
 * @return uint32_t 当前毫秒值。
 */
uint32_t ws63_bsp_get_tick_ms(void);

/**
 * @brief 初始化 RGB WS2812 使用的 SPI1 输出链路。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_rgb_spi_init(void);

/**
 * @brief 通过 RGB SPI1 链路发送编码后的 WS2812 帧数据。
 *
 * @param tx_buf     待发送缓冲区。
 * @param tx_bytes   发送字节数。
 * @param timeout_ms SPI 发送超时（毫秒）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_rgb_spi_write(const uint8_t *tx_buf, uint32_t tx_bytes, uint32_t timeout_ms);

/**
 * @brief GPIO 中断回调类型。
 */
typedef void (*ws63_bsp_gpio_callback_t)(pin_t pin, uintptr_t param);

/**
 * @brief UART 接收回调类型。
 *
 * @param buffer 接收数据缓冲区。
 * @param length 本次接收长度。
 * @param error  是否发生接收错误。
 */
typedef void (*ws63_bsp_uart_rx_callback_t)(const void *buffer, uint16_t length, bool error);

/**
 * @brief 初始化电机底层资源（GPIO/PWM）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_motor_init(void);

/**
 * @brief 关闭电机 PWM 输出并恢复 IA/IB 为 GPIO 模式。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_motor_disable_pwm(void);

/**
 * @brief 设置电机 IA/IB GPIO 电平。
 *
 * @param ia_high IA 目标电平（1=高，0=低）。
 * @param ib_high IB 目标电平（1=高，0=低）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_motor_set_level(uint8_t ia_high, uint8_t ib_high);

/**
 * @brief 使能 IA 通道 PWM 输出。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_motor_enable_pwm_ia(uint8_t duty_percent);

/**
 * @brief 使能 IB 通道 PWM 输出。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_motor_enable_pwm_ib(uint8_t duty_percent);

/**
 * @brief 初始化编码器 IO。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_encoder_init(void);

/**
 * @brief 注册编码器 A 相上升沿中断回调。
 *
 * @param callback A 相中断回调函数。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_encoder_register_a_isr(ws63_bsp_gpio_callback_t callback);

/**
 * @brief 读取编码器 B 相电平。
 *
 * @return uint8_t 1=高电平，0=低电平。
 */
uint8_t ws63_bsp_encoder_get_b_level(void);

/**
 * @brief 初始化调试串口。
 *
 * @param rx_buffer     接收缓冲区。
 * @param rx_buffer_len 接收缓冲区长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_debug_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len);

/**
 * @brief 调试串口写数据。
 *
 * @param data       数据缓冲区。
 * @param len        数据长度。
 * @param timeout_ms 超时时间。
 * @return int32_t >=0 表示写入字节数，<0 表示失败。
 */
int32_t ws63_bsp_debug_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 调试串口读数据。
 *
 * @param data       输出缓冲区。
 * @param len        最大读取长度。
 * @param timeout_ms 超时时间。
 * @return int32_t >0 表示读取字节数，<=0 表示无数据或失败。
 */
int32_t ws63_bsp_debug_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 注册调试串口接收回调。
 *
 * @param callback 回调函数。
 * @param min_len  触发回调的最小长度，传 0 使用默认值 1。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_debug_uart_register_rx_callback(ws63_bsp_uart_rx_callback_t callback, uint16_t min_len);

/**
 * @brief 初始化蜂鸣器底层资源（GPIO/PWM）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_beep_init(void);

/**
 * @brief 使能蜂鸣器连续发声。
 *
 * @param freq_hz 目标频率（Hz）。
 * @param volume_percent 音量百分比（映射为 PWM 占空比）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_beep_start(uint16_t freq_hz, uint8_t volume_percent);

/**
 * @brief 停止蜂鸣器并将引脚拉低静音。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_beep_stop(void);

/**
 * @brief 初始化 TTP229 I2C 相关引脚与总线资源。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_ttp229_init(void);

/**
 * @brief 通过 TTP229 I2C 总线读取键值数据。
 *
 * @param data 输出缓冲区，至少 2 字节。
 * @param len  读取长度，必须不小于 2 字节。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_bsp_ttp229_read_bytes(uint8_t *data, uint16_t len);

/**
 * @brief TTP229 毫秒级延时。
 *
 * @param ms 延时毫秒。
 */
void ws63_bsp_ttp229_delay_ms(uint32_t ms);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
