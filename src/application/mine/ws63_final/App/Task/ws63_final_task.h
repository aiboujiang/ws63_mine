/**
 * @file ws63_final_task.h
 * @brief WK2114 最终版应用任务层接口。
 */

#ifndef WS63_TASK_H
#define WS63_TASK_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 子串口接收回调。
 *
 * @param sub_port 子串口号（1~4）。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 */
typedef void (*ws63_rx_callback_t)(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 注册子串口接收回调。
 *
 * @param sub_port 子串口号（1~4）。
 * @param callback 回调函数，可为 NULL（表示注销）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_register_rx_callback(uint8_t sub_port,
    ws63_rx_callback_t callback);

/**
 * @brief 通过 WK2114 子串口发送数据。
 *
 * @param sub_port 子串口号（1~4）。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_send(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 控制电机正转（IA=0，IB=PWM）。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_forward(uint8_t duty_percent);

/**
 * @brief 控制电机反转（IA=PWM，IB=0）。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_reverse(uint8_t duty_percent);

/**
 * @brief 电机停止（滑行，IA=0，IB=0）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_coast_stop(void);

/**
 * @brief 电机刹车（急停，IA=1，IB=1）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_brake_stop(void);

/**
 * @brief 动态调整当前运行方向占空比。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_set_duty(uint8_t duty_percent);

/**
 * @brief 获取编码器最新 RPM。
 *
 * @return int32_t 有符号 RPM。
 */
int32_t ws63_task_get_motor_rpm(void);

/**
 * @brief 获取编码器累计计数值。
 *
 * @return int32_t 有符号累计脉冲计数。
 */
int32_t ws63_task_get_encoder_total_count(void);

/**
 * @brief WK2114 最终版业务任务入口。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
void *ws63_task_entry(const char *arg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
