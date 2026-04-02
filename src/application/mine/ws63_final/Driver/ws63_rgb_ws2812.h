/**
 * @file ws63_rgb_ws2812.h
 * @brief WS63 Final 分层框架的 WS2812 RGB 驱动接口。
 *
 * 说明：
 * 1) 本层仅提供设备驱动语义，不直接暴露 pinctrl/SPI 配置细节；
 * 2) 硬件初始化与发送动作通过 BSP 层接口完成。
 */

#ifndef WS63_RGB_WS2812_H
#define WS63_RGB_WS2812_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief RGB 颜色结构（按 R/G/B 表达，驱动内部自动转为 WS2812 所需 GRB 顺序）。
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws63_rgb_color_t;

/**
 * @brief 初始化 WS2812 驱动。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_rgb_ws2812_init(void);

/**
 * @brief 设置单颗 WS2812 颜色。
 *
 * @param color 目标颜色。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_rgb_ws2812_set_color(const ws63_rgb_color_t *color);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
