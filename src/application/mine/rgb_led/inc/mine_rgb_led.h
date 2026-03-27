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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
