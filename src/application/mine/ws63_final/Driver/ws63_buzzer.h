/**
 * @file ws63_buzzer.h
 * @brief WS63 蜂鸣器驱动层接口。
 *
 * 说明：
 * 1) 本层封装蜂鸣器业务语义（开/关/频率/音量）；
 * 2) 具体 GPIO/PWM 访问全部下沉到 BSP；
 * 3) 应用层通过 Task 接口调用，不直接依赖本驱动。
 */

#ifndef WS63_BUZZER_H
#define WS63_BUZZER_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化蜂鸣器驱动。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_buzzer_init(void);

/**
 * @brief 打开蜂鸣器并以指定频率连续发声。
 *
 * @param freq_hz 目标频率（Hz）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_buzzer_start(uint16_t freq_hz);

/**
 * @brief 设置蜂鸣器音量。
 *
 * 说明：音量映射为 PWM 占空比百分比。
 * 若蜂鸣器当前处于开启状态，设置后会立即按新音量重配置输出。
 *
 * @param volume_percent 音量百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_buzzer_set_volume(uint8_t volume_percent);

/**
 * @brief 关闭蜂鸣器。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_buzzer_stop(void);

/**
 * @brief 查询蜂鸣器当前是否处于发声状态。
 *
 * @return uint8_t 1=正在发声，0=静音。
 */
uint8_t ws63_buzzer_is_on(void);

/**
 * @brief 获取当前蜂鸣器工作频率（Hz）。
 *
 * @return uint16_t 当前频率。
 */
uint16_t ws63_buzzer_get_freq_hz(void);

/**
 * @brief 获取当前蜂鸣器音量百分比。
 *
 * @return uint8_t 当前音量百分比。
 */
uint8_t ws63_buzzer_get_volume(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
