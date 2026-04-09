/**
 * @file ws63_final_bsp.c
 * @brief WK2114 最终版 BSP 通用控制子模块实现。
 */

#include "ws63_final_bsp.h"

#include "gpio.h"
#include "osal_task.h"
#include "pinctrl.h"
#include "systick.h"

#include "ws63_final_config.h"

/**
 * @brief IRQ 空实现。
 *
 * 当前阶段仅保留中断链路，实际业务在 Driver/App 层走轮询，
 * 便于后续按模块接入统一事件分发。
 */
static void ws63_irq_stub(pin_t pin, uintptr_t param)
{
    (void)pin;
    (void)param;
}

/**
 * @brief 复位引脚初始化。
 */
void ws63_bsp_reset_init(void)
{
    uapi_pin_set_mode(WS63_RST_PIN, WS63_RST_PIN_MODE);
    uapi_gpio_set_dir(WS63_RST_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(WS63_RST_PIN, GPIO_LEVEL_HIGH);
}

/**
 * @brief 设置复位引脚电平。
 */
void ws63_bsp_reset_set(uint8_t level_high)
{
    uapi_gpio_set_val(WS63_RST_PIN,
        level_high ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/**
 * @brief 初始化 IRQ 引脚。
 */
void ws63_bsp_irq_init(void)
{
    uapi_pin_set_mode(WS63_IRQ_PIN, WS63_IRQ_PIN_MODE);
    uapi_gpio_set_dir(WS63_IRQ_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_register_isr_func(WS63_IRQ_PIN,
        GPIO_INTERRUPT_FALLING_EDGE, ws63_irq_stub);
    uapi_gpio_enable_interrupt(WS63_IRQ_PIN);
}

/**
 * @brief 毫秒延时适配。
 */
void ws63_bsp_sleep_ms(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 获取系统毫秒计时。
 */
uint32_t ws63_bsp_get_tick_ms(void)
{
    return (uint32_t)uapi_systick_get_ms();
}
