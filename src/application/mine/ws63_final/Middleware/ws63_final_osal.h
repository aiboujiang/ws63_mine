/**
 * @file ws63_final_osal.h
 * @brief WK2114 最终版中间件层 OSAL 抽象接口。
 */

#ifndef MINE_WS63_FINAL_OSAL_H
#define MINE_WS63_FINAL_OSAL_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 任务入口类型。
 */
typedef void *(*mine_ws63_final_task_entry_t)(const char *arg);

/**
 * @brief 创建并启动一个线程任务。
 *
 * @param name       任务名。
 * @param entry      任务入口。
 * @param arg        任务参数。
 * @param stack_size 栈大小。
 * @param priority   任务优先级。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t mine_ws63_final_os_start_task(const char *name,
    mine_ws63_final_task_entry_t entry, uintptr_t arg, uint16_t stack_size, uint8_t priority);

/**
 * @brief 毫秒延时。
 *
 * @param ms 毫秒。
 */
void mine_ws63_final_os_sleep_ms(uint32_t ms);

/**
 * @brief 获取系统毫秒计时。
 *
 * @return uint32_t 当前毫秒值。
 */
uint32_t mine_ws63_final_os_tick_ms(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
