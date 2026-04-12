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

/* RGB 控制队列与任务状态。 */
static unsigned long g_ws63_rgb_ctrl_queue = 0UL;
static uint8_t g_ws63_rgb_task_started = 0U;
#endif

/**
 * @brief 统一发送 RGB 控制消息。
 */
static errcode_t ws63_rgb_post_ctrl_msg(const ws63_rgb_ctrl_msg_t *msg, uint32_t timeout)
{
#if (WS63_RGB_ENABLE != 1U)
    (void)msg;
    (void)timeout;
    return ERRCODE_FAIL;
#else
    if ((msg == NULL) || (g_ws63_rgb_ctrl_queue == 0UL)) {
        return ERRCODE_FAIL;
    }

    return ws63_os_msg_queue_send(g_ws63_rgb_ctrl_queue,
        msg,
        (uint16_t)sizeof(ws63_rgb_ctrl_msg_t),
        timeout);
#endif
}

/**
 * @brief 初始化 RGB 硬件链路，并按默认策略决定是否进入演示。
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
    g_ws63_rgb_color_index = 0U;
    g_ws63_rgb_demo_enable = (WS63_RGB_DEMO_ENABLE_DEFAULT != 0U) ? 1U : 0U;
    if (g_ws63_rgb_demo_enable == 1U) {
        /* 默认开启演示时，先把时间戳回拨一个周期，保证上电后立即出第一帧。 */
        g_ws63_rgb_last_switch_ms = ws63_os_tick_ms() - WS63_RGB_DEMO_INTERVAL_MS;
        osal_printk("[wk2114 final task] rgb demo start (SPI1/GPIO1+GPIO6)\r\n");
    } else {
        osal_printk("[wk2114 final task] rgb init ok, demo disabled\r\n");
    }
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
 * @brief 执行一条 RGB 控制命令。
 */
static void ws63_rgb_process_ctrl_msg(const ws63_rgb_ctrl_msg_t *msg)
{
#if (WS63_RGB_ENABLE == 1U)
    errcode_t ret;
    ws63_rgb_color_t color;

    if (msg == NULL) {
        return;
    }

    switch (msg->type) {
        case WS63_RGB_CMD_REINIT:
            ws63_rgb_demo_init();
            break;
        case WS63_RGB_CMD_SET_COLOR:
            if (g_ws63_rgb_ready == 0U) {
                break;
            }
            color.r = msg->r;
            color.g = msg->g;
            color.b = msg->b;
            ret = ws63_rgb_ws2812_set_color(&color);
            if (ret == ERRCODE_SUCC) {
                g_ws63_rgb_demo_enable = 0U;
            } else {
                osal_printk("[wk2114 final task] rgb set fail, ret=0x%x\r\n", (unsigned int)ret);
            }
            break;
        case WS63_RGB_CMD_SET_DEMO:
            if (g_ws63_rgb_ready == 0U) {
                break;
            }
            g_ws63_rgb_demo_enable = (msg->enable != 0U) ? 1U : 0U;
            if (g_ws63_rgb_demo_enable == 1U) {
                /* 开启演示后立即触发一次颜色切换。 */
                g_ws63_rgb_last_switch_ms = ws63_os_tick_ms() - WS63_RGB_DEMO_INTERVAL_MS;
            }
            break;
        case WS63_RGB_CMD_OFF:
            if (g_ws63_rgb_ready == 0U) {
                break;
            }
            color.r = 0U;
            color.g = 0U;
            color.b = 0U;
            ret = ws63_rgb_ws2812_set_color(&color);
            if (ret == ERRCODE_SUCC) {
                g_ws63_rgb_demo_enable = 0U;
            }
            break;
        default:
            break;
    }
#else
    (void)msg;
#endif
}

/**
 * @brief RGB 独立任务入口：串行处理控制命令并驱动演示周期。
 */
static void *ws63_rgb_task_entry(const char *arg)
{
#if (WS63_RGB_ENABLE == 1U)
    ws63_rgb_ctrl_msg_t msg;

    (void)arg;
    /* 任务启动后只做一次硬件初始化，演示模式是否运行由默认策略和调试命令决定。 */
    ws63_rgb_demo_init();

    while (1) {
        uint32_t size;
        uint32_t now_ms;

        /* 每个周期尽量清空控制队列，保证命令响应实时性。 */
        while (1) {
            size = (uint32_t)sizeof(msg);
            if (ws63_os_msg_queue_recv(g_ws63_rgb_ctrl_queue, &msg, &size, WS63_OS_NO_WAIT) != ERRCODE_SUCC) {
                break;
            }
            ws63_rgb_process_ctrl_msg(&msg);
        }

        now_ms = ws63_os_tick_ms();
        ws63_rgb_demo_process(now_ms);
        ws63_os_sleep_ms(WS63_RGB_TASK_POLL_MS);
    }
#else
    (void)arg;
    while (1) {
        ws63_os_sleep_ms(1000U);
    }
#endif

    return NULL;
}

/**
 * @brief 启动 RGB 独立任务。
 */
errcode_t ws63_rgb_task_start(void)
{
#if (WS63_RGB_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    if (g_ws63_rgb_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_rgb_q",
        WS63_RGB_CTRL_QUEUE_DEPTH,
        (uint16_t)sizeof(ws63_rgb_ctrl_msg_t),
        &g_ws63_rgb_ctrl_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] rgb queue create fail\r\n");
        return ret;
    }

    ret = ws63_os_start_task("ws63_rgb_task",
        ws63_rgb_task_entry,
        0U,
        WS63_RGB_TASK_STACK_SIZE,
        WS63_RGB_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_rgb_ctrl_queue);
        g_ws63_rgb_ctrl_queue = 0UL;
        osal_printk("[wk2114 final task] rgb task start fail\r\n");
        return ret;
    }

    g_ws63_rgb_task_started = 1U;
    osal_printk("[wk2114 final task] rgb task start ok\r\n");
    return ERRCODE_SUCC;
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
    ws63_rgb_ctrl_msg_t msg = {
        .type = WS63_RGB_CMD_REINIT,
        .r = 0U,
        .g = 0U,
        .b = 0U,
        .enable = 0U
    };

    if (g_ws63_rgb_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_rgb_post_ctrl_msg(&msg, WS63_OS_NO_WAIT);
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
    ws63_rgb_ctrl_msg_t msg = {
        .type = WS63_RGB_CMD_SET_COLOR,
        .r = r,
        .g = g,
        .b = b,
        .enable = 0U
    };

    if ((g_ws63_rgb_task_started == 0U) || (g_ws63_rgb_ready == 0U)) {
        return ERRCODE_FAIL;
    }

    return ws63_rgb_post_ctrl_msg(&msg, WS63_OS_NO_WAIT);
#endif
}

/**
 * @brief 关闭 RGB（输出黑色）。
 */
errcode_t ws63_task_rgb_off(void)
{
#if (WS63_RGB_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    ws63_rgb_ctrl_msg_t msg = {
        .type = WS63_RGB_CMD_OFF,
        .r = 0U,
        .g = 0U,
        .b = 0U,
        .enable = 0U
    };

    if ((g_ws63_rgb_task_started == 0U) || (g_ws63_rgb_ready == 0U)) {
        return ERRCODE_FAIL;
    }

    return ws63_rgb_post_ctrl_msg(&msg, WS63_OS_NO_WAIT);
#endif
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
    ws63_rgb_ctrl_msg_t msg = {
        .type = WS63_RGB_CMD_SET_DEMO,
        .r = 0U,
        .g = 0U,
        .b = 0U,
        .enable = enable
    };

    if ((g_ws63_rgb_task_started == 0U) || (g_ws63_rgb_ready == 0U)) {
        return ERRCODE_FAIL;
    }

    return ws63_rgb_post_ctrl_msg(&msg, WS63_OS_NO_WAIT);
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
