/**
 * @file ws63_final_task_debug.h
 * @brief ws63_final 调试命令子模块接口。
 */

#ifndef WS63_TASK_DEBUG_H
#define WS63_TASK_DEBUG_H

#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化调试命令子模块。
 */
void ws63_task_debug_init(void);

/**
 * @brief 周期处理调试命令子模块。
 *
 * @param now_ms 当前系统毫秒计时。
 */
void ws63_task_debug_process(uint32_t now_ms);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
