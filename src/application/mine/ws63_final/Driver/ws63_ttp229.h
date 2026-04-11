/**
 * @file ws63_ttp229.h
 * @brief WS63 TTP229 驱动层接口。
 */

#ifndef WS63_TTP229_H
#define WS63_TTP229_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief TTP229 一次采样结果。
 *
 * 说明：
 * 1) raw_code 为芯片通过 I2C 直接读回的 16 位原始键值；
 * 2) pressed_mask 保持与手册一致的语义：位为 1 表示按下，位为 0 表示未按下。
 */
typedef struct {
    uint16_t raw_code;
    uint16_t pressed_mask;
    uint8_t pressed_count;
    uint8_t multi_key;
} ws63_ttp229_sample_t;

/**
 * @brief 初始化 TTP229 驱动。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_ttp229_init(void);

/**
 * @brief 读取一次 TTP229 按键状态。
 *
 * @param sample 输出采样结果。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_ttp229_read_sample(ws63_ttp229_sample_t *sample);

/**
 * @brief 统计按下按键数量。
 *
 * @param pressed_mask 位图（位为1表示按下）。
 * @return uint8_t 按下键数（0~16）。
 */
uint8_t ws63_ttp229_count_pressed(uint16_t pressed_mask);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
