/**
 * @file ws63_final_task_rgb.c
 * @brief Task 层 RGB 演示子模块实现。
 */

#include "ws63_final_task_internal.h"

#include "osal_debug.h"

#include "ws63_final_config.h"
#if (WS63_RGB_ENABLE == 1U)
#include "ws63_rgb_ws2812.h"
#endif
#include "ws63_final_osal.h"

#if (WS63_RGB_ENABLE == 1U)
/* RGB 演示颜色表：固定红绿蓝循环。 */
static const ws63_rgb_color_t g_ws63_rgb_demo_colors[] = {
    {255U, 0U, 0U},
    {0U, 255U, 0U},
    {0U, 0U, 255U}
};

/* RGB 子模块状态：避免初始化失败后持续刷日志。 */
static uint8_t g_ws63_rgb_ready = 0U;
static uint8_t g_ws63_rgb_color_index = 0U;
static uint32_t g_ws63_rgb_last_switch_ms = 0U;
/* 演示模式开关：命令手动设色时会自动关闭，避免颜色被周期任务覆盖。 */
static uint8_t g_ws63_rgb_demo_enable = 0U;
#endif

/**
 * @brief 初始化 RGB 演示链路。
 */
void ws63_rgb_demo_init(void)
{
#if (WS63_RGB_ENABLE == 1U)
    errcode_t ret;

    ret = ws63_rgb_ws2812_init();
    if (ret != ERRCODE_SUCC) {
        g_ws63_rgb_ready = 0U;
        osal_printk("[wk2114 final task] rgb init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_rgb_ready = 1U;
    g_ws63_rgb_demo_enable = 1U;
    g_ws63_rgb_color_index = 0U;
    g_ws63_rgb_last_switch_ms = ws63_os_tick_ms() - WS63_RGB_DEMO_INTERVAL_MS;
    osal_printk("[wk2114 final task] rgb demo start (SPI1/GPIO1+GPIO6)\r\n");
#endif
}

/**
 * @brief 周期驱动 RGB 演示（红绿蓝循环）。
 */
void ws63_rgb_demo_process(uint32_t now_ms)
{
#if (WS63_RGB_ENABLE == 1U)
    errcode_t ret;

    if (g_ws63_rgb_ready == 0U) {
        return;
    }

    if (g_ws63_rgb_demo_enable == 0U) {
        return;
    }

    if ((now_ms - g_ws63_rgb_last_switch_ms) < WS63_RGB_DEMO_INTERVAL_MS) {
        return;
    }

    g_ws63_rgb_last_switch_ms = now_ms;
    ret = ws63_rgb_ws2812_set_color(&g_ws63_rgb_demo_colors[g_ws63_rgb_color_index]);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] rgb send fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_rgb_color_index++;
    if (g_ws63_rgb_color_index >= (uint8_t)(sizeof(g_ws63_rgb_demo_colors) / sizeof(g_ws63_rgb_demo_colors[0]))) {
        g_ws63_rgb_color_index = 0U;
    }
#else
    (void)now_ms;
#endif
}

/**
 * @brief 重新初始化 RGB 驱动并恢复演示模式。
 */
errcode_t ws63_task_rgb_reinit(void)
{
#if (WS63_RGB_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    ws63_rgb_demo_init();
    return (g_ws63_rgb_ready == 1U) ? ERRCODE_SUCC : ERRCODE_FAIL;
#endif
}

/**
 * @brief 设置 RGB 颜色，并关闭演示模式避免被周期轮询覆盖。
 */
errcode_t ws63_task_rgb_set_color(uint8_t r, uint8_t g, uint8_t b)
{
#if (WS63_RGB_ENABLE != 1U)
    (void)r;
    (void)g;
    (void)b;
    return ERRCODE_FAIL;
#else
    errcode_t ret;
    ws63_rgb_color_t color = {r, g, b};

    if (g_ws63_rgb_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ret = ws63_rgb_ws2812_set_color(&color);
    if (ret == ERRCODE_SUCC) {
        g_ws63_rgb_demo_enable = 0U;
    }
    return ret;
#endif
}

/**
 * @brief 关闭 RGB（输出黑色）。
 */
errcode_t ws63_task_rgb_off(void)
{
    return ws63_task_rgb_set_color(0U, 0U, 0U);
}

/**
 * @brief 设置 RGB 演示模式开关。
 */
errcode_t ws63_task_rgb_set_demo_enable(uint8_t enable)
{
#if (WS63_RGB_ENABLE != 1U)
    (void)enable;
    return ERRCODE_FAIL;
#else
    if (g_ws63_rgb_ready == 0U) {
        return ERRCODE_FAIL;
    }

    g_ws63_rgb_demo_enable = (enable != 0U) ? 1U : 0U;
    if (g_ws63_rgb_demo_enable == 1U) {
        /* 开启演示后立即生效，避免等待一个完整周期。 */
        g_ws63_rgb_last_switch_ms = ws63_os_tick_ms() - WS63_RGB_DEMO_INTERVAL_MS;
    }
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 查询 RGB 驱动是否已就绪。
 */
uint8_t ws63_task_rgb_is_ready(void)
{
#if (WS63_RGB_ENABLE != 1U)
    return 0U;
#else
    return g_ws63_rgb_ready;
#endif
}

/**
 * @brief 查询 RGB 演示模式是否开启。
 */
uint8_t ws63_task_rgb_is_demo_enable(void)
{
#if (WS63_RGB_ENABLE != 1U)
    return 0U;
#else
    return g_ws63_rgb_demo_enable;
#endif
}
