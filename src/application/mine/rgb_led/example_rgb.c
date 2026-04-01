/**
 * @file example_rgb.c
 * @brief PWM 初版可用演示（GPIO4 -> PWM4）
 *
 * 说明：
 * - 回退为最简单的占空比切换模式，避免 WS2812 preload/中断链路。
 * - 周期输出两组波形：50% 与 75%，用于确认 PWM 通道工作正常。
 */

#include "common_def.h"
#include "app_init.h"
#include "pinctrl.h"
#include "pwm.h"
#include "gpio.h"
#include "watchdog.h"
#include "soc_osal.h"

#define PWM_TASK_PRIO                24
#define PWM_TASK_STACK_SIZE          0x1000

#define CONFIG_PWM_PIN               GPIO_04
#define CONFIG_PWM_PIN_MODE          PIN_MODE_1
#define PWM_CHANNEL                  4U
#define PWM_GROUP_ID                 1U

#define PWM_STEP_HOLD_MS             1000U
#define PWM_LOW_GAP_MS               10U

#define PWM_PATTERN_A_HIGH           60U
#define PWM_PATTERN_A_LOW            60U
#define PWM_PATTERN_B_HIGH           60U
#define PWM_PATTERN_B_LOW            20U

/**
 * @brief 任务态延时：分片休眠并喂狗，避免长延时触发看门狗。
 */
static void pwm_task_delay_ms(uint32_t delay_ms)
{
    const uint32_t slice_ms = 20U;

    while (delay_ms > 0U) {
        uint32_t curr_ms = (delay_ms > slice_ms) ? slice_ms : delay_ms;
        (void)uapi_watchdog_kick();
        (void)osal_msleep(curr_ms);
        delay_ms -= curr_ms;
    }
}

/**
 * @brief 启动 PWM 输出（兼容 V150/V151）。
 */
static errcode_t pwm_start_output(void)
{
#if defined(CONFIG_PWM_USING_V151)
    return uapi_pwm_start_group(PWM_GROUP_ID);
#else
    return uapi_pwm_start(PWM_CHANNEL);
#endif
}

/**
 * @brief 停止 PWM 输出（兼容 V150/V151）。
 */
static void pwm_stop_output(void)
{
#if defined(CONFIG_PWM_USING_V151)
    (void)uapi_pwm_stop_group(PWM_GROUP_ID);
#else
    (void)uapi_pwm_stop(PWM_CHANNEL);
#endif
}

/**
 * @brief 输出低电平间隙，便于观察两段波形切换。
 */
static void pwm_force_low_gap(uint32_t hold_ms)
{
    pwm_stop_output();
    (void)uapi_pin_set_mode(CONFIG_PWM_PIN, PIN_MODE_0);
    (void)uapi_gpio_set_dir(CONFIG_PWM_PIN, GPIO_DIRECTION_OUTPUT);
    (void)uapi_gpio_set_val(CONFIG_PWM_PIN, GPIO_LEVEL_LOW);
    pwm_task_delay_ms(hold_ms);
    (void)uapi_pin_set_mode(CONFIG_PWM_PIN, CONFIG_PWM_PIN_MODE);
}

/**
 * @brief 按指定高低电平 tick 重新配置并启动 PWM。
 */
static errcode_t pwm_start_pattern(uint32_t high_ticks, uint32_t low_ticks)
{
    pwm_config_t cfg = {
        .low_time = low_ticks,
        .high_time = high_ticks,
        .offset_time = 0,
        .cycles = 0,
        .repeat = true
    };

    errcode_t ret = uapi_pwm_open(PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm open failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    ret = pwm_start_output();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm start failed, ret=%d\r\n", (int)ret);
    }
    return ret;
}

/**
 * @brief PWM 演示初始化。
 */
static errcode_t pwm_demo_init(void)
{
    errcode_t ret;

    (void)uapi_pin_set_mode(CONFIG_PWM_PIN, CONFIG_PWM_PIN_MODE);

    ret = uapi_pwm_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm init failed, ret=%d\r\n", (int)ret);
        return ret;
    }

#if defined(CONFIG_PWM_USING_V151)
    {
        uint8_t channel_id = PWM_CHANNEL;
        (void)uapi_pwm_clear_group(PWM_GROUP_ID);
        ret = uapi_pwm_set_group(PWM_GROUP_ID, &channel_id, 1);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pwm set group failed, ret=%d\r\n", (int)ret);
            return ret;
        }
    }
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief PWM 演示任务：50% 与 75% 占空比循环切换。
 */
static void *pwm_task(const char *arg)
{
    UNUSED(arg);
    osal_printk("[mine_rgb_led] demo 000_rgb hello world!\r\n");

    if (pwm_demo_init() != ERRCODE_SUCC) {
        return NULL;
    }

    while (1) {
        (void)uapi_watchdog_kick();

        if (pwm_start_pattern(PWM_PATTERN_A_HIGH, PWM_PATTERN_A_LOW) == ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pattern A: H=%u L=%u\r\n",
                        (unsigned int)PWM_PATTERN_A_HIGH,
                        (unsigned int)PWM_PATTERN_A_LOW);
        }
        pwm_task_delay_ms(PWM_STEP_HOLD_MS);

        pwm_force_low_gap(PWM_LOW_GAP_MS);

        if (pwm_start_pattern(PWM_PATTERN_B_HIGH, PWM_PATTERN_B_LOW) == ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pattern B: H=%u L=%u\r\n",
                        (unsigned int)PWM_PATTERN_B_HIGH,
                        (unsigned int)PWM_PATTERN_B_LOW);
        }
        pwm_task_delay_ms(PWM_STEP_HOLD_MS);

        pwm_force_low_gap(PWM_LOW_GAP_MS);
    }

    return NULL;
}

/**
 * @brief 任务入口。
 */
static void pwm_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)pwm_task, 0, "PwmTask", PWM_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, PWM_TASK_PRIO);
    }
    osal_kthread_unlock();
}

/* Run the pwm_entry. */
app_run(pwm_entry);
