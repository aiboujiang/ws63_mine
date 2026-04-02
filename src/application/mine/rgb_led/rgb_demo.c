#include <stdbool.h>
#include <stdint.h>

#include "app_init.h"
#include "gpio.h"
#include "pinctrl.h"
#include "platform_core.h"
#include "soc_osal.h"
#include "tcxo.h"
#include "watchdog.h"

#define RGB_LED_TASK_PRIO                24
#define RGB_LED_TASK_STACK_SIZE          0x1000

/* WS2812 数据脚使用 GPIO9。 */
#define WS2812_DATA_PIN                  GPIO_09

/*
 * 按当前硬件连线要求，GPIO9 需切到复用信号0，
 * 再作为普通 GPIO 输出使用。
 */
#define WS2812_DATA_PIN_MODE             PIN_MODE_0

/* 调试阶段默认打开日志，稳定后可改为 0 降低串口输出。 */
#define WS2812_DEBUG_LOG_ENABLE          1

/* 直写 GPIO 数据寄存器，降低单 bit 输出开销。 */
#define WS2812_USE_DIRECT_REG_IO         1

#define WS2812_GPIO9_CHANNEL_BASE        GPIO_CHANNEL_1_BASE_ADDR
#define WS2812_GPIO_GROUP_STRIDE         0x40U
#define WS2812_GPIO_DATA_SET_OFFSET      0x30U
#define WS2812_GPIO_DATA_CLR_OFFSET      0x34U
#define WS2812_GPIO_GROUP_INDEX          0U
#define WS2812_GPIO_GROUP_PIN            1U
#define WS2812_GPIO_PIN_MASK             (1U << WS2812_GPIO_GROUP_PIN)

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

static volatile uint32_t *const g_ws2812_gpio_data_set_reg =
    (volatile uint32_t *)(uintptr_t)(WS2812_GPIO9_CHANNEL_BASE +
    WS2812_GPIO_GROUP_INDEX * WS2812_GPIO_GROUP_STRIDE + WS2812_GPIO_DATA_SET_OFFSET);

static volatile uint32_t *const g_ws2812_gpio_data_clr_reg =
    (volatile uint32_t *)(uintptr_t)(WS2812_GPIO9_CHANNEL_BASE +
    WS2812_GPIO_GROUP_INDEX * WS2812_GPIO_GROUP_STRIDE + WS2812_GPIO_DATA_CLR_OFFSET);

/**
 * @brief 打印 RGB 配置参数，便于确认固件与接线一致。
 */
static void ws2812_log_config(void)
{
#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][cfg] pin=%u mode=%u reset_us=%u interval_ms=%u\r\n",
        (unsigned int)WS2812_DATA_PIN,
        (unsigned int)WS2812_DATA_PIN_MODE,
        (unsigned int)WS2812_RESET_LATCH_US,
        (unsigned int)WS2812_DEMO_INTERVAL_MS);
    osal_printk("[mine_rgb][cfg] nop T0H=%u T0L=%u T1H=%u T1L=%u\r\n",
        (unsigned int)WS2812_T0H_NOP,
        (unsigned int)WS2812_T0L_NOP,
        (unsigned int)WS2812_T1H_NOP,
        (unsigned int)WS2812_T1L_NOP);
#if (WS2812_USE_DIRECT_REG_IO == 1)
    osal_printk("[mine_rgb][cfg] direct-reg-io=on set=0x%08x clr=0x%08x mask=0x%08x\r\n",
        (unsigned int)(uintptr_t)g_ws2812_gpio_data_set_reg,
        (unsigned int)(uintptr_t)g_ws2812_gpio_data_clr_reg,
        (unsigned int)WS2812_GPIO_PIN_MASK);
#else
    osal_printk("[mine_rgb][cfg] direct-reg-io=off (uapi_gpio_set_val)\r\n");
#endif
#endif
}

/**
 * @brief 将 WS2812 数据脚拉高。
 */
static inline void ws2812_gpio_high(void)
{
#if (WS2812_USE_DIRECT_REG_IO == 1)
    *g_ws2812_gpio_data_set_reg = WS2812_GPIO_PIN_MASK;
#else
    (void)uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_HIGH);
#endif
}

/**
 * @brief 将 WS2812 数据脚拉低。
 */
static inline void ws2812_gpio_low(void)
{
#if (WS2812_USE_DIRECT_REG_IO == 1)
    *g_ws2812_gpio_data_clr_reg = WS2812_GPIO_PIN_MASK;
#else
    (void)uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_LOW);
#endif
}

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
    ws2812_gpio_high();
    ws2812_delay_nop(bit_val ? WS2812_T1H_NOP : WS2812_T0H_NOP);

    ws2812_gpio_low();
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
    ws2812_gpio_low();
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

    ws2812_log_config();

    (void)uapi_tcxo_init();
    uapi_gpio_init();

    ret = uapi_pin_set_mode(WS2812_DATA_PIN, WS2812_DATA_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][init] pin_set_mode failed, ret=0x%x\r\n", (unsigned int)ret);
#endif
        return ret;
    }

#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][init] pin_set_mode ok\r\n");
#endif

    ret = uapi_gpio_set_dir(WS2812_DATA_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][init] gpio_set_dir failed, ret=0x%x\r\n", (unsigned int)ret);
#endif
        return ret;
    }

#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][init] gpio_set_dir ok\r\n");
#endif

    ret = uapi_gpio_set_val(WS2812_DATA_PIN, GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][init] gpio_set_low failed, ret=0x%x\r\n", (unsigned int)ret);
#endif
        return ret;
    }

    ws2812_reset_latch();

#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][init] ws2812_reset_latch done\r\n");
#endif

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
    errcode_t wdt_ret;
    uint32_t i;
    uint32_t frame_cnt = 0U;
    uint64_t tx_begin_us;
    uint64_t tx_end_us;
    uint64_t wave_begin_us;
    uint64_t wave_end_us;

    unused(arg);

    ret = ws2812_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb] ws2812 init failed, ret=0x%x\\r\\n", (unsigned int)ret);
        return NULL;
    }

    osal_printk("[mine_rgb] ws2812 demo start on GPIO_09 (PIN_MODE_0)\\r\\n");

    while (1) {
        for (i = 0U; i < (uint32_t)(sizeof(g_ws2812_demo_colors) / sizeof(g_ws2812_demo_colors[0])); i++) {
            tx_begin_us = uapi_tcxo_get_us();
#if (WS2812_DEBUG_LOG_ENABLE == 1)
            osal_printk("[mine_rgb][loop] frame=%u idx=%u rgb=(%u,%u,%u) begin_us=%llu\\r\\n",
                (unsigned int)frame_cnt,
                (unsigned int)i,
                (unsigned int)g_ws2812_demo_colors[i].r,
                (unsigned int)g_ws2812_demo_colors[i].g,
                (unsigned int)g_ws2812_demo_colors[i].b,
                (unsigned long long)tx_begin_us);
#endif

            wave_begin_us = uapi_tcxo_get_us();
            ws2812_set_color(&g_ws2812_demo_colors[i]);
            tx_end_us = uapi_tcxo_get_us();
            wave_end_us = tx_end_us;

#if (WS2812_DEBUG_LOG_ENABLE == 1)
            osal_printk("[mine_rgb][loop] frame=%u idx=%u total_cost_us=%llu wave_cost_us=%llu gpio_out=%u\\r\\n",
                (unsigned int)frame_cnt,
                (unsigned int)i,
                (unsigned long long)(tx_end_us - tx_begin_us),
                (unsigned long long)(wave_end_us - wave_begin_us),
                (unsigned int)uapi_gpio_get_output_val(WS2812_DATA_PIN));
#endif

            /*
             * 避免使用忙等毫秒延时占满 CPU。
             * 采用 OS 睡眠让出调度，并在循环中喂狗，降低 NMI/看门狗复位风险。
             */
            wdt_ret = uapi_watchdog_kick();
#if (WS2812_DEBUG_LOG_ENABLE == 1)
            if (wdt_ret != ERRCODE_SUCC) {
                osal_printk("[mine_rgb][loop] watchdog_kick failed, ret=0x%x\\r\\n", (unsigned int)wdt_ret);
            }
#endif
            osal_msleep(WS2812_DEMO_INTERVAL_MS);
        }

        frame_cnt++;
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][hb] frame cycle done, frame=%u uptime_ms=%llu\\r\\n",
            (unsigned int)frame_cnt,
            (unsigned long long)uapi_tcxo_get_ms());
#endif
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