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
#include "pwm.h"
#include "dma.h"
#include "tcxo.h"
#include "soc_osal.h"
#include "app_init.h"
#include "gpio.h"
#include "watchdog.h"

#define WS2812_TASK_PRIO 24
#define WS2812_TASK_STACK_SIZE 0x1000

/* 硬件连接：示例默认使用 GPIO9 的 PWM 复用（与原验证代码一致）。 */
#define WS2812_PWM_PIN 9
#define WS2812_PWM_PIN_MODE 1
#define WS2812_PWM_CHANNEL 1
#define WS2812_PWM_GROUP 1

#define WS2812_LED_COUNT 1
#define WS2812_BITS_PER_LED 24
#define WS2812_FRAME_SYMBOLS (WS2812_LED_COUNT * WS2812_BITS_PER_LED)

/*
 * WS2812B 时序来自 mine/lib/RGB_LED.pdf：
 * Tin0h=0.295us, Tin1h=0.595us, T0L=0.595us, T1L=0.295us。
 * 另外文档备注建议复位低电平至少 100us。
 */
#define WS2812_T0H_NS 295U
#define WS2812_T0L_NS 595U
#define WS2812_T1H_NS 595U
#define WS2812_T1L_NS 295U
#define WS2812_RESET_US 120U

#define WS2812_DMA_WAIT_US 2000U
#define WS2812_PWM_WAIT_US 3000U

#define WS2812_BREATH_MIN_PERCENT 8U
#define WS2812_BREATH_MAX_PERCENT 100U
#define WS2812_BREATH_STEP_PERCENT 4U
#define WS2812_FRAME_INTERVAL_MS 20U

/* DMA 配置取值参考 include/driver/dma.h 里的字段注释。 */
#define WS2812_DMA_WIDTH_WORD 2U
#define WS2812_DMA_PRIORITY_HIGH 3U
#define WS2812_DMA_INTR_TFR 0U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_color_t;

static const ws2812_color_t g_color_table[] = {
    {255U, 0U, 0U},
    {0U, 255U, 0U},
    {0U, 0U, 255U},
    {255U, 120U, 0U},
    {0U, 255U, 180U},
    {180U, 0U, 255U}
};

/* bit0/bit1 的 PWM 模板，在时序校准后初始化。 */
static pwm_config_t g_bit_cfg[2];
/* staging -> active 通过 DMA 拷贝，避免发送时被上层改写。 */
static pwm_config_t g_frame_staging[WS2812_FRAME_SYMBOLS];
static pwm_config_t g_frame_active[WS2812_FRAME_SYMBOLS];
static uint8_t g_grb_frame[WS2812_LED_COUNT * 3U];

static volatile bool g_dma_done = false;
static volatile bool g_dma_error = false;
static volatile bool g_pwm_sending = false;
static volatile uint16_t g_symbol_index = 0U;
static uint16_t g_symbol_total = 0U;

static uint8_t g_pwm_group_channel = WS2812_PWM_CHANNEL;
static uint8_t g_color_index = 0U;
static uint8_t g_breath_percent = WS2812_BREATH_MIN_PERCENT;
static int8_t g_breath_step = (int8_t)WS2812_BREATH_STEP_PERCENT;
static uint32_t g_pwm_clk_hz = 0U;

/**
 * @brief 将纳秒时间转换为 PWM 计数值。
 *
 * @param time_ns 时间（ns）。
 * @return uint32_t 对应计数值，最小为 1。
 */
static uint32_t ws2812_ns_to_count(uint32_t time_ns)
{
    uint64_t count;

    if (g_pwm_clk_hz == 0U) {
        g_pwm_clk_hz = uapi_pwm_get_frequency(WS2812_PWM_CHANNEL);
    }
    count = ((uint64_t)time_ns * (uint64_t)g_pwm_clk_hz + 999999999ULL) / 1000000000ULL;
    if (count == 0ULL) {
        count = 1ULL;
    }
    return (uint32_t)count;
}

/**
 * @brief 线性亮度缩放。
 *
 * @param value 原始颜色分量。
 * @param percent 亮度百分比。
 * @return uint8_t 缩放后的分量值。
 */
static uint8_t ws2812_apply_brightness(uint8_t value, uint8_t percent)
{
    uint32_t scaled = ((uint32_t)value * (uint32_t)percent + 50U) / 100U;
    if (scaled > 255U) {
        scaled = 255U;
    }
    return (uint8_t)scaled;
}

/**
 * @brief 计算并更新 bit0/bit1 的 PWM 模板。
 */
static void ws2812_calibrate_timing(void)
{
    uint32_t t0h = ws2812_ns_to_count(WS2812_T0H_NS);
    uint32_t t0l = ws2812_ns_to_count(WS2812_T0L_NS);
    uint32_t t1h = ws2812_ns_to_count(WS2812_T1H_NS);
    uint32_t t1l = ws2812_ns_to_count(WS2812_T1L_NS);

    g_bit_cfg[0].low_time = t0l;
    g_bit_cfg[0].high_time = t0h;
    g_bit_cfg[0].offset_time = 0U;
    g_bit_cfg[0].cycles = 1U;
    g_bit_cfg[0].repeat = false;

    g_bit_cfg[1].low_time = t1l;
    g_bit_cfg[1].high_time = t1h;
    g_bit_cfg[1].offset_time = 0U;
    g_bit_cfg[1].cycles = 1U;
    g_bit_cfg[1].repeat = false;

    osal_printk("[mine_rgb_led] pwm clk=%uHz, count T0H/T0L/T1H/T1L=%u/%u/%u/%u\r\n",
        (unsigned int)g_pwm_clk_hz,
        (unsigned int)t0h,
        (unsigned int)t0l,
        (unsigned int)t1h,
        (unsigned int)t1l);
}

/**
 * @brief 切换到 GPIO 输出低电平并保持复位时间。
 *
 * 这样可以确保每帧后都有明确的 RESET 窗口，避免灯珠误判帧边界。
 */
static void ws2812_hold_reset_low(void)
{
    (void)uapi_pin_set_mode(WS2812_PWM_PIN, GPIO_FUNC);
    (void)uapi_gpio_set_dir(WS2812_PWM_PIN, GPIO_DIRECTION_OUTPUT);
    (void)uapi_gpio_set_val(WS2812_PWM_PIN, GPIO_LEVEL_LOW);
    uapi_tcxo_delay_us(WS2812_RESET_US);
    (void)uapi_pin_set_mode(WS2812_PWM_PIN, WS2812_PWM_PIN_MODE);
}

/**
 * @brief DMA 传输完成/错误回调。
 *
 * @param intr DMA 中断类型。
 * @param channel DMA 通道。
 * @param arg 用户参数。
 */
static void ws2812_dma_callback(uint8_t intr, uint8_t channel, uintptr_t arg)
{
    unused(channel);
    unused(arg);

    if (intr == WS2812_DMA_INTR_TFR) {
        g_dma_done = true;
    } else {
        g_dma_error = true;
    }
}

/**
 * @brief 使用 DMA 将 staging 帧复制到 active 帧。
 *
 * WS63 当前 DMA 握手源里没有 PWM 通道，因此这里采用“DMA 帧准备 + PWM 定时发送”的组合。
 * 这样既满足 PWM+DMA 架构，也能保证 WS2812 发送阶段的时序稳定。
 *
 * @param symbols 待复制的符号数。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t ws2812_dma_copy_frame(uint16_t symbols)
{
    dma_ch_user_memory_config_t cfg;
    uint32_t wait_us = WS2812_DMA_WAIT_US;
    uint32_t transfer_words;
    errcode_t ret;

    if ((symbols == 0U) || (symbols > WS2812_FRAME_SYMBOLS)) {
        return ERRCODE_INVALID_PARAM;
    }

    transfer_words = ((uint32_t)symbols * (uint32_t)sizeof(pwm_config_t)) / sizeof(uint32_t);

    cfg.src = (uint32_t)(uintptr_t)g_frame_staging;
    cfg.dest = (uint32_t)(uintptr_t)g_frame_active;
    cfg.transfer_num = (uint16_t)transfer_words;
    cfg.priority = WS2812_DMA_PRIORITY_HIGH;
    cfg.width = WS2812_DMA_WIDTH_WORD;

    g_dma_done = false;
    g_dma_error = false;
    ret = uapi_dma_transfer_memory_single(&cfg, ws2812_dma_callback, (uintptr_t)symbols);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* DMA copy 完成前不进入发送流程，避免发送与改写并发。 */
    while ((!g_dma_done) && (!g_dma_error) && (wait_us > 0U)) {
        uapi_tcxo_delay_us(1U);
        wait_us--;
    }

    if ((!g_dma_done) || g_dma_error) {
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief PWM 周期装载中断回调。
 *
 * @param channel PWM 通道。
 * @return errcode_t 固定返回 ERRCODE_SUCC。
 */
static errcode_t ws2812_pwm_callback(uint8_t channel)
{
    if (!g_pwm_sending) {
        return ERRCODE_SUCC;
    }

    if (g_symbol_index < g_symbol_total) {
        (void)uapi_pwm_config_preload(WS2812_PWM_GROUP, channel, &g_frame_active[g_symbol_index]);
        g_symbol_index++;
    } else {
        g_pwm_sending = false;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 将 GRB 字节帧编码为 PWM 符号帧。
 *
 * 每个 bit 映射为一组 high/low 时长，顺序严格按“高位先发”。
 */
static void ws2812_encode_frame(void)
{
    uint16_t symbol = 0U;
    uint16_t idx;

    for (idx = 0U; idx < (uint16_t)sizeof(g_grb_frame); idx++) {
        uint8_t value = g_grb_frame[idx];
        uint8_t bit;
        for (bit = 0U; bit < 8U; bit++) {
            uint8_t code = (uint8_t)((value & 0x80U) ? 1U : 0U);
            g_frame_staging[symbol] = g_bit_cfg[code];
            value <<= 1U;
            symbol++;
        }
    }

    g_symbol_total = symbol;
}

/**
 * @brief 按当前颜色与亮度生成 GRB 帧。
 */
static void ws2812_build_grb_frame(void)
{
    const ws2812_color_t *color = &g_color_table[g_color_index];
    uint8_t r = ws2812_apply_brightness(color->r, g_breath_percent);
    uint8_t g = ws2812_apply_brightness(color->g, g_breath_percent);
    uint8_t b = ws2812_apply_brightness(color->b, g_breath_percent);
    uint16_t led;

    for (led = 0U; led < WS2812_LED_COUNT; led++) {
        uint16_t base = (uint16_t)(led * 3U);
        g_grb_frame[base + 0U] = g;
        g_grb_frame[base + 1U] = r;
        g_grb_frame[base + 2U] = b;
    }
}

/**
 * @brief 更新“呼吸亮度 + 颜色切换”状态机。
 */
static void ws2812_step_breath(void)
{
    int16_t next_percent = (int16_t)g_breath_percent + (int16_t)g_breath_step;

    if (next_percent >= (int16_t)WS2812_BREATH_MAX_PERCENT) {
        g_breath_percent = WS2812_BREATH_MAX_PERCENT;
        g_breath_step = -((int8_t)WS2812_BREATH_STEP_PERCENT);
        return;
    }

    if (next_percent <= (int16_t)WS2812_BREATH_MIN_PERCENT) {
        g_breath_percent = WS2812_BREATH_MIN_PERCENT;
        g_breath_step = (int8_t)WS2812_BREATH_STEP_PERCENT;
        g_color_index++;
        if (g_color_index >= (uint8_t)(sizeof(g_color_table) / sizeof(g_color_table[0]))) {
            g_color_index = 0U;
        }
        return;
    }

    g_breath_percent = (uint8_t)next_percent;
}

/**
 * @brief 发出一帧 WS2812 数据。
 *
 * @return errcode_t ERRCODE_SUCC 表示发送成功。
 */
static errcode_t ws2812_send_frame(void)
{
    uint32_t wait_us = WS2812_PWM_WAIT_US;
    errcode_t ret;

    g_symbol_index = 1U;
    g_pwm_sending = true;

    ret = uapi_pwm_open(WS2812_PWM_CHANNEL, &g_frame_active[0]);
    if (ret != ERRCODE_SUCC) {
        g_pwm_sending = false;
        return ret;
    }

    ret = uapi_pwm_start(WS2812_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        g_pwm_sending = false;
        (void)uapi_pwm_close(WS2812_PWM_CHANNEL);
        return ret;
    }

    /* 等待回调推送完整帧，期间持续喂狗避免系统复位。 */
    while (g_pwm_sending && (wait_us > 0U)) {
        (void)uapi_watchdog_kick();
        uapi_tcxo_delay_us(1U);
        wait_us--;
    }

    (void)uapi_pwm_stop_group(WS2812_PWM_GROUP);
    (void)uapi_pwm_close(WS2812_PWM_CHANNEL);
    ws2812_hold_reset_low();

    if (wait_us == 0U) {
        return ERRCODE_FAIL;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief 初始化 WS2812 所需的 GPIO/PWM/DMA 资源。
 *
 * @return errcode_t ERRCODE_SUCC 表示初始化成功。
 */
static errcode_t ws2812_init(void)
{
    errcode_t ret;

    uapi_gpio_init();

    ret = uapi_pin_set_mode(WS2812_PWM_PIN, WS2812_PWM_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_pull(WS2812_PWM_PIN, PIN_PULL_TYPE_DISABLE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_set_group(WS2812_PWM_GROUP, &g_pwm_group_channel, 1U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_register_interrupt(WS2812_PWM_CHANNEL, ws2812_pwm_callback);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_dma_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_dma_open();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_pwm_clk_hz = uapi_pwm_get_frequency(WS2812_PWM_CHANNEL);
    ws2812_calibrate_timing();
    ws2812_hold_reset_low();
    return ERRCODE_SUCC;
}

/**
 * @brief WS2812 多色呼吸任务。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *ws2812_task(const char *arg)
{
    errcode_t ret;

    (void)arg;
    osal_printk("\r\n[mine_rgb_led] WS2812 PWM+DMA breathing start\r\n");

    ret = ws2812_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] init failed, ret=%d\r\n", (int)ret);
        return NULL;
    }

    while (1) {
        ws2812_build_grb_frame();
        ws2812_encode_frame();

        ret = ws2812_dma_copy_frame(g_symbol_total);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] dma copy failed, ret=%d\r\n", (int)ret);
            uapi_tcxo_delay_ms(5U);
            continue;
        }

        ret = ws2812_send_frame();
        if (ret != ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] pwm send failed, ret=%d\r\n", (int)ret);
            uapi_tcxo_delay_ms(5U);
            continue;
        }

        ws2812_step_breath();
        (void)uapi_watchdog_kick();
        uapi_tcxo_delay_ms(WS2812_FRAME_INTERVAL_MS);
    }
}

/**
 * @brief 创建 WS2812 任务。
 */
static void ws2812_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)ws2812_task,
                                      0,
                                      "Ws2812Task",
                                      WS2812_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WS2812_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the ws2812_entry. */
app_run(ws2812_entry);