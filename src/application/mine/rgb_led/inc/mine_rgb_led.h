/**
 * @file mine_rgb_led.h
 * @brief Mine单颗RGB灯珠(WS2812B时序兼容)驱动接口。
 */

#ifndef MINE_RGB_LED_H
#define MINE_RGB_LED_H

#include <stdint.h>
#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化RGB灯珠输出引脚与时序校准参数。
 *
 * @return ERRCODE_SUCC 初始化成功。
 * @return Other       初始化失败。
 */
errcode_t mine_rgb_led_init(void);

/**
 * @brief 设置单颗灯珠的RGB颜色并立即发送。
 *
 * @param r 红色亮度(0~255)。
 * @param g 绿色亮度(0~255)。
 * @param b 蓝色亮度(0~255)。
 */
void mine_rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 关闭单颗灯珠。
 */
void mine_rgb_led_off(void);

/**
 * @brief 与 STM32 示例保持一致的灯珠数量宏（兼容迁移代码）。
 */
#define Led_Num 30U

/**
 * @brief 与 STM32 示例保持一致的占空比缓冲区（24bit * Led_Num）。
 *
 * 说明：
 * - 该数组沿用原工程命名，便于上层最小改动迁移；
 * - 典型填充值：0 码使用 30，1 码使用 60。
 */
extern uint16_t WS2812_Value[24U * Led_Num];

/**
 * @brief STM32 风格初始化接口：完成 GPIO/时序初始化并执行清屏首帧。
 */
void WS2812_Init(void);

/**
 * @brief STM32 风格显示接口：发送前 Num+1 个灯珠数据。
 *
 * @param Num 最后一个灯珠下标（0 表示只发送第 1 颗）。
 */
void WS2812_Show(uint8_t Num);

/**
 * @brief STM32 风格清零接口：将 WS2812_Value 全部置为 0 码占空比。
 */
void WS2812_Clear(void);

/**
 * @brief STM32 风格复位接口：拉低 DIN 并等待复位时间。
 */
void WS2812_rest(void);

/**
 * @brief 迁移辅助接口：按 RGB 设置指定灯珠数据到 WS2812_Value。
 *
 * @param led_index 灯珠索引（0 ~ Led_Num-1）。
 * @param r 红色亮度。
 * @param g 绿色亮度。
 * @param b 蓝色亮度。
 */
void WS2812_SetRGB(uint16_t led_index, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
