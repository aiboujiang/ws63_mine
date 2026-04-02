#include <stdbool.h>
#include <stdint.h>

#include "app_init.h"
#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "tcxo.h"

#define RGB_LED_TASK_PRIO                24
#define RGB_LED_TASK_STACK_SIZE          0x1000

/* WS2812 数据脚，默认使用 GPIO0，可按硬件连线修改。 */
#define WS2812_DATA_PIN                  GPIO_00

/* WS2812 的复位锁存时间要求 > 50us，这里留出裕量。 */
#define WS2812_RESET_LATCH_US            80U

/* 颜色切换演示间隔。 */
#define WS2812_DEMO_INTERVAL_MS          500U

/*
 * WS2812 位编码时序依赖 CPU 主频，此处使用 NOP 粗调脉宽。
 * 若实测颜色异常或闪烁，请根据示波器波形微调以下参数。
 */
#define WS2812_T0H_NOP                   4U
#define WS2812_T0L_NOP                   10U
#define WS2812_T1H_NOP                   9U
#define WS2812_T1L_NOP                   5U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_color_t;

static const ws2812_color_t g_ws2812_demo_colors[] = {
    {255U, 0U, 0U},
    {0U, 255U, 0U},
    {0U, 0U, 255U}
};

/**
 * @brief NOP 延时，用于 WS2812 子微秒级时序粗调。
 *
 * @param cycles NOP 循环次数。
 */
static inline void ws2812_delay_nop(uint32_t cycles)
{
    while (cycles-- > 0U) {
        __asm__ volatile("nop");
    }
}

/**
 * @brief 发送单个 WS2812 bit。
 *
 * @param bit_val true 发送 1 码；false 发送 0 码。
 */
static inline void ws2812_send_bit(bool bit_val)
{
    (void)uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_HIGH);
    ws2812_delay_nop(bit_val ? WS2812_T1H_NOP : WS2812_T0H_NOP);

    (void)uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_LOW);
    ws2812_delay_nop(bit_val ? WS2812_T1L_NOP : WS2812_T0L_NOP);
}

/**
 * @brief 发送 1 字节数据（MSB first）。
 *
 * @param data 待发送字节。
 */
static void ws2812_send_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        ws2812_send_bit((data & 0x80U) != 0U);
        data <<= 1U;
    }
}

/**
 * @brief 发送复位锁存脉冲。
 */
static void ws2812_reset_latch(void)
{
    (void)uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_LOW);
    (void)uapi_tcxo_delay_us(WS2812_RESET_LATCH_US);
}

/**
 * @brief 设置单颗 WS2812 颜色（GRB 顺序）。
 *
 * @param color 目标颜色。
 */
static void ws2812_set_color(const ws2812_color_t *color)
{
    uint32_t irq_state;

    if (color == NULL) {
        return;
    }

    /*
     * WS2812 对位间隔敏感，发送 24bit 期间禁止中断抢占。
     * 发送完成后立即恢复中断，避免影响系统实时性。
     */
    irq_state = osal_irq_lock();
    ws2812_send_byte(color->g);
    ws2812_send_byte(color->r);
    ws2812_send_byte(color->b);
    osal_irq_restore(irq_state);

    ws2812_reset_latch();
}

/**
 * @brief 初始化 WS2812 所需外设。
 *
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t ws2812_init(void)
{
    errcode_t ret;

    (void)uapi_tcxo_init();
    uapi_gpio_init();

    ret = uapi_pin_set_mode(WS2812_DATA_PIN, HAL_PIO_FUNC_GPIO);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(WS2812_DATA_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ws2812_reset_latch();
    return ERRCODE_SUCC;
}

/**
 * @brief RGB 演示任务：循环显示红绿蓝。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *rgb_led_task(const char *arg)
{
    errcode_t ret;
    uint32_t i;

    unused(arg);

    ret = ws2812_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb] ws2812 init failed, ret=0x%x\\r\\n", (unsigned int)ret);
        return NULL;
    }

    osal_printk("[mine_rgb] ws2812 demo start on GPIO_00\\r\\n");

    while (1) {
        for (i = 0U; i < (uint32_t)(sizeof(g_ws2812_demo_colors) / sizeof(g_ws2812_demo_colors[0])); i++) {
            ws2812_set_color(&g_ws2812_demo_colors[i]);
            (void)uapi_tcxo_delay_ms(WS2812_DEMO_INTERVAL_MS);
        }
    }
}

/**
 * @brief 创建 RGB LED 演示任务。
 */
static void rgb_led_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)rgb_led_task, 0, "RgbLedTask", RGB_LED_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, RGB_LED_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the rgb_led_entry. */
app_run(rgb_led_entry);