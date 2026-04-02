/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pinctrl.h"
#include "gpio.h"
#include "pwm.h"
#include "tcxo.h"
#include "soc_osal.h"
#include "app_init.h"
#include "watchdog.h"

#define BUZZER_TASK_PRIO 24
#define BUZZER_TASK_STACK_SIZE 0x1000

/* GPIO9 在复用模式 1 下对应 PWM1。 */
#define BUZZER_GPIO_PIN GPIO_09
#define BUZZER_GPIO_MODE HAL_PIO_FUNC_GPIO
#define BUZZER_PWM_PIN_MODE PIN_MODE_1
#define BUZZER_PWM_CHANNEL 1U

/* 音符之间留短间隔，避免无源蜂鸣器连续驱动时听感粘连。 */
#define BUZZER_NOTE_GAP_MS 30U
#define BUZZER_LOOP_GAP_MS 800U

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} buzzer_note_t;

/*
 * 以 C 大调上行为例：Do-Re-Mi-Fa-So-La-Si-Do。
 * 若需自定义音调，仅修改下表频率和时长即可。
 */
static const buzzer_note_t g_buzzer_scale[] = {
    {262U, 220U},
    {294U, 220U},
    {330U, 220U},
    {349U, 220U},
    {392U, 220U},
    {440U, 220U},
    {494U, 220U},
    {523U, 420U},
    {0U,   160U}
};

/**
 * @brief 让蜂鸣器引脚回到 GPIO 低电平，确保静音。
 *
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t buzzer_force_silent(void)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(BUZZER_GPIO_PIN, BUZZER_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(BUZZER_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_val(BUZZER_GPIO_PIN, GPIO_LEVEL_LOW);
}

/**
 * @brief 根据目标频率生成 PWM 配置。
 *
 * @param freq_hz 目标频率（Hz）。
 * @param cfg 输出配置。
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t buzzer_build_pwm_cfg(uint16_t freq_hz, pwm_config_t *cfg)
{
    uint32_t pwm_clk_hz;
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint32_t low_ticks;

    if ((cfg == NULL) || (freq_hz == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    pwm_clk_hz = uapi_pwm_get_frequency(BUZZER_PWM_CHANNEL);
    if (pwm_clk_hz == 0U) {
        return ERRCODE_FAIL;
    }

    period_ticks = pwm_clk_hz / (uint32_t)freq_hz;
    if (period_ticks < 2U) {
        period_ticks = 2U;
    }

    high_ticks = period_ticks / 2U;
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

/**
 * @brief 初始化蜂鸣器依赖外设。
 *
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t buzzer_init(void)
{
    errcode_t ret;

    (void)uapi_tcxo_init();
    uapi_gpio_init();

    ret = buzzer_force_silent();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    uapi_pwm_deinit();
    return uapi_pwm_init();
}

/**
 * @brief 以指定频率播放一个音符。
 *
 * @param note 音符描述，freq_hz=0 表示休止符。
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t buzzer_play_note(const buzzer_note_t *note)
{
    errcode_t ret;
    pwm_config_t cfg;

    if (note == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    if (note->freq_hz == 0U) {
        uapi_tcxo_delay_ms(note->duration_ms);
        return ERRCODE_SUCC;
    }

    ret = buzzer_build_pwm_cfg(note->freq_hz, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 每次切频前强制停止上一音符，防止通道保持旧参数。 */
    (void)uapi_pwm_close(BUZZER_PWM_CHANNEL);

    ret = uapi_pin_set_mode(BUZZER_GPIO_PIN, BUZZER_PWM_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_open(BUZZER_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start(BUZZER_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        (void)uapi_pwm_close(BUZZER_PWM_CHANNEL);
        return ret;
    }

    uapi_tcxo_delay_ms(note->duration_ms);

    /* 音符结束后拉低引脚，避免听到残余电平噪声。 */
    (void)uapi_pwm_close(BUZZER_PWM_CHANNEL);
    (void)buzzer_force_silent();
    return ERRCODE_SUCC;
}

/**
 * @brief 蜂鸣器任务：循环播放多音调音阶。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *buzzer_task(const char *arg)
{
    errcode_t ret;
    uint32_t i;

    (void)arg;

    ret = buzzer_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_buzzer] init failed, ret=0x%x\r\n", (unsigned int)ret);
        return NULL;
    }

    osal_printk("[mine_buzzer] start on GPIO9/PWM1 (mux1), playing tones\r\n");

    while (1) {
        for (i = 0U; i < (uint32_t)(sizeof(g_buzzer_scale) / sizeof(g_buzzer_scale[0])); i++) {
            ret = buzzer_play_note(&g_buzzer_scale[i]);
            if (ret != ERRCODE_SUCC) {
                osal_printk("[mine_buzzer] play note failed, idx=%u, ret=0x%x\r\n",
                    (unsigned int)i,
                    (unsigned int)ret);
            }

            (void)uapi_watchdog_kick();
            uapi_tcxo_delay_ms(BUZZER_NOTE_GAP_MS);
        }

        /* 每轮音阶之间停顿，便于区分循环边界。 */
        uapi_tcxo_delay_ms(BUZZER_LOOP_GAP_MS);
    }
}

/**
 * @brief 创建蜂鸣器任务。
 */
static void buzzer_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)buzzer_task,
                                      0,
                                      "BuzzerTask",
                                      BUZZER_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, BUZZER_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the buzzer_entry. */
app_run(buzzer_entry);