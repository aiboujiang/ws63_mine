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

/* GPIO3 在复用模式 1 下对应 PWM3。 */
#define BUZZER_GPIO_PIN GPIO_03
#define BUZZER_PWM_PIN_MODE PIN_MODE_1
#define BUZZER_GPIO_MODE HAL_PIO_FUNC_GPIO
#define BUZZER_PWM_CHANNEL 3U
#define BUZZER_PWM_GROUP_ID 3U

#define BUZZER_NOTE_GAP_MS 30U
#define BUZZER_LOOP_GAP_MS 800U

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} buzzer_note_t;

/* 默认音阶可直接替换为业务旋律。 */
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
 * @brief 将蜂鸣器引脚拉为低电平，避免停音时噪声。
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
 * @brief 根据频率计算 PWM 参数（约 50% 占空比）。
 *
 * @param freq_hz 目标频率。
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
 * @brief 初始化蜂鸣器外设。
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
    ret = uapi_pwm_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

#if defined(CONFIG_PWM_USING_V151)
    {
        uint8_t channel_id = BUZZER_PWM_CHANNEL;

        /*
         * PWM v151: 通道必须先归属到组，否则 start(channel) 会失败。
         * 当组已被设置时可能返回 ERRCODE_PWM_INVALID_PARAMETER，这里按“已就绪”处理。
         */
        ret = uapi_pwm_set_group(BUZZER_PWM_GROUP_ID, &channel_id, 1U);
        if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_PWM_INVALID_PARAMETER)) {
            return ret;
        }
    }
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief 播放单个音符。
 *
 * @param note 音符参数，freq_hz=0 表示休止符。
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

    (void)uapi_pwm_close(BUZZER_PWM_CHANNEL);
    (void)buzzer_force_silent();
    return ERRCODE_SUCC;
}

/**
 * @brief 蜂鸣器任务：循环播放多音调。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *buzzer_task(const char *arg)
{
    errcode_t ret;
    uint32_t i;

    UNUSED(arg);

    ret = buzzer_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_beep] init failed, ret=0x%x\r\n", (unsigned int)ret);
        return NULL;
    }

    osal_printk("[mine_beep] start on GPIO3/PWM3 (mux1), playing tones\r\n");

    while (1) {
        for (i = 0U; i < (uint32_t)(sizeof(g_buzzer_scale) / sizeof(g_buzzer_scale[0])); i++) {
            ret = buzzer_play_note(&g_buzzer_scale[i]);
            if (ret != ERRCODE_SUCC) {
                osal_printk("[mine_beep] play note failed, idx=%u, ret=0x%x\r\n",
                    (unsigned int)i,
                    (unsigned int)ret);
            }

            (void)uapi_watchdog_kick();
            uapi_tcxo_delay_ms(BUZZER_NOTE_GAP_MS);
        }

        /* 每轮音阶之间增加停顿，便于观察播放边界。 */
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
    task_handle = osal_kthread_create((osal_kthread_handler)buzzer_task, 0, "BuzzerTask", BUZZER_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, BUZZER_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the buzzer_entry. */
app_run(buzzer_entry);