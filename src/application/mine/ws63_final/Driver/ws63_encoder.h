/**
 * @file ws63_encoder.h
 * @brief WS63 编码器测速驱动层接口。
 *
 * 说明：
 * 1) 使用 A 相上升沿中断计数，并结合 B 相电平判定方向；
 * 2) 周期采样输出有符号 RPM（正负代表方向）。
 */

#ifndef WS63_ENCODER_H
#define WS63_ENCODER_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化编码器驱动。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_encoder_init(void);

/**
 * @brief 周期采样编码器，更新最新 RPM。
 *
 * @param now_ms 当前系统毫秒计时。
 */
void ws63_encoder_sample(uint32_t now_ms);

/**
 * @brief 获取最新 RPM。
 *
 * @return int32_t 有符号 RPM。
 */
int32_t ws63_encoder_get_rpm(void);

/**
 * @brief 获取上一个采样窗口的增量计数。
 *
 * @return int32_t 有符号脉冲增量。
 */
int32_t ws63_encoder_get_last_delta(void);

/**
 * @brief 获取累计计数值。
 *
 * @return int32_t 有符号累计脉冲。
 */
int32_t ws63_encoder_get_total_count(void);

/**
 * @brief 清零测速与计数状态。
 */
void ws63_encoder_reset(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif