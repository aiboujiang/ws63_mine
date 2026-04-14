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

/* 统一队列等待参数：与 OSAL 默认定义保持一致。 */
#define WS63_OS_NO_WAIT      0U
#define WS63_OS_WAIT_FOREVER 0xFFFFFFFFU

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
 * @brief 创建消息队列。
 *
 * @param name     队列名（用于调试）。
 * @param queue_len 队列深度（消息个数）。
 * @param msg_size  单条消息大小（字节）。
 * @param queue_id  输出队列句柄。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_os_msg_queue_create(const char *name,
    uint16_t queue_len, uint16_t msg_size, unsigned long *queue_id);

/**
 * @brief 创建互斥锁。
 *
 * @param mutex_id 输出互斥锁句柄。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_os_mutex_create(unsigned long *mutex_id);

/**
 * @brief 尝试获取互斥锁。
 *
 * @param mutex_id 互斥锁句柄。
 * @param timeout  超时参数（Tick）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_os_mutex_lock(unsigned long mutex_id, uint32_t timeout);

/**
 * @brief 释放互斥锁。
 *
 * @param mutex_id 互斥锁句柄。
 */
void ws63_os_mutex_unlock(unsigned long mutex_id);

/**
 * @brief 向消息队列发送一条拷贝消息。
 *
 * @param queue_id 队列句柄。
 * @param msg      消息缓冲区。
 * @param msg_size 消息长度（字节）。
 * @param timeout  超时参数（Tick）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_os_msg_queue_send(unsigned long queue_id,
    const void *msg, uint16_t msg_size, uint32_t timeout);

/**
 * @brief 从消息队列接收一条拷贝消息。
 *
 * @param queue_id  队列句柄。
 * @param msg       输出缓冲区。
 * @param msg_size  输入为缓冲区长度，输出为实际消息长度。
 * @param timeout   超时参数（Tick）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_os_msg_queue_recv(unsigned long queue_id,
    void *msg, uint32_t *msg_size, uint32_t timeout);

/**
 * @brief 删除消息队列。
 *
 * @param queue_id 队列句柄。
 */
void ws63_os_msg_queue_delete(unsigned long queue_id);

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
