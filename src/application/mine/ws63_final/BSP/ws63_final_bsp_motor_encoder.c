/**
 * @file ws63_final_bsp_motor_encoder.c
 * @brief WK2114 最终版 BSP 电机/编码器子模块实现。
 */

#include "ws63_final_bsp.h"

#include <stdbool.h>

#include "gpio.h"
#include "pinctrl.h"
#include "pwm.h"

#include "ws63_final_config.h"

static bool g_ws63_motor_pwm_inited = false;

/**
 * @brief 配置 GPIO 输出并拉到目标电平。
 */
static errcode_t ws63_bsp_set_gpio_output_level(pin_t pin, uint8_t mode, uint8_t level_high)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(pin, (pin_mode_t)mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(pin, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_val(pin, level_high ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/**
 * @brief 配置编码器输入引脚（模式/上下拉/方向）。
 */
static errcode_t ws63_bsp_set_gpio_input(pin_t pin, uint8_t mode, uint8_t pull_mode)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(pin, (pin_mode_t)mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_pull(pin, (pin_pull_t)pull_mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_dir(pin, GPIO_DIRECTION_INPUT);
}

/**
 * @brief 根据占空比构建 PWM 配置。
 */
static errcode_t ws63_bsp_motor_build_pwm_cfg(uint8_t duty_percent, pwm_config_t *cfg)
{
    uint32_t duty;
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint32_t low_ticks;

    if (cfg == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    period_ticks = WS63_MOTOR_PWM_PERIOD_TICKS;
    if (period_ticks < 2U) {
        return ERRCODE_INVALID_PARAM;
    }

    duty = duty_percent;
    if (duty > 100U) {
        duty = 100U;
    }

    high_ticks = (period_ticks * duty) / 100U;
    if (high_ticks == 0U) {
        high_ticks = 1U;
    }
    if (high_ticks >= period_ticks) {
        high_ticks = period_ticks - 1U;
    }

    low_ticks = period_ticks - high_ticks;
    if (low_ticks == 0U) {
        low_ticks = 1U;
    }

    cfg->low_time = low_ticks;
    cfg->high_time = high_ticks;
    cfg->offset_time = 0U;
    cfg->cycles = 0U;
    cfg->repeat = true;
    return ERRCODE_SUCC;
}

/**
 * @brief 初始化电机底层资源（GPIO/PWM）。
 */
errcode_t ws63_bsp_motor_init(void)
{
    errcode_t ret;

    uapi_pin_init();
    uapi_gpio_init();

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IA_PIN, WS63_MOTOR_IA_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IB_PIN, WS63_MOTOR_IB_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    if (!g_ws63_motor_pwm_inited) {
        ret = uapi_pwm_init();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        g_ws63_motor_pwm_inited = true;
    }

#if defined(CONFIG_PWM_USING_V151)
    {
        uint8_t channel_id;

        channel_id = WS63_MOTOR_IA_PWM_CHANNEL;
        ret = uapi_pwm_set_group(WS63_MOTOR_IA_PWM_GROUP, &channel_id, 1U);
        if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_PWM_INVALID_PARAMETER)) {
            return ret;
        }

        channel_id = WS63_MOTOR_IB_PWM_CHANNEL;
        ret = uapi_pwm_set_group(WS63_MOTOR_IB_PWM_GROUP, &channel_id, 1U);
        if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_PWM_INVALID_PARAMETER)) {
            return ret;
        }
    }
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief 关闭电机 PWM 输出并恢复 IA/IB 为 GPIO 模式。
 */
errcode_t ws63_bsp_motor_disable_pwm(void)
{
    errcode_t ret;

    if (g_ws63_motor_pwm_inited) {
        (void)uapi_pwm_close(WS63_MOTOR_IA_PWM_CHANNEL);
        (void)uapi_pwm_close(WS63_MOTOR_IB_PWM_CHANNEL);
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IA_PIN, (pin_mode_t)WS63_MOTOR_IA_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IB_PIN, (pin_mode_t)WS63_MOTOR_IB_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 设置电机 IA/IB GPIO 电平。
 */
errcode_t ws63_bsp_motor_set_level(uint8_t ia_high, uint8_t ib_high)
{
    errcode_t ret;

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IA_PIN, WS63_MOTOR_IA_GPIO_MODE, ia_high);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ws63_bsp_set_gpio_output_level(WS63_MOTOR_IB_PIN, WS63_MOTOR_IB_GPIO_MODE, ib_high);
}

/**
 * @brief 使能 IA 通道 PWM 输出。
 */
errcode_t ws63_bsp_motor_enable_pwm_ia(uint8_t duty_percent)
{
    errcode_t ret;
    pwm_config_t cfg;

    ret = ws63_bsp_motor_build_pwm_cfg(duty_percent, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_motor_disable_pwm();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IB_PIN, WS63_MOTOR_IB_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IA_PIN, (pin_mode_t)WS63_MOTOR_IA_PWM_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    (void)uapi_pwm_close(WS63_MOTOR_IA_PWM_CHANNEL);
    ret = uapi_pwm_open(WS63_MOTOR_IA_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start(WS63_MOTOR_IA_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        (void)uapi_pwm_close(WS63_MOTOR_IA_PWM_CHANNEL);
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 使能 IB 通道 PWM 输出。
 */
errcode_t ws63_bsp_motor_enable_pwm_ib(uint8_t duty_percent)
{
    errcode_t ret;
    pwm_config_t cfg;

    ret = ws63_bsp_motor_build_pwm_cfg(duty_percent, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_motor_disable_pwm();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IA_PIN, WS63_MOTOR_IA_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IB_PIN, (pin_mode_t)WS63_MOTOR_IB_PWM_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    (void)uapi_pwm_close(WS63_MOTOR_IB_PWM_CHANNEL);
    ret = uapi_pwm_open(WS63_MOTOR_IB_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start(WS63_MOTOR_IB_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        (void)uapi_pwm_close(WS63_MOTOR_IB_PWM_CHANNEL);
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 初始化编码器 IO。
 */
errcode_t ws63_bsp_encoder_init(void)
{
    errcode_t ret;

    uapi_pin_init();
    uapi_gpio_init();

    ret = ws63_bsp_set_gpio_input(WS63_ENCODER_A_PIN, WS63_ENCODER_PIN_MODE, WS63_ENCODER_PULL_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ws63_bsp_set_gpio_input(WS63_ENCODER_B_PIN, WS63_ENCODER_PIN_MODE, WS63_ENCODER_PULL_MODE);
}

/**
 * @brief 注册编码器 A 相上升沿中断回调。
 */
errcode_t ws63_bsp_encoder_register_a_isr(ws63_bsp_gpio_callback_t callback)
{
    errcode_t ret;

    if (callback == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    (void)uapi_gpio_unregister_isr_func(WS63_ENCODER_A_PIN);
    ret = uapi_gpio_register_isr_func(WS63_ENCODER_A_PIN, GPIO_INTERRUPT_RISING_EDGE, callback);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_enable_interrupt(WS63_ENCODER_A_PIN);
}

/**
 * @brief 读取编码器 B 相电平。
 */
uint8_t ws63_bsp_encoder_get_b_level(void)
{
    return (uapi_gpio_get_val(WS63_ENCODER_B_PIN) == GPIO_LEVEL_HIGH) ? 1U : 0U;
}
