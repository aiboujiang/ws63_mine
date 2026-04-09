/**
 * @file ws63_buzzer.c
 * @brief WS63 蜂鸣器驱动层实现。
 */

#include "ws63_buzzer.h"

#include "ws63_final_bsp.h"
#include "ws63_final_config.h"

#if (WS63_BEEP_ENABLE == 1U)
static uint8_t g_ws63_buzzer_ready = 0U;
static uint8_t g_ws63_buzzer_on = 0U;
static uint16_t g_ws63_buzzer_freq_hz = WS63_BEEP_DEFAULT_FREQ_HZ;
static uint8_t g_ws63_buzzer_volume_percent = WS63_BEEP_DEFAULT_VOLUME_PERCENT;

/**
 * @brief 限制频率到配置边界，避免驱动层向 BSP 传入非法值。
 */
static uint16_t ws63_buzzer_clamp_freq(uint16_t freq_hz)
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
 * @brief 限制音量到配置边界。
 */
static uint8_t ws63_buzzer_clamp_volume(uint8_t volume_percent)
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
#endif

/**
 * @brief 初始化蜂鸣器驱动。
 */
errcode_t ws63_buzzer_init(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    ret = ws63_bsp_beep_init();
    if (ret != ERRCODE_SUCC) {
        g_ws63_buzzer_ready = 0U;
        return ret;
    }

    g_ws63_buzzer_freq_hz = ws63_buzzer_clamp_freq(WS63_BEEP_DEFAULT_FREQ_HZ);
    g_ws63_buzzer_volume_percent = ws63_buzzer_clamp_volume(WS63_BEEP_DEFAULT_VOLUME_PERCENT);
    g_ws63_buzzer_on = 0U;
    g_ws63_buzzer_ready = 1U;
    return ws63_bsp_beep_stop();
#endif
}

/**
 * @brief 打开蜂鸣器并以指定频率连续发声。
 */
errcode_t ws63_buzzer_start(uint16_t freq_hz)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)freq_hz;
    return ERRCODE_FAIL;
#else
    errcode_t ret;
    uint16_t freq;

    if (g_ws63_buzzer_ready == 0U) {
        ret = ws63_buzzer_init();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    freq = ws63_buzzer_clamp_freq(freq_hz);
    if (g_ws63_buzzer_volume_percent == 0U) {
        (void)ws63_bsp_beep_stop();
        g_ws63_buzzer_on = 0U;
        g_ws63_buzzer_freq_hz = freq;
        return ERRCODE_SUCC;
    }

    ret = ws63_bsp_beep_start(freq, g_ws63_buzzer_volume_percent);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_buzzer_on = 1U;
    g_ws63_buzzer_freq_hz = freq;
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 设置蜂鸣器音量。
 */
errcode_t ws63_buzzer_set_volume(uint8_t volume_percent)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)volume_percent;
    return ERRCODE_FAIL;
#else
    errcode_t ret;
    uint8_t volume;

    if (g_ws63_buzzer_ready == 0U) {
        ret = ws63_buzzer_init();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    volume = ws63_buzzer_clamp_volume(volume_percent);
    g_ws63_buzzer_volume_percent = volume;

    if (volume == 0U) {
        if (g_ws63_buzzer_on == 0U) {
            return ERRCODE_SUCC;
        }

        ret = ws63_bsp_beep_stop();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }

        g_ws63_buzzer_on = 0U;
        return ERRCODE_SUCC;
    }

    if (g_ws63_buzzer_on == 0U) {
        return ERRCODE_SUCC;
    }

    return ws63_bsp_beep_start(g_ws63_buzzer_freq_hz, g_ws63_buzzer_volume_percent);
#endif
}

/**
 * @brief 关闭蜂鸣器。
 */
errcode_t ws63_buzzer_stop(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    if (g_ws63_buzzer_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ret = ws63_bsp_beep_stop();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_buzzer_on = 0U;
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 查询蜂鸣器当前是否处于发声状态。
 */
uint8_t ws63_buzzer_is_on(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    return g_ws63_buzzer_on;
#endif
}

/**
 * @brief 获取当前蜂鸣器工作频率（Hz）。
 */
uint16_t ws63_buzzer_get_freq_hz(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    return g_ws63_buzzer_freq_hz;
#endif
}

/**
 * @brief 获取当前蜂鸣器音量百分比。
 */
uint8_t ws63_buzzer_get_volume(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    return g_ws63_buzzer_volume_percent;
#endif
}
