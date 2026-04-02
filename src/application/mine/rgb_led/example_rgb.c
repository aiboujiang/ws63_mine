/**
 * @file example_rgb.c
 * @brief GPIO 软时序驱动 WS2812（GPIO4）。
 *
 * 说明：
 * - 使用 GPIO 拉高/拉低 + Systick 计数延时，模拟 800KHz 单总线时序。
 * - 0 码时序：高 0.4us + 低 0.85us。
 * - 1 码时序：高 0.8us + 低 0.45us。
 * - 颜色循环：红、绿、蓝、黄、紫、青、白，每色保持 1 秒。
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
 * 启动保护窗口：避开 RF 校准阶段（cali_offline_cali_entry）与 RGB 软时序并发。
 * 该值来自 application/mine/README.md 的稳定性建议默认值。
 */
#define WS2812_STARTUP_GUARD_MS          30000U

#define WS2812_T0H_NS                    400U
#define WS2812_T0L_NS                    850U
#define WS2812_T1H_NS                    800U
#define WS2812_T1L_NS                    450U
#define WS2812_RESET_US                  80U

#define WS2812_COLOR_HOLD_MS             1000U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    const char *name;
} ws2812_color_t;

static uint32_t g_systick_count_per_us = 40U;
static uint32_t g_t0h_count = 16U;
static uint32_t g_t0l_count = 34U;
static uint32_t g_t1h_count = 32U;
static uint32_t g_t1l_count = 18U;
static uint32_t g_gpio_edge_overhead_count = 6U;
static uint32_t g_t0h_delay_count = 10U;
static uint32_t g_t0l_delay_count = 28U;
static uint32_t g_t1h_delay_count = 26U;
static uint32_t g_t1l_delay_count = 12U;

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
 * @brief 任务态延时：分片休眠并喂狗，避免长延时触发看门狗。
 *
 * @param delay_ms 目标延时（毫秒）。
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
 * @brief 将纳秒时间换算为 Systick 计数。
 *
 * @param time_ns 时间（纳秒）。
 * @return uint32_t 对应的 Systick 计数，最小返回 1。
 */
static uint32_t ws2812_ns_to_count(uint32_t time_ns)
{
    uint64_t count = ((uint64_t)g_systick_count_per_us * (uint64_t)time_ns + 999U) / 1000U;
    return (count == 0U) ? 1U : (uint32_t)count;
}

/**
 * @brief 根据 GPIO 翻转调用开销回扣延时计数，避免脉宽整体被拉长。
 *
 * @param target_count 目标脉宽对应的计数。
 * @return uint32_t 实际用于 delay_count 的计数，最小返回 1。
 */
static uint32_t ws2812_compensate_delay_count(uint32_t target_count)
{
    if (target_count > g_gpio_edge_overhead_count) {
        return target_count - g_gpio_edge_overhead_count;
    }
    return 1U;
}

/**
 * @brief 测量 GPIO 翻转 API 的平均开销，供软时序补偿使用。
 */
static void ws2812_calibrate_gpio_overhead(void)
{
    const uint32_t sample_round = 16U;
    uint64_t total_call_count = 0U;
    unsigned int irq_status;
    uint32_t i;

    irq_status = osal_irq_lock();
    for (i = 0U; i < sample_round; i++) {
        uint64_t t0 = uapi_systick_get_count();
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_HIGH);
        uint64_t t1 = uapi_systick_get_count();
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
        uint64_t t2 = uapi_systick_get_count();

        total_call_count += (t1 - t0);
        total_call_count += (t2 - t1);
    }
    osal_irq_restore(irq_status);

    if (total_call_count > 0U) {
        uint32_t avg = (uint32_t)(total_call_count / (uint64_t)(sample_round * 2U));
        if (avg > 0U) {
            g_gpio_edge_overhead_count = avg;
        }
    }
}

/**
 * @brief 运行期测量 Systick 每微秒计数，避免硬编码时钟误差。
 */
static void ws2812_calibrate_systick_count(void)
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

    ws2812_calibrate_gpio_overhead();
    g_t0h_delay_count = ws2812_compensate_delay_count(g_t0h_count);
    g_t0l_delay_count = ws2812_compensate_delay_count(g_t0l_count);
    g_t1h_delay_count = ws2812_compensate_delay_count(g_t1h_count);
    g_t1l_delay_count = ws2812_compensate_delay_count(g_t1l_count);

    osal_printk("[mine_rgb_led] systick/us=%u, gpio_ovh=%u\r\n",
                (unsigned int)g_systick_count_per_us,
                (unsigned int)g_gpio_edge_overhead_count);
    osal_printk("[mine_rgb_led] target T0H/T0L/T1H/T1L=%u/%u/%u/%u\r\n",
                (unsigned int)g_t0h_count,
                (unsigned int)g_t0l_count,
                (unsigned int)g_t1h_count,
                (unsigned int)g_t1l_count);
    osal_printk("[mine_rgb_led] delay  T0H/T0L/T1H/T1L=%u/%u/%u/%u\r\n",
                (unsigned int)g_t0h_delay_count,
                (unsigned int)g_t0l_delay_count,
                (unsigned int)g_t1h_delay_count,
                (unsigned int)g_t1l_delay_count);
}

/**
 * @brief 输出一个 WS2812 比特。
 *
 * @param bit_val 比特值，0 或 1。
 */
static void ws2812_write_bit(uint8_t bit_val)
{
    if (bit_val == 0U) {
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_HIGH);
        (void)uapi_systick_delay_count(g_t0h_delay_count);
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
        (void)uapi_systick_delay_count(g_t0l_delay_count);
    } else {
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_HIGH);
        (void)uapi_systick_delay_count(g_t1h_delay_count);
        (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
        (void)uapi_systick_delay_count(g_t1l_delay_count);
    }
}

/**
 * @brief 发送 24bit 颜色数据（GRB 顺序）到 WS2812。
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

    /* 发送 24bit 期间临时关中断，减少调度抖动导致的时序拉伸。 */
    irq_status = osal_irq_lock();
    for (mask = 0x800000U; mask > 0U; mask >>= 1U) {
        ws2812_write_bit((grb & mask) ? 1U : 0U);
    }
    osal_irq_restore(irq_status);

    /* WS2812 锁存要求低电平保持至少 50us，这里留 80us 裕量。 */
    (void)uapi_gpio_set_val(WS2812_GPIO_PIN, GPIO_LEVEL_LOW);
    (void)uapi_systick_delay_us(WS2812_RESET_US);
}

/**
 * @brief GPIO 软时序 WS2812 初始化。
 *
 * @return errcode_t ERRCODE_SUCC 表示成功，其它值表示失败。
 */
static errcode_t ws2812_gpio_init(void)
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
    ws2812_calibrate_systick_count();
    return ERRCODE_SUCC;
}

/**
 * @brief WS2812 演示任务：按指定顺序循环输出 7 种颜色。
 */
static void *ws2812_task(const char *arg)
{
    uint32_t color_index = 0U;

    UNUSED(arg);
    osal_printk("[mine_rgb_led] gpio ws2812 demo start\r\n");

    if (ws2812_gpio_init() != ERRCODE_SUCC) {
        return NULL;
    }

    /* 先等待射频校准窗口结束，避免校准链路与 bit-bang 并发触发看门狗复位。 */
    osal_printk("[mine_rgb_led] wait %u ms for RF cali guard\r\n",
                (unsigned int)WS2812_STARTUP_GUARD_MS);
    ws2812_task_delay_ms(WS2812_STARTUP_GUARD_MS);

    while (1) {
        const ws2812_color_t *color = &g_color_table[color_index];

        (void)uapi_watchdog_kick();
        ws2812_send_color(color->r, color->g, color->b);
        osal_printk("[mine_rgb_led] color=%s R=%u G=%u B=%u\r\n",
                    color->name,
                    (unsigned int)color->r,
                    (unsigned int)color->g,
                    (unsigned int)color->b);

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
