/**
 * @file ws63_motor.h
 * @brief WS63 电机控制驱动层接口。
 *
 * 说明：
 * 1) 本层只封装“电机控制语义”，不直接调用底层 uapi；
 * 2) 具体 GPIO/PWM 访问统一下沉到 BSP 层实现。
 */

#ifndef WS63_MOTOR_H
#define WS63_MOTOR_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 电机状态定义。
 */
typedef enum {
    WS63_MOTOR_STATE_COAST = 0,
    WS63_MOTOR_STATE_FORWARD,
    WS63_MOTOR_STATE_REVERSE,
    WS63_MOTOR_STATE_BRAKE,
} ws63_motor_state_t;

/**
 * @brief 初始化电机驱动。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_motor_init(void);

/**
 * @brief 电机正转（IA=0，IB=PWM）。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_motor_forward(uint8_t duty_percent);

/**
 * @brief 电机反转（IA=PWM，IB=0）。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_motor_reverse(uint8_t duty_percent);

/**
 * @brief 电机停止（滑行，IA=0，IB=0）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_motor_coast_stop(void);

/**
 * @brief 电机刹车（急停，IA=1，IB=1）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_motor_brake_stop(void);

/**
 * @brief 调整当前运行方向的占空比。
 *
 * 若当前处于停止/刹车状态，仅更新默认占空比缓存，不立即驱动电机。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_motor_set_duty(uint8_t duty_percent);

/**
 * @brief 获取当前缓存占空比。
 *
 * @return uint8_t 占空比（0~100）。
 */
uint8_t ws63_motor_get_duty(void);

/**
 * @brief 获取当前电机状态。
 *
 * @return ws63_motor_state_t 当前状态。
 */
ws63_motor_state_t ws63_motor_get_state(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif