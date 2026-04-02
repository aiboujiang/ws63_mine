/**
 * @file example_rgb.c
 * @brief GPIO4 软时序驱动 WS2812 单灯示例（全新版本）。
 *
 * 时序依据：application/mine/lib/RGB_LED.pdf
 * - 0码高电平 Tin0h 典型值：0.295us
 * - 0码低电平 T0L  典型值：0.595us
 * - 1码高电平 Tin1h 典型值：0.595us
 * - 1码低电平 T1L  典型值：0.295us
 * - RESET 低电平：按备注取 >=100us，这里使用 120us。
 */

#include "common_def.h"
#include "app_init.h"
#include "pinctrl.h"
#include "gpio.h"
#include "systick.h"
#include "watchdog.h"
#include "soc_osal.h"

#define WS2812_TASK_PRIO                 24
#define WS2812_TASK_STACK_SIZE           0x1000

#define WS2812_GPIO_PIN                  GPIO_04
#define WS2812_GPIO_MODE                 PIN_MODE_2

/*
 * 按硬件文档要求：上电启动阶段不要对关键引脚施加上拉。
 * 这里先进入“无上下拉 + 输入态”并等待系统启动完成，再执行 LED 输出。
 */
#define WS2812_STARTUP_DELAY_MS          5000U

#define WS2812_T0H_NS                    295U
#define WS2812_T0L_NS                    595U
#define WS2812_T1H_NS                    595U
#define WS2812_T1L_NS                    295U
#define WS2812_RESET_US                  120U

#define WS2812_COLOR_HOLD_MS             1000U
#define WS2812_BRIGHTNESS_PERCENT        30U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    const char *name;
} ws2812_color_t;

static uint32_t g_systick_count_per_us = 40U;
static uint32_t g_t0h_count = 12U;
static uint32_t g_t0l_count = 24U;
static uint32_t g_t1h_count = 24U;
static uint32_t g_t1l_count = 12U;

static const ws2812_color_t g_color_table[] = {
    {255U,   0U,   0U, "RED"},
    {  0U, 255U,   0U, "GREEN"},
    {  0U,   0U, 255U, "BLUE"},
    {255U, 255U,   0U, "YELLOW"},
    {255U,   0U, 255U, "PURPLE"},
    {  0U, 255U, 255U, "CYAN"},
    {255U, 255U, 255U, "WHITE"}
};

/**
 * @brief 按百分比缩放颜色分量，用于统一亮度控制。
 *
 * @param value 原始颜色分量。
 * @return uint8_t 缩放后的颜色分量。
 */
static uint8_t ws2812_apply_brightness(uint8_t value)
{
    uint32_t scaled = ((uint32_t)value * WS2812_BRIGHTNESS_PERCENT + 50U) / 100U;
    if (scaled > 255U) {
        scaled = 255U;
    }
    return (uint8_t)scaled;
}

/**
 * @brief 启动阶段的引脚保护配置：关闭上下拉并保持输入态。
 */
static void ws2812_prepare_pin_safe_state(void)
{
    uapi_gpio_init();
    (void)uapi_pin_set_mode(WS2812_GPIO_PIN, WS2812_GPIO_MODE);
    (void)uapi_pin_set_pull(WS2812_GPIO_PIN, PIN_PULL_TYPE_DISABLE);
    (void)uapi_gpio_set_dir(WS2812_GPIO_PIN, GPIO_DIRECTION_INPUT);
}

/**
 * @brief 分片休眠并喂狗，避免长延时触发看门狗。
 *
 * @param delay_ms 延时时间（毫秒）。
 */
static void ws2812_task_delay_ms(uint32_t delay_ms)
{
    const uint32_t slice_ms = 20U;

    while (delay_ms > 0U) {
        uint32_t curr_ms = (delay_ms > slice_ms) ? slice_ms : delay_ms;
        (void)uapi_watchdog_kick();
        (void)osal_msleep(curr_ms);
        delay_ms -= curr_ms;
    }
}

/**
 * @brief 将纳秒换算为 Systick 计数。
 *
 * @param time_ns 时间（纳秒）。
 * @return uint32_t 计数值，最小为 1。
 */
static uint32_t ws2812_ns_to_count(uint32_t time_ns)
{
    uint64_t count = ((uint64_t)g_systick_count_per_us * (uint64_t)time_ns + 999U) / 1000U;
    return (count == 0U) ? 1U : (uint32_t)count;
}

/**
 * @brief 运行期测量 Systick 每微秒计数并更新 WS2812 时序计数。
 */
static void ws2812_calibrate_timing(void)
{
    const uint64_t sample_window_us = 500U;
    uint64_t start_us;
    uint64_t now_us;
    uint64_t delta_us;
    uint64_t start_count;
    uint64_t now_count;
    uint64_t delta_count;

    start_us = uapi_systick_get_us();
    start_count = uapi_systick_get_count();
    do {
        now_us = uapi_systick_get_us();
    } while ((now_us - start_us) < sample_window_us);
    now_count = uapi_systick_get_count();

    delta_us = now_us - start_us;
    delta_count = now_count - start_count;
    if ((delta_us > 0U) && (delta_count > 0U)) {
        uint32_t measured = (uint32_t)((delta_count + (delta_us / 2U)) / delta_us);
        if (measured > 0U) {
            g_systick_count_per_us = measured;
        }
    }

    g_t0h_count = ws2812_ns_to_count(WS2812_T0H_NS);
    g_t0l_count = ws2812_ns_to_count(WS2812_T0L_NS);
    g_t1h_count = ws2812_ns_to_count(WS2812_T1H_NS);
    g_t1l_count = ws2812_ns_to_count(WS2812_T1L_NS);

    osal_printk("[mine_rgb_led] PDF timing: T0H/T0L/T1H/T1L(ns)=%u/%u/%u/%u\r\n",
                (unsigned int)WS2812_T0H_NS,
                (unsigned int)WS2812_T0L_NS,
                (unsigned int)WS2812_T1H_NS,
                (unsigned int)WS2812_T1L_NS);
    osal_printk("[mine_rgb_led] timing counts: T0H/T0L/T1H/T1L=%u/%u/%u/%u\r\n",
                (unsigned int)g_t0h_count,
                (unsigned int)g_t0l_count,
                (unsigned int)g_t1h_count,
                (unsigned int)g_t1l_count);
}

/**
 * @brief 输出 1bit WS2812 时序。
 *
 * @param bit_val 比特值，0 或 1。
 */
static void ws2812_write_bit(uint8_t bit_val)
{
    if (bit_val == 0U) {
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_HIGH);
        (void)uapi_systick_delay_count(g_t0h_count);
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
        (void)uapi_systick_delay_count(g_t0l_count);
    } else {
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_HIGH);
        (void)uapi_systick_delay_count(g_t1h_count);
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
        (void)uapi_systick_delay_count(g_t1l_count);
    }
}

/**
 * @brief 发送一帧颜色（GRB 顺序，高位先发）。
 *
 * @param r 红色分量。
 * @param g 绿色分量。
 * @param b 蓝色分量。
 */
static void ws2812_send_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    uint32_t mask;
    unsigned int irq_status;

    /* 24bit 发送期间关闭中断，减少任务切换对单线时序的干扰。 */
    irq_status = osal_irq_lock();
    for (mask = 0x800000U; mask > 0U; mask >>= 1U) {
        ws2812_write_bit((grb & mask) ? 1U : 0U);
    }
    osal_irq_restore(irq_status);

    /* 保持低电平完成锁存。 */
    (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
    (void)uapi_systick_delay_us(WS2812_RESET_US);
}

/**
 * @brief 初始化 GPIO4 与软时序基础时钟。
 *
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t ws2812_init(void)
{
    errcode_t ret;

    uapi_gpio_init();

    ret = uapi_pin_set_mode(WS2812_GPIO_PIN, WS2812_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pin mode failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    ret = uapi_gpio_set_dir(WS2812_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] gpio dir failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
    uapi_systick_init();
    ws2812_calibrate_timing();

    return ERRCODE_SUCC;
}

/**
 * @brief 任务主循环：红、绿、蓝、黄、紫、青、白，每色 1 秒。
 */
static void *ws2812_task(const char *arg)
{
    uint32_t color_index = 0U;

    UNUSED(arg);
    osal_printk("[mine_rgb_led] ws2812 gpio4 restart from scratch\r\n");

    ws2812_prepare_pin_safe_state();
    osal_printk("[mine_rgb_led] startup delay %u ms, led start after boot\r\n",
                (unsigned int)WS2812_STARTUP_DELAY_MS);
    ws2812_task_delay_ms(WS2812_STARTUP_DELAY_MS);

    if (ws2812_init() != ERRCODE_SUCC) {
        return NULL;
    }

    while (1) {
        const ws2812_color_t *color = &g_color_table[color_index];
        uint8_t r_out = ws2812_apply_brightness(color->r);
        uint8_t g_out = ws2812_apply_brightness(color->g);
        uint8_t b_out = ws2812_apply_brightness(color->b);

        (void)uapi_watchdog_kick();
        ws2812_send_color(r_out, g_out, b_out);
        osal_printk("[mine_rgb_led] color=%s R=%u G=%u B=%u (30%%)\r\n",
                    color->name,
                    (unsigned int)r_out,
                    (unsigned int)g_out,
                    (unsigned int)b_out);

        ws2812_task_delay_ms(WS2812_COLOR_HOLD_MS);
        color_index = (color_index + 1U) % (uint32_t)(sizeof(g_color_table) / sizeof(g_color_table[0]));
    }

    return NULL;
}

/**
 * @brief 任务入口。
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
    }
    osal_kthread_unlock();
}

/* Run the ws2812_entry. */
app_run(ws2812_entry);
