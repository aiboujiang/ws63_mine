/**
 * @file ws63_final_bsp_beep.c
 * @brief WK2114 最终版 BSP 蜂鸣器子模块实现。
 */

#include "ws63_final_bsp.h"

#include <stdbool.h>

#include "gpio.h"
#include "pinctrl.h"
#include "pwm.h"

#include "ws63_final_config.h"

#if (WS63_BEEP_ENABLE == 1U)
static bool g_ws63_beep_pwm_inited = false;

/**
 * @brief 将蜂鸣器引脚切回 GPIO 并拉低，避免停音时残余噪声。
 */
static errcode_t ws63_bsp_beep_force_silent(void)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(WS63_BEEP_GPIO_PIN, (pin_mode_t)WS63_BEEP_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(WS63_BEEP_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_val(WS63_BEEP_GPIO_PIN, GPIO_LEVEL_LOW);
}

/**
 * @brief 限制蜂鸣器频率到配置边界，避免异常参数导致 PWM 配置失败。
 */
static uint16_t ws63_bsp_beep_clamp_freq(uint16_t freq_hz)
{
    uint16_t freq;

    freq = freq_hz;
    if (freq < WS63_BEEP_MIN_FREQ_HZ) {
        freq = WS63_BEEP_MIN_FREQ_HZ;
    }
    if (freq > WS63_BEEP_MAX_FREQ_HZ) {
        freq = WS63_BEEP_MAX_FREQ_HZ;
    }

    return freq;
}

/**
 * @brief 限制蜂鸣器音量（占空比）到配置边界。
 */
static uint8_t ws63_bsp_beep_clamp_volume(uint8_t volume_percent)
{
    uint8_t volume;

    volume = volume_percent;
    if (volume < WS63_BEEP_MIN_VOLUME_PERCENT) {
        volume = WS63_BEEP_MIN_VOLUME_PERCENT;
    }
    if (volume > WS63_BEEP_MAX_VOLUME_PERCENT) {
        volume = WS63_BEEP_MAX_VOLUME_PERCENT;
    }

    return volume;
}

/**
 * @brief 由目标频率和音量构建 PWM 配置。
 */
static errcode_t ws63_bsp_beep_build_pwm_cfg(uint16_t freq_hz,
    uint8_t volume_percent,
    pwm_config_t *cfg)
{
    uint32_t pwm_clk_hz;
    uint32_t min_support_freq_hz;
    uint32_t adapted_freq_hz;
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint32_t low_ticks;
    uint32_t duty;

    if ((cfg == NULL) || (freq_hz == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    pwm_clk_hz = uapi_pwm_get_frequency(WS63_BEEP_PWM_CHANNEL);
    if (pwm_clk_hz == 0U) {
        return ERRCODE_FAIL;
    }

    /*
     * PWM v151 计数器位宽为 16bit，超低频会导致周期计数越界。
     * 这里按 2 倍升频自适应，保证 period_ticks 不超过硬件上限。
     */
    min_support_freq_hz = (pwm_clk_hz + WS63_BEEP_PWM_PERIOD_TICKS_MAX - 1U) /
        WS63_BEEP_PWM_PERIOD_TICKS_MAX;
    adapted_freq_hz = (uint32_t)ws63_bsp_beep_clamp_freq(freq_hz);
    while ((adapted_freq_hz < min_support_freq_hz) && (adapted_freq_hz <= (UINT16_MAX / 2U))) {
        adapted_freq_hz <<= 1U;
    }
    if (adapted_freq_hz < min_support_freq_hz) {
        adapted_freq_hz = min_support_freq_hz;
    }

    period_ticks = pwm_clk_hz / adapted_freq_hz;
    if (period_ticks < 2U) {
        period_ticks = 2U;
    }
    if (period_ticks > WS63_BEEP_PWM_PERIOD_TICKS_MAX) {
        period_ticks = WS63_BEEP_PWM_PERIOD_TICKS_MAX;
    }

    duty = (uint32_t)ws63_bsp_beep_clamp_volume(volume_percent);
    high_ticks = (period_ticks * duty) / 100U;
    low_ticks = period_ticks - high_ticks;
    if (high_ticks == 0U) {
        high_ticks = 1U;
    }
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
#endif

/**
 * @brief 初始化蜂鸣器底层资源（GPIO/PWM）。
 */
errcode_t ws63_bsp_beep_init(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    uapi_pin_init();
    uapi_gpio_init();

    ret = ws63_bsp_beep_force_silent();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    if (!g_ws63_beep_pwm_inited) {
        ret = uapi_pwm_init();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        g_ws63_beep_pwm_inited = true;
    }

#if defined(CONFIG_PWM_USING_V151)
    {
        uint8_t channel_id;

        channel_id = WS63_BEEP_PWM_CHANNEL;
        ret = uapi_pwm_set_group(WS63_BEEP_PWM_GROUP, &channel_id, 1U);
        if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_PWM_INVALID_PARAMETER)) {
            return ret;
        }
    }
#endif

    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 使能蜂鸣器连续发声。
 */
errcode_t ws63_bsp_beep_start(uint16_t freq_hz, uint8_t volume_percent)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)freq_hz;
    (void)volume_percent;
    return ERRCODE_FAIL;
#else
    errcode_t ret;
    pwm_config_t cfg;

    if (!g_ws63_beep_pwm_inited) {
        ret = ws63_bsp_beep_init();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    ret = ws63_bsp_beep_build_pwm_cfg(freq_hz, volume_percent, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_BEEP_GPIO_PIN, (pin_mode_t)WS63_BEEP_PWM_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    (void)uapi_pwm_close(WS63_BEEP_PWM_CHANNEL);
    ret = uapi_pwm_open(WS63_BEEP_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start(WS63_BEEP_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        (void)uapi_pwm_close(WS63_BEEP_PWM_CHANNEL);
        (void)ws63_bsp_beep_force_silent();
        return ret;
    }

    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 停止蜂鸣器并将引脚拉低静音。
 */
errcode_t ws63_bsp_beep_stop(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    (void)uapi_pwm_close(WS63_BEEP_PWM_CHANNEL);
    return ws63_bsp_beep_force_silent();
#endif
}
