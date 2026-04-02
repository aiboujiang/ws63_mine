/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pinctrl.h"
#include "tcxo.h"
#include "hal_gpio.h"
#include "soc_osal.h"
#include "app_init.h"
#include "gpio.h"
#include "watchdog.h"

#define WS2812_TASK_PRIO 24
#define WS2812_TASK_STACK_SIZE 0x1000

/* 使用 GPIO9 直接软件模拟 WS2812 单线时序。 */
#define WS2812_GPIO_PIN 9
#define WS2812_GPIO_MODE 0
#define WS2812_PIN_PULL_NONE 0

/* WS2812 手册时序（mine/lib/RGB_LED.pdf）。 */
#define WS2812_T0H_NS 295U
#define WS2812_T0L_NS 595U
#define WS2812_T1H_NS 595U
#define WS2812_T1L_NS 295U
#define WS2812_RESET_US 120U

/* 固定红色：GRB 顺序 = G=0x00, R=0xFF, B=0x00。 */
#define WS2812_RED_G 0x00U
#define WS2812_RED_R 0xFFU
#define WS2812_RED_B 0x00U

/* 周期刷新，保证灯色稳定。 */
#define WS2812_REFRESH_MS 100U

static uint32_t g_tcxo_ticks_per_1000us = 0U;
static uint32_t g_t0h_ticks = 0U;
static uint32_t g_t0l_ticks = 0U;
static uint32_t g_t1h_ticks = 0U;
static uint32_t g_t1l_ticks = 0U;

/*
 * 某些构建路径下 hal_tcxo 接口来自 ROM 函数表，
 * 这里声明最小必要结构，优先走高速 get 回调读取计数。
 */
typedef uint64_t (*ws2812_hal_tcxo_get_t)(void);
typedef struct {
    void *init;
    void *deinit;
    ws2812_hal_tcxo_get_t get;
} ws2812_hal_tcxo_funcs_t;
extern ws2812_hal_tcxo_funcs_t *hal_tcxo_get_funcs(void);

/**
 * @brief 获取高速 TCXO 计数。
 *
 * 优先使用 HAL 函数表直读计数，避免 UAPI 的锁开销影响位时序。
 *
 * @return uint64_t 当前 TCXO 计数。
 */
static uint64_t ws2812_fast_tcxo_get(void)
{
    ws2812_hal_tcxo_funcs_t *funcs = hal_tcxo_get_funcs();
    if ((funcs != NULL) && (funcs->get != NULL)) {
        return funcs->get();
    }
    return uapi_tcxo_get_count();
}

/**
 * @brief 将纳秒换算为 TCXO tick。
 *
 * @param time_ns 纳秒。
 * @return uint32_t 至少为 1 tick。
 */
static uint32_t ws2812_ns_to_ticks(uint32_t time_ns)
{
    uint64_t ticks = ((uint64_t)time_ns * (uint64_t)g_tcxo_ticks_per_1000us + 999999ULL) / 1000000ULL;
    if (ticks == 0ULL) {
        ticks = 1ULL;
    }
    return (uint32_t)ticks;
}

/**
 * @brief 以 TCXO tick 进行忙等延时。
 *
 * @param ticks 需要延时的 tick 数。
 */
static void ws2812_delay_ticks(uint32_t ticks)
{
    uint64_t target = ws2812_fast_tcxo_get() + (uint64_t)ticks;
    while (ws2812_fast_tcxo_get() < target) {
    }
}

/**
 * @brief 发送一个 WS2812 bit。
 *
 * @param one true 发送“1”，false 发送“0”。
 */
static void ws2812_write_bit(bool one)
{
    (void)hal_gpio_output(WS2812_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (one) {
        ws2812_delay_ticks(g_t1h_ticks);
    } else {
        ws2812_delay_ticks(g_t0h_ticks);
    }

    (void)hal_gpio_output(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
    if (one) {
        ws2812_delay_ticks(g_t1l_ticks);
    } else {
        ws2812_delay_ticks(g_t0l_ticks);
    }
}

/**
 * @brief 按高位先发发送一个字节。
 *
 * @param value 待发送字节。
 */
static void ws2812_write_byte(uint8_t value)
{
    uint8_t i;
    for (i = 0U; i < 8U; i++) {
        ws2812_write_bit((value & 0x80U) != 0U);
        value <<= 1U;
    }
}

/**
 * @brief 发送固定红色（GRB）。
 */
static void ws2812_send_red(void)
{
    uint32_t irq_state = osal_irq_lock();

    ws2812_write_byte(WS2812_RED_G);
    ws2812_write_byte(WS2812_RED_R);
    ws2812_write_byte(WS2812_RED_B);

    osal_irq_restore(irq_state);

    /* 一帧结束后的复位低电平。 */
    (void)hal_gpio_output(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
    uapi_tcxo_delay_us(WS2812_RESET_US);
}

/**
 * @brief 初始化 GPIO 和时序标定。
 *
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t ws2812_gpio_init(void)
{
    errcode_t ret;
    uint64_t start_count;
    uint64_t end_count;

    (void)uapi_tcxo_init();
    uapi_gpio_init();

    ret = uapi_pin_set_mode(WS2812_GPIO_PIN, WS2812_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_pull(WS2812_GPIO_PIN, WS2812_PIN_PULL_NONE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(WS2812_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);

    /* 标定 1000us 的 TCXO tick 数，用于换算亚微秒时序。 */
    start_count = ws2812_fast_tcxo_get();
    uapi_tcxo_delay_us(1000U);
    end_count = ws2812_fast_tcxo_get();
    if (end_count > start_count) {
        g_tcxo_ticks_per_1000us = (uint32_t)(end_count - start_count);
    }
    if (g_tcxo_ticks_per_1000us == 0U) {
        g_tcxo_ticks_per_1000us = 40000U;
    }

    g_t0h_ticks = ws2812_ns_to_ticks(WS2812_T0H_NS);
    g_t0l_ticks = ws2812_ns_to_ticks(WS2812_T0L_NS);
    g_t1h_ticks = ws2812_ns_to_ticks(WS2812_T1H_NS);
    g_t1l_ticks = ws2812_ns_to_ticks(WS2812_T1L_NS);

    osal_printk("[mine_rgb_led] gpio bitbang red start, T0H/T0L/T1H/T1L ticks=%u/%u/%u/%u\r\n",
        (unsigned int)g_t0h_ticks,
        (unsigned int)g_t0l_ticks,
        (unsigned int)g_t1h_ticks,
        (unsigned int)g_t1l_ticks);
    return ERRCODE_SUCC;
}

/**
 * @brief GPIO 软件模拟 WS2812 任务，仅输出红色。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *ws2812_task(const char *arg)
{
    errcode_t ret;

    (void)arg;

    ret = ws2812_gpio_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] gpio init failed, ret=0x%x\r\n", (unsigned int)ret);
        return NULL;
    }

    while (1) {
        ws2812_send_red();
        (void)uapi_watchdog_kick();
        uapi_tcxo_delay_ms(WS2812_REFRESH_MS);
    }
}

/**
 * @brief 创建 WS2812 任务。
 */
static void ws2812_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)ws2812_task,
                                      0,
                                      "Ws2812Task",
                                      WS2812_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WS2812_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the ws2812_entry. */
app_run(ws2812_entry);