/**
 * @file ws63_motor.c
 * @brief WS63 电机控制驱动层实现。
 */

#include "ws63_motor.h"

#include "ws63_final_bsp.h"
#include "ws63_final_config.h"

/* 驱动层缓存当前状态，便于任务层按状态调整占空比。 */
static ws63_motor_state_t g_ws63_motor_state = WS63_MOTOR_STATE_COAST;
static uint8_t g_ws63_motor_duty_percent = WS63_MOTOR_DEFAULT_DUTY_PERCENT;

/**
 * @brief 限幅占空比到 0~100。
 */
static uint8_t ws63_motor_clamp_duty(uint8_t duty_percent)
{
    if (duty_percent > 100U) {
        return 100U;
    }
    return duty_percent;
}

/**
 * @brief 初始化电机驱动。
 */
errcode_t ws63_motor_init(void)
{
    errcode_t ret;

    ret = ws63_bsp_motor_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_motor_duty_percent = ws63_motor_clamp_duty(WS63_MOTOR_DEFAULT_DUTY_PERCENT);
    return ws63_motor_coast_stop();
}

/**
 * @brief 电机正转（IA=0，IB=PWM）。
 */
errcode_t ws63_motor_forward(uint8_t duty_percent)
{
    errcode_t ret;
    uint8_t duty;

    duty = ws63_motor_clamp_duty(duty_percent);
    if (duty == 0U) {
        return ws63_motor_coast_stop();
    }

    ret = ws63_bsp_motor_enable_pwm_ib(duty);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_motor_state = WS63_MOTOR_STATE_FORWARD;
    g_ws63_motor_duty_percent = duty;
    return ERRCODE_SUCC;
}

/**
 * @brief 电机反转（IA=PWM，IB=0）。
 */
errcode_t ws63_motor_reverse(uint8_t duty_percent)
{
    errcode_t ret;
    uint8_t duty;

    duty = ws63_motor_clamp_duty(duty_percent);
    if (duty == 0U) {
        return ws63_motor_coast_stop();
    }

    ret = ws63_bsp_motor_enable_pwm_ia(duty);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_motor_state = WS63_MOTOR_STATE_REVERSE;
    g_ws63_motor_duty_percent = duty;
    return ERRCODE_SUCC;
}

/**
 * @brief 电机停止（滑行，IA=0，IB=0）。
 */
errcode_t ws63_motor_coast_stop(void)
{
    errcode_t ret;

    /* 先关 PWM，再切回 GPIO 电平，避免模式切换时出现毛刺。 */
    ret = ws63_bsp_motor_disable_pwm();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_motor_set_level(0U, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_motor_state = WS63_MOTOR_STATE_COAST;
    g_ws63_motor_duty_percent = 0U;
    return ERRCODE_SUCC;
}

/**
 * @brief 电机刹车（急停，IA=1，IB=1）。
 */
errcode_t ws63_motor_brake_stop(void)
{
    errcode_t ret;

    /* 刹车前先停 PWM，确保双高电平是稳定直流电平。 */
    ret = ws63_bsp_motor_disable_pwm();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_motor_set_level(1U, 1U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_ws63_motor_state = WS63_MOTOR_STATE_BRAKE;
    g_ws63_motor_duty_percent = 0U;
    return ERRCODE_SUCC;
}

/**
 * @brief 调整当前运行方向的占空比。
 */
errcode_t ws63_motor_set_duty(uint8_t duty_percent)
{
    uint8_t duty;

    duty = ws63_motor_clamp_duty(duty_percent);
    g_ws63_motor_duty_percent = duty;

    if (g_ws63_motor_state == WS63_MOTOR_STATE_FORWARD) {
        return ws63_motor_forward(duty);
    }

    if (g_ws63_motor_state == WS63_MOTOR_STATE_REVERSE) {
        return ws63_motor_reverse(duty);
    }

    if (duty == 0U) {
        return ws63_motor_coast_stop();
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 获取当前缓存占空比。
 */
uint8_t ws63_motor_get_duty(void)
{
    return g_ws63_motor_duty_percent;
}

/**
 * @brief 获取当前电机状态。
 */
ws63_motor_state_t ws63_motor_get_state(void)
{
    return g_ws63_motor_state;
}
