/**
 * @file ws63_final_task_device_ctrl.c
 * @brief Task 层设备控制子模块（电机/编码器/蜂鸣器）。
 */

#include "ws63_final_task_internal.h"

#include "osal_debug.h"

#include "ws63_final_config.h"
#include "ws63_motor.h"
#include "ws63_encoder.h"
#include "ws63_buzzer.h"

/* 设备能力就绪标记：任务层只通过能力状态对外提供控制接口。 */
static uint8_t g_ws63_motor_encoder_ready = 0U;
static uint8_t g_ws63_buzzer_ready = 0U;

/**
 * @brief 初始化电机与编码器能力。
 */
void ws63_motor_encoder_init(void)
{
    errcode_t ret;

    g_ws63_motor_encoder_ready = 0U;

    ret = ws63_motor_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] motor init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    ret = ws63_encoder_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] encoder init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_motor_encoder_ready = 1U;
    osal_printk("[wk2114 final task] motor+encoder init ok (IA=GPIO2 IB=GPIO3 ENC=GPIO11/12)\r\n");
}

/**
 * @brief 查询电机/编码器能力是否可用。
 */
uint8_t ws63_task_motor_encoder_is_ready(void)
{
    return g_ws63_motor_encoder_ready;
}

/**
 * @brief 初始化蜂鸣器能力。
 */
void ws63_task_buzzer_init(void)
{
#if (WS63_BEEP_ENABLE == 1U)
    errcode_t ret;

    g_ws63_buzzer_ready = 0U;
    ret = ws63_buzzer_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] buzzer init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_buzzer_ready = 1U;
    osal_printk("[wk2114 final task] buzzer init ok (GPIO9/PWM1)\r\n");
#else
    g_ws63_buzzer_ready = 0U;
#endif
}

/**
 * @brief 控制电机正转（IA=0，IB=PWM）。
 */
errcode_t ws63_task_motor_forward(uint8_t duty_percent)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_forward(duty_percent);
}

/**
 * @brief 控制电机反转（IA=PWM，IB=0）。
 */
errcode_t ws63_task_motor_reverse(uint8_t duty_percent)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_reverse(duty_percent);
}

/**
 * @brief 电机停止（滑行，IA=0，IB=0）。
 */
errcode_t ws63_task_motor_coast_stop(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_coast_stop();
}

/**
 * @brief 电机刹车（急停，IA=1，IB=1）。
 */
errcode_t ws63_task_motor_brake_stop(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_brake_stop();
}

/**
 * @brief 动态调整当前运行方向占空比。
 */
errcode_t ws63_task_motor_set_duty(uint8_t duty_percent)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_set_duty(duty_percent);
}

/**
 * @brief 获取编码器最新 RPM。
 */
int32_t ws63_task_get_motor_rpm(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return 0;
    }

    return ws63_encoder_get_rpm();
}

/**
 * @brief 获取编码器累计计数值。
 */
int32_t ws63_task_get_encoder_total_count(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return 0;
    }

    return ws63_encoder_get_total_count();
}

/**
 * @brief 打开蜂鸣器并设置频率。
 */
errcode_t ws63_task_buzzer_on(uint16_t freq_hz)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)freq_hz;
    return ERRCODE_FAIL;
#else
    if (g_ws63_buzzer_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_buzzer_start(freq_hz);
#endif
}

/**
 * @brief 设置蜂鸣器音量。
 */
errcode_t ws63_task_buzzer_set_volume(uint8_t volume_percent)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)volume_percent;
    return ERRCODE_FAIL;
#else
    if (g_ws63_buzzer_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_buzzer_set_volume(volume_percent);
#endif
}

/**
 * @brief 关闭蜂鸣器。
 */
errcode_t ws63_task_buzzer_off(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    if (g_ws63_buzzer_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_buzzer_stop();
#endif
}

/**
 * @brief 查询蜂鸣器是否正在发声。
 */
uint8_t ws63_task_buzzer_is_on(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    if (g_ws63_buzzer_ready == 0U) {
        return 0U;
    }

    return ws63_buzzer_is_on();
#endif
}

/**
 * @brief 获取蜂鸣器当前频率。
 */
uint16_t ws63_task_buzzer_get_freq_hz(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    if (g_ws63_buzzer_ready == 0U) {
        return 0U;
    }

    return ws63_buzzer_get_freq_hz();
#endif
}

/**
 * @brief 获取蜂鸣器当前音量。
 */
uint8_t ws63_task_buzzer_get_volume(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    if (g_ws63_buzzer_ready == 0U) {
        return 0U;
    }

    return ws63_buzzer_get_volume();
#endif
}
