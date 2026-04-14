/**
 * @file ws63_final_osal.c
 * @brief WK2114 最终版中间件层 OSAL 抽象实现。
 */

#include "ws63_final_osal.h"

#include <stddef.h>

#include "osal_addr.h"
#include "soc_osal.h"
#include "osal_task.h"
#include "systick.h"
#include "watchdog.h"

/**
 * @brief 创建并启动线程任务。
 */
errcode_t ws63_os_start_task(const char *name,
    ws63_task_entry_t entry, uintptr_t arg, uint16_t stack_size, uint8_t priority)
{
    osal_task *task_handle;

    if ((name == NULL) || (entry == NULL) || (stack_size == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    osal_kthread_lock();
    /*
     * arg 在对外接口中以 uintptr_t 透传，创建线程前统一转回 void *，
     * 兼容 OSAL 接口签名并避免告警升级为错误。
     */
    task_handle = osal_kthread_create((osal_kthread_handler)entry,
        (void *)(uintptr_t)arg,
        name,
        stack_size);
    if (task_handle == NULL) {
        osal_kthread_unlock();
        return ERRCODE_FAIL;
    }

    osal_kthread_set_priority(task_handle, (int32_t)priority);
    osal_kfree(task_handle);
    osal_kthread_unlock();
    return ERRCODE_SUCC;
}

/**
 * @brief 创建消息队列。
 */
errcode_t ws63_os_msg_queue_create(const char *name,
    uint16_t queue_len, uint16_t msg_size, unsigned long *queue_id)
{
    int ret;

    if ((name == NULL) || (queue_id == NULL) || (queue_len == 0U) || (msg_size == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = osal_msg_queue_create(name,
        (unsigned short)queue_len,
        queue_id,
        0U,
        (unsigned short)msg_size);
    if (ret != OSAL_SUCCESS) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 向消息队列发送一条拷贝消息。
 */
errcode_t ws63_os_msg_queue_send(unsigned long queue_id,
    const void *msg, uint16_t msg_size, uint32_t timeout)
{
    int ret;

    if ((queue_id == 0UL) || (msg == NULL) || (msg_size == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = osal_msg_queue_write_copy(queue_id,
        (void *)(uintptr_t)msg,
        (unsigned int)msg_size,
        (unsigned int)timeout);
    if (ret != OSAL_SUCCESS) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 从消息队列接收一条拷贝消息。
 */
errcode_t ws63_os_msg_queue_recv(unsigned long queue_id,
    void *msg, uint32_t *msg_size, uint32_t timeout)
{
    int ret;
    unsigned int read_size;

    if ((queue_id == 0UL) || (msg == NULL) || (msg_size == NULL) || (*msg_size == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    read_size = (unsigned int)(*msg_size);
    ret = osal_msg_queue_read_copy(queue_id, msg, &read_size, (unsigned int)timeout);
    if (ret != OSAL_SUCCESS) {
        return ERRCODE_FAIL;
    }

    *msg_size = (uint32_t)read_size;
    return ERRCODE_SUCC;
}

/**
 * @brief 创建互斥锁。
 */
errcode_t ws63_os_mutex_create(unsigned long *mutex_id)
{
    osal_mutex *mutex;

    if (mutex_id == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    mutex = osal_kmalloc(sizeof(osal_mutex), OSAL_GFP_KERNEL);
    if (mutex == NULL) {
        return ERRCODE_FAIL;
    }

    if (osal_mutex_init(mutex) != OSAL_SUCCESS) {
        osal_kfree(mutex);
        return ERRCODE_FAIL;
    }

    *mutex_id = (unsigned long)(uintptr_t)mutex;
    return ERRCODE_SUCC;
}

/**
 * @brief 尝试获取互斥锁。
 */
errcode_t ws63_os_mutex_lock(unsigned long mutex_id, uint32_t timeout)
{
    osal_mutex *mutex = (osal_mutex *)(uintptr_t)mutex_id;

    if (mutex == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    if (osal_mutex_lock_timeout(mutex, (unsigned int)timeout) != OSAL_SUCCESS) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 释放互斥锁。
 */
void ws63_os_mutex_unlock(unsigned long mutex_id)
{
    osal_mutex *mutex = (osal_mutex *)(uintptr_t)mutex_id;

    if (mutex != NULL) {
        osal_mutex_unlock(mutex);
    }
}

/**
 * @brief 删除消息队列。
 */
void ws63_os_msg_queue_delete(unsigned long queue_id)
{
    if (queue_id == 0UL) {
        return;
    }

    osal_msg_queue_delete(queue_id);
}

/**
 * @brief 毫秒延时。
 */
void ws63_os_sleep_ms(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 获取系统毫秒计时。
 */
uint32_t ws63_os_tick_ms(void)
{
    return (uint32_t)uapi_systick_get_ms();
}

/**
 * @brief 进入 OSAL 临界区。
 */
unsigned int ws63_os_irq_lock(void)
{
    return osal_irq_lock();
}

/**
 * @brief 退出 OSAL 临界区。
 */
void ws63_os_irq_unlock(unsigned int irq_status)
{
    osal_irq_restore(irq_status);
}

/**
 * @brief 喂狗适配接口。
 */
errcode_t ws63_os_feed_watchdog(void)
{
    return uapi_watchdog_kick();
}
