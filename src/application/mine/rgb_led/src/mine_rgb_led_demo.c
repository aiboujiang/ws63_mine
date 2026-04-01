/**
 * @file mine_rgb_led_demo.c
 * @brief Mine单颗RGB灯珠示例任务。
 */

#include "mine_rgb_led.h"

#include <stddef.h>

#include "app_init.h"
#include "soc_osal.h"

#define MINE_RGB_LED_TASK_STACK        2048
#define MINE_RGB_LED_TASK_PRIO         26
#define MINE_RGB_LED_BOOT_DELAY_MS     500
#define MINE_RGB_LED_INIT_RETRY_MS     1000
#define MINE_RGB_LED_STEP_MS           500

/**
 * @brief RGB示例任务：循环展示基础颜色。
 */
static void *mine_rgb_led_task(const char *arg)
{
    (void)arg;

    /* 启动日志用于确认任务是否真正被调度执行。 */
    osal_printk("[mine_rgb_led] task start\r\n");
    osal_msleep(MINE_RGB_LED_BOOT_DELAY_MS);

    /* 初始化失败时持续重试，避免一次瞬态失败后任务直接退出。 */
    while (mine_rgb_led_init() != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] init failed, retry in %u ms\r\n",
                    (unsigned int)MINE_RGB_LED_INIT_RETRY_MS);
        osal_msleep(MINE_RGB_LED_INIT_RETRY_MS);
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

    /* 理论上不会到达这里，仅用于满足编译器返回路径检查。 */
    return NULL;
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
        osal_printk("[mine_rgb_led] task created, prio=%u\r\n", (unsigned int)MINE_RGB_LED_TASK_PRIO);
        osal_kfree(task_handle);
    } else {
        /* 线程创建失败时输出明确日志，便于定位无响应问题。 */
        osal_printk("[mine_rgb_led] task create failed\r\n");
    }
    osal_kthread_unlock();
}

app_run(mine_rgb_led_demo_entry);
