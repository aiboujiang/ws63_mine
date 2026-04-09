/**
 * @file ws63_final_task_internal.h
 * @brief Task 层内部模块共享接口（仅供 App/Task 内部 C 文件使用）。
 */

#ifndef WS63_FINAL_TASK_INTERNAL_H
#define WS63_FINAL_TASK_INTERNAL_H

#include <stdint.h>

#include "ws63_final_task.h"

#define WS63_SUBPORT_MAX 4U

/**
 * @brief 初始化 RGB 演示链路。
 */
void ws63_rgb_demo_init(void);

/**
 * @brief 周期驱动 RGB 演示。
 *
 * @param now_ms 当前系统毫秒 Tick。
 */
void ws63_rgb_demo_process(uint32_t now_ms);

/**
 * @brief 初始化电机与编码器能力。
 */
void ws63_motor_encoder_init(void);

/**
 * @brief 查询电机/编码器能力是否可用。
 *
 * @return uint8_t 1=就绪，0=未就绪。
 */
uint8_t ws63_task_motor_encoder_is_ready(void);

/**
 * @brief 初始化蜂鸣器能力。
 */
void ws63_task_buzzer_init(void);

#endif