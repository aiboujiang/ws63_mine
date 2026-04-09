/**
 * @file ws63_encoder.c
 * @brief WS63 编码器测速驱动层实现。
 */

#include "ws63_encoder.h"

#include "soc_osal.h"

#include "ws63_final_bsp.h"
#include "ws63_final_config.h"

/* ISR 与任务并发共享的计数数据。 */
static volatile int32_t g_ws63_encoder_total_count = 0;
static volatile int32_t g_ws63_encoder_window_count = 0;

/* 周期采样产出的可读状态。 */
static int32_t g_ws63_encoder_last_delta = 0;
static int32_t g_ws63_encoder_rpm = 0;
static uint32_t g_ws63_encoder_last_sample_ms = 0;
static uint8_t g_ws63_encoder_ready = 0U;

/**
 * @brief 进入临界区，保护 ISR 与任务并发读写。
 */
static unsigned int ws63_encoder_irq_lock(void)
{
    return osal_irq_lock();
}

/**
 * @brief 退出临界区。
 */
static void ws63_encoder_irq_unlock(unsigned int irq_status)
{
    osal_irq_restore(irq_status);
}

/**
 * @brief A 相上升沿中断处理。
 */
static void ws63_encoder_a_rise_isr(pin_t pin, uintptr_t param)
{
    int32_t step;

    (void)pin;
    (void)param;

    /*
     * 方向判定规则：
     * 1) B=0 记为正向，计数 +1；
     * 2) B=1 记为反向，计数 -1。
     */
    step = (ws63_bsp_encoder_get_b_level() == 0U) ? 1 : -1;
    g_ws63_encoder_total_count += step;
    g_ws63_encoder_window_count += step;
}

/**
 * @brief 初始化编码器驱动。
 */
errcode_t ws63_encoder_init(void)
{
    errcode_t ret;

    ret = ws63_bsp_encoder_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_encoder_register_a_isr(ws63_encoder_a_rise_isr);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ws63_encoder_reset();
    g_ws63_encoder_ready = 1U;
    return ERRCODE_SUCC;
}

/**
 * @brief 周期采样编码器，更新最新 RPM。
 */
void ws63_encoder_sample(uint32_t now_ms)
{
    uint32_t elapsed_ms;
    unsigned int irq_status;
    int32_t delta_count;
    int64_t numerator;
    int64_t denominator;

    if (g_ws63_encoder_ready == 0U) {
        return;
    }

    if (g_ws63_encoder_last_sample_ms == 0U) {
        g_ws63_encoder_last_sample_ms = now_ms;
        return;
    }

    elapsed_ms = now_ms - g_ws63_encoder_last_sample_ms;
    if (elapsed_ms < WS63_ENCODER_SAMPLE_MS) {
        return;
    }

    /* 原子取走窗口计数，避免 ISR 与任务并发导致统计不一致。 */
    irq_status = ws63_encoder_irq_lock();
    delta_count = g_ws63_encoder_window_count;
    g_ws63_encoder_window_count = 0;
    ws63_encoder_irq_unlock(irq_status);

    g_ws63_encoder_last_delta = delta_count;
    g_ws63_encoder_last_sample_ms = now_ms;

    if ((WS63_ENCODER_PPR == 0U) || (elapsed_ms == 0U)) {
        g_ws63_encoder_rpm = 0;
        return;
    }

    /* RPM = delta_count * 60 * 1000 / (PPR * sample_ms)。 */
    numerator = (int64_t)delta_count * 60000LL;
    denominator = (int64_t)WS63_ENCODER_PPR * (int64_t)elapsed_ms;
    if (denominator == 0) {
        g_ws63_encoder_rpm = 0;
        return;
    }

    g_ws63_encoder_rpm = (int32_t)(numerator / denominator);
}

/**
 * @brief 获取最新 RPM。
 */
int32_t ws63_encoder_get_rpm(void)
{
    int32_t rpm;
    unsigned int irq_status;

    irq_status = ws63_encoder_irq_lock();
    rpm = g_ws63_encoder_rpm;
    ws63_encoder_irq_unlock(irq_status);
    return rpm;
}

/**
 * @brief 获取上一个采样窗口的增量计数。
 */
int32_t ws63_encoder_get_last_delta(void)
{
    int32_t delta;
    unsigned int irq_status;

    irq_status = ws63_encoder_irq_lock();
    delta = g_ws63_encoder_last_delta;
    ws63_encoder_irq_unlock(irq_status);
    return delta;
}

/**
 * @brief 获取累计计数值。
 */
int32_t ws63_encoder_get_total_count(void)
{
    int32_t total;
    unsigned int irq_status;

    irq_status = ws63_encoder_irq_lock();
    total = g_ws63_encoder_total_count;
    ws63_encoder_irq_unlock(irq_status);
    return total;
}

/**
 * @brief 清零测速与计数状态。
 */
void ws63_encoder_reset(void)
{
    unsigned int irq_status;

    irq_status = ws63_encoder_irq_lock();
    g_ws63_encoder_total_count = 0;
    g_ws63_encoder_window_count = 0;
    g_ws63_encoder_last_delta = 0;
    g_ws63_encoder_rpm = 0;
    g_ws63_encoder_last_sample_ms = 0;
    ws63_encoder_irq_unlock(irq_status);
}
