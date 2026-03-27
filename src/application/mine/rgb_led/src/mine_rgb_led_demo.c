/**
 * @file mine_rgb_led_demo.c
 * @brief Mine单颗RGB灯珠示例任务。
 */

#include "mine_rgb_led.h"

#include <stddef.h>

#include "app_init.h"
#include "osal_debug.h"
#include "osal_task.h"

#define MINE_RGB_LED_TASK_STACK        2048
#define MINE_RGB_LED_TASK_PRIO         26
#define MINE_RGB_LED_STEP_MS           500

/**
 * @brief RGB示例任务：循环展示基础颜色。
 */
static void *mine_rgb_led_task(const char *arg)
{
    (void)arg;

    osal_msleep(500);
    if (mine_rgb_led_init() != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] init failed\r\n");
        return NULL;
    }

    osal_printk("[mine_rgb_led] init ok, DI=GPIO4\r\n");

    while (1) {
        /* 红 */
        mine_rgb_led_set_rgb(0xFFU, 0x00U, 0x00U);
        osal_msleep(MINE_RGB_LED_STEP_MS);

        /* 绿 */
        mine_rgb_led_set_rgb(0x00U, 0xFFU, 0x00U);
        osal_msleep(MINE_RGB_LED_STEP_MS);

        /* 蓝 */
        mine_rgb_led_set_rgb(0x00U, 0x00U, 0xFFU);
        osal_msleep(MINE_RGB_LED_STEP_MS);

        /* 白 */
        mine_rgb_led_set_rgb(0x40U, 0x40U, 0x40U);
        osal_msleep(MINE_RGB_LED_STEP_MS);

        /* 灭灯 */
        mine_rgb_led_off();
        osal_msleep(MINE_RGB_LED_STEP_MS);
    }
}

/**
 * @brief 应用启动入口，创建RGB示例线程。
 */
static void mine_rgb_led_demo_entry(void)
{
    osal_task *task_handle;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mine_rgb_led_task,
                                      0,
                                      "mine_rgb_led",
                                      MINE_RGB_LED_TASK_STACK);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MINE_RGB_LED_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(mine_rgb_led_demo_entry);
