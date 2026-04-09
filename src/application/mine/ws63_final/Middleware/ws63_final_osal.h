/**
 * @file ws63_final_osal.h
 * @brief WK2114 最终版中间件层 OSAL 抽象接口。
 */

#ifndef WS63_OSAL_H
#define WS63_OSAL_H

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
typedef void *(*ws63_task_entry_t)(const char *arg);

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
errcode_t ws63_os_start_task(const char *name,
    ws63_task_entry_t entry, uintptr_t arg, uint16_t stack_size, uint8_t priority);

/**
 * @brief 毫秒延时。
 *
 * @param ms 毫秒。
 */
void ws63_os_sleep_ms(uint32_t ms);

/**
 * @brief 获取系统毫秒计时。
 *
 * @return uint32_t 当前毫秒值。
 */
uint32_t ws63_os_tick_ms(void);

/**
 * @brief 进入 OSAL 临界区。
 *
 * @return unsigned int 中断状态快照，需原样传回 ws63_os_irq_unlock。
 */
unsigned int ws63_os_irq_lock(void);

/**
 * @brief 退出 OSAL 临界区。
 *
 * @param irq_status ws63_os_irq_lock 返回的中断状态快照。
 */
void ws63_os_irq_unlock(unsigned int irq_status);

/**
 * @brief 喂狗适配接口。
 *
 * 说明：
 * 1) 应用层通过本接口喂狗，避免直接依赖 watchdog HAL 头文件；
 * 2) 便于后续替换为不同平台/看门狗实现。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_os_feed_watchdog(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
