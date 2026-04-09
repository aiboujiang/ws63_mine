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
