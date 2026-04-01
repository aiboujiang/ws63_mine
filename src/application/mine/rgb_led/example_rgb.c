/**
 * @file example_rgb.c
 * @brief 外部板卡 PWM demo 的 WS63 适配版（GPIO4 -> PWM4）
 *
 * 说明：
 * - 在 PWM V151 模式下，按 WS2812 协议发送 GRB 数据，实现红/绿/蓝/白/灭循环。
 * - 在 PWM V150 模式下，保留原 demo 的两种占空比波形切换行为作为兼容回退。
 */

#include "common_def.h"
#include "app_init.h"
#include "pinctrl.h"
#include "pwm.h"
#include "gpio.h"
#include "watchdog.h"
#include "tcxo.h"
#include "soc_osal.h"

#define MINE_RGB_TASK_PRIO            24
#define MINE_RGB_TASK_STACK_SIZE      0x1000

#define MINE_RGB_PWM_PIN              GPIO_04
#define MINE_RGB_PWM_PIN_MODE         PIN_MODE_1
#define MINE_RGB_PWM_CHANNEL          4U
#define MINE_RGB_PWM_GROUP            1U

#define MINE_RGB_STEP_HOLD_MS         500U
#define MINE_RGB_GAP_MS               10U
#define MINE_RGB_SEND_TIMEOUT_US      3000U
#define MINE_RGB_START_DELAY_MS       30000U

#define MINE_WS2812_BITS_PER_LED      24U
#define MINE_WS2812_RESET_SLOTS       84U
#define MINE_WS2812_FRAME_SLOTS       (MINE_WS2812_BITS_PER_LED + MINE_WS2812_RESET_SLOTS)
#define MINE_WS2812_RATE_HZ           800000U

/*
 * 下面两组参数保持与原始 demo 行为一致：
 * - PatternA: 高低各 60 tick（50%）
 * - PatternB: 高 60 / 低 20 tick（75%）
 */
#define MINE_RGB_PATTERN_A_HIGH       60U
#define MINE_RGB_PATTERN_A_LOW        60U
#define MINE_RGB_PATTERN_B_HIGH       60U
#define MINE_RGB_PATTERN_B_LOW        20U

#if defined(CONFIG_PWM_USING_V151)
static pwm_config_t g_ws2812_frame[MINE_WS2812_FRAME_SLOTS];
static volatile uint16_t g_ws2812_next_idx = 0;
static volatile uint16_t g_ws2812_frame_len = 0;
static volatile bool g_ws2812_frame_done = false;
static bool g_ws2812_session_ready = false;
static uint32_t g_ws2812_total_ticks = 0;
static uint32_t g_ws2812_t0h_ticks = 0;
static uint32_t g_ws2812_t1h_ticks = 0;
#endif

/**
 * @brief 启动 PWM 输出（兼容 V150/V151）。
 */
static errcode_t mine_rgb_start_output(void)
{
#if defined(CONFIG_PWM_USING_V151)
    return uapi_pwm_start_group(MINE_RGB_PWM_GROUP);
#else
    return uapi_pwm_start(MINE_RGB_PWM_CHANNEL);
#endif
}

/**
 * @brief 停止 PWM 输出（兼容 V150/V151）。
 */
static void mine_rgb_stop_output(void)
{
#if defined(CONFIG_PWM_USING_V151)
    (void)uapi_pwm_stop_group(MINE_RGB_PWM_GROUP);
#else
    (void)uapi_pwm_stop(MINE_RGB_PWM_CHANNEL);
#endif
}

/**
 * @brief 任务态延时：分片休眠并喂狗，避免长忙等触发看门狗复位。
 */
static void mine_rgb_task_delay_ms(uint32_t delay_ms)
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
 * @brief 将输出脚切到 GPIO 低电平，形成两段 PWM 之间的间隙。
 */
#if !defined(CONFIG_PWM_USING_V151)
static void mine_rgb_force_low_gap(uint32_t hold_ms)
{
    mine_rgb_stop_output();
    (void)uapi_pin_set_mode(MINE_RGB_PWM_PIN, PIN_MODE_0);
    (void)uapi_gpio_set_dir(MINE_RGB_PWM_PIN, GPIO_DIRECTION_OUTPUT);
    (void)uapi_gpio_set_val(MINE_RGB_PWM_PIN, GPIO_LEVEL_LOW);
    mine_rgb_task_delay_ms(hold_ms);
    (void)uapi_pin_set_mode(MINE_RGB_PWM_PIN, MINE_RGB_PWM_PIN_MODE);
}

/**
 * @brief 重新配置 PWM 占空比并启动输出。
 */
static errcode_t mine_rgb_start_pattern(uint32_t high_ticks, uint32_t low_ticks)
{
    pwm_config_t cfg = {
        .low_time = low_ticks,
        .high_time = high_ticks,
        .offset_time = 0,
        .cycles = 0,
        .repeat = true
    };

    errcode_t ret = uapi_pwm_open(MINE_RGB_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm open failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    ret = mine_rgb_start_output();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm start failed, ret=%d\r\n", (int)ret);
    }
    return ret;
}
#endif

#if defined(CONFIG_PWM_USING_V151)
/**
 * @brief PWM 周期回调：在当前 bit 结束时预装下一 bit。
 */
static errcode_t mine_rgb_ws2812_cb(uint8_t channel)
{
    if (channel != MINE_RGB_PWM_CHANNEL) {
        return ERRCODE_SUCC;
    }

    if (g_ws2812_next_idx < g_ws2812_frame_len) {
        (void)uapi_pwm_config_preload(MINE_RGB_PWM_GROUP,
                                      MINE_RGB_PWM_CHANNEL,
                                      &g_ws2812_frame[g_ws2812_next_idx]);
        g_ws2812_next_idx++;
    } else {
        /* 仅在最后一个配置周期实际完成后才置完成，避免提前停止导致尾码不完整。 */
        g_ws2812_frame_done = true;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief 计算 WS2812 对应 PWM tick。
 */
static errcode_t mine_rgb_ws2812_calc_ticks(void)
{
    uint32_t clk_hz = uapi_pwm_get_frequency(MINE_RGB_PWM_CHANNEL);
    if (clk_hz == 0U) {
        return ERRCODE_FAIL;
    }

    g_ws2812_total_ticks = (clk_hz + (MINE_WS2812_RATE_HZ / 2U)) / MINE_WS2812_RATE_HZ;
    if (g_ws2812_total_ticks < 4U) {
        return ERRCODE_FAIL;
    }

    g_ws2812_t0h_ticks = (g_ws2812_total_ticks * 28U + 50U) / 100U;
    g_ws2812_t1h_ticks = (g_ws2812_total_ticks * 56U + 50U) / 100U;

    if (g_ws2812_t0h_ticks == 0U) {
        g_ws2812_t0h_ticks = 1U;
    }
    if (g_ws2812_t1h_ticks <= g_ws2812_t0h_ticks) {
        g_ws2812_t1h_ticks = g_ws2812_t0h_ticks + 1U;
    }
    if (g_ws2812_t1h_ticks >= g_ws2812_total_ticks) {
        g_ws2812_t1h_ticks = g_ws2812_total_ticks - 1U;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief 构造单灯 WS2812 帧（GRB，高位先发）。
 */
static void mine_rgb_ws2812_build_frame(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;

    for (uint32_t i = 0; i < MINE_WS2812_BITS_PER_LED; i++) {
        uint32_t bit = (grb >> (23U - i)) & 0x1U;
        uint32_t high_ticks = (bit != 0U) ? g_ws2812_t1h_ticks : g_ws2812_t0h_ticks;
        uint32_t low_ticks = g_ws2812_total_ticks - high_ticks;

        g_ws2812_frame[i].low_time = low_ticks;
        g_ws2812_frame[i].high_time = high_ticks;
        g_ws2812_frame[i].offset_time = 0;
        g_ws2812_frame[i].cycles = 1;
        g_ws2812_frame[i].repeat = false;
    }

    for (uint32_t i = MINE_WS2812_BITS_PER_LED; i < MINE_WS2812_FRAME_SLOTS; i++) {
        g_ws2812_frame[i].low_time = g_ws2812_total_ticks;
        g_ws2812_frame[i].high_time = 0;
        g_ws2812_frame[i].offset_time = 0;
        g_ws2812_frame[i].cycles = 1;
        g_ws2812_frame[i].repeat = false;
    }
}

/**
 * @brief 发送单帧颜色。
 */
static errcode_t mine_rgb_ws2812_send_color(uint8_t r, uint8_t g, uint8_t b)
{
    errcode_t ret;

    if (!g_ws2812_session_ready) {
        return ERRCODE_PWM_NOT_INIT;
    }

    mine_rgb_ws2812_build_frame(r, g, b);

    g_ws2812_frame_len = MINE_WS2812_FRAME_SLOTS;
    g_ws2812_next_idx = 1U;
    g_ws2812_frame_done = false;

    /* 先预装首个周期，回调里再按序补齐后续周期。 */
    ret = uapi_pwm_config_preload(MINE_RGB_PWM_GROUP, MINE_RGB_PWM_CHANNEL, &g_ws2812_frame[0]);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] ws preload[0] failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    ret = mine_rgb_start_output();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] ws start failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    uint32_t wait_us = 0;
    while (!g_ws2812_frame_done) {
        uapi_tcxo_delay_us(1);
        wait_us++;
        if (wait_us > MINE_RGB_SEND_TIMEOUT_US) {
            osal_printk("[mine_rgb_led] ws timeout idx=%u/%u\r\n",
                        (unsigned int)g_ws2812_next_idx,
                        (unsigned int)g_ws2812_frame_len);
            break;
        }
    }

    uapi_tcxo_delay_us(2);
    mine_rgb_stop_output();

    return (wait_us > MINE_RGB_SEND_TIMEOUT_US) ? ERRCODE_FAIL : ERRCODE_SUCC;
}
#endif

/**
 * @brief 初始化 PWM4 与分组关系。
 */
static errcode_t mine_rgb_pwm_init(void)
{
    errcode_t ret;

    (void)uapi_pin_set_mode(MINE_RGB_PWM_PIN, MINE_RGB_PWM_PIN_MODE);

    uapi_pwm_deinit();
    ret = uapi_pwm_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm init failed, ret=%d\r\n", (int)ret);
        return ret;
    }

#if defined(CONFIG_PWM_USING_V151)
    pwm_config_t idle_cfg = {
        .low_time = 1,
        .high_time = 1,
        .offset_time = 0,
        .cycles = 1,
        .repeat = false
    };

    {
        uint8_t channel_set = MINE_RGB_PWM_CHANNEL;

        (void)uapi_pwm_clear_group(MINE_RGB_PWM_GROUP);
        ret = uapi_pwm_set_group(MINE_RGB_PWM_GROUP, &channel_set, 1);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pwm set group failed, ret=%d\r\n", (int)ret);
            return ret;
        }
    }

    ret = mine_rgb_ws2812_calc_ticks();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] ws tick calc failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    if (g_ws2812_total_ticks > 2U) {
        idle_cfg.low_time = g_ws2812_total_ticks - 1U;
        idle_cfg.high_time = 1U;
    }

    ret = uapi_pwm_open(MINE_RGB_PWM_CHANNEL, &idle_cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] ws init open failed, ret=%d\r\n", (int)ret);
        return ret;
    }

    ret = uapi_pwm_register_interrupt(MINE_RGB_PWM_CHANNEL, mine_rgb_ws2812_cb);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] ws init irq reg failed, ret=%d\r\n", (int)ret);
        (void)uapi_pwm_close(MINE_RGB_PWM_CHANNEL);
        return ret;
    }

    g_ws2812_session_ready = true;
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief PWM 演示线程：周期输出两种占空比波形。
 */
static void *mine_rgb_pwm_task(const char *arg)
{
    UNUSED(arg);
    osal_printk("[mine_rgb_led] example_rgb ws63 adaptation start\r\n");
#if defined(CONFIG_PWM_USING_V151)
    osal_printk("[mine_rgb_led] pwm backend: v151(ws2812 color mode)\r\n");
#else
    osal_printk("[mine_rgb_led] pwm backend: v150(single channel mode)\r\n");
#endif

    if (mine_rgb_pwm_init() != ERRCODE_SUCC) {
        return NULL;
    }

    /* 启动后延时，避开系统射频/校准高风险窗口。 */
    mine_rgb_task_delay_ms(MINE_RGB_START_DELAY_MS);

    while (1) {
        (void)uapi_watchdog_kick();

#if defined(CONFIG_PWM_USING_V151)
        static const uint8_t color_table[][3] = {
            {255, 0, 0},
            {0, 255, 0},
            {0, 0, 255},
            {255, 255, 255},
            {0, 0, 0}
        };

        for (uint32_t i = 0; i < (sizeof(color_table) / sizeof(color_table[0])); i++) {
            if (mine_rgb_ws2812_send_color(color_table[i][0], color_table[i][1], color_table[i][2]) == ERRCODE_SUCC) {
                osal_printk("[mine_rgb_led] color step=%u ok\r\n", (unsigned int)i);
            } else {
                osal_printk("[mine_rgb_led] color step=%u failed\r\n", (unsigned int)i);
            }
            mine_rgb_task_delay_ms(MINE_RGB_STEP_HOLD_MS);
        }
#else
        if (mine_rgb_start_pattern(MINE_RGB_PATTERN_A_HIGH, MINE_RGB_PATTERN_A_LOW) == ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pattern A: H=%u L=%u\r\n",
                        (unsigned int)MINE_RGB_PATTERN_A_HIGH,
                        (unsigned int)MINE_RGB_PATTERN_A_LOW);
        }
        mine_rgb_task_delay_ms(MINE_RGB_STEP_HOLD_MS);

        mine_rgb_force_low_gap(MINE_RGB_GAP_MS);

        if (mine_rgb_start_pattern(MINE_RGB_PATTERN_B_HIGH, MINE_RGB_PATTERN_B_LOW) == ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pattern B: H=%u L=%u\r\n",
                        (unsigned int)MINE_RGB_PATTERN_B_HIGH,
                        (unsigned int)MINE_RGB_PATTERN_B_LOW);
        }
        mine_rgb_task_delay_ms(MINE_RGB_STEP_HOLD_MS);

        mine_rgb_force_low_gap(MINE_RGB_GAP_MS);
#endif
    }

    return NULL;
}

/**
 * @brief 示例入口。
 */
static void mine_rgb_pwm_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mine_rgb_pwm_task,
                                      0,
                                      "MineRgbPwmTask",
                                      MINE_RGB_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MINE_RGB_TASK_PRIO);
    }
    osal_kthread_unlock();
}

/* Run the mine_rgb_pwm_entry. */
app_run(mine_rgb_pwm_entry);