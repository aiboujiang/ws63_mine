/**
 * @file ws63_final_task.c
 * @brief WK2114 最终版应用任务层实现。
 */

#include "ws63_final_task.h"

#include "osal_debug.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "ws63_final_bsp.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"
#include "ws63_motor.h"
#include "ws63_encoder.h"
#if (WS63_RGB_ENABLE == 1U)
#include "ws63_rgb_ws2812.h"
#endif
#include "ws63_final_osal.h"
#include "ws63_final_sle.h"
#include "ws63_final_task_debug.h"
#include "watchdog.h"

#define WS63_SUBPORT_MAX 4U

/* 各子串口回调表，下标与子串口号一一对应（0 号位保留不用）。 */
static ws63_rx_callback_t g_ws63_rx_cb[WS63_SUBPORT_MAX + 1U] = {0};
static uint32_t g_ws63_last_log_ms[WS63_SUBPORT_MAX + 1U] = {0};
static uint8_t g_ws63_motor_encoder_ready = 0U;

#if (WS63_RGB_ENABLE == 1U)
/* RGB 演示颜色表：固定红绿蓝循环。 */
static const ws63_rgb_color_t g_ws63_rgb_demo_colors[] = {
    {255U, 0U, 0U},
    {0U, 255U, 0U},
    {0U, 0U, 255U}
};

/* RGB 任务状态，避免初始化失败后高频重复打日志。 */
static uint8_t g_ws63_rgb_ready = 0U;
static uint8_t g_ws63_rgb_color_index = 0U;
static uint32_t g_ws63_rgb_last_switch_ms = 0U;
#endif

/**
 * @brief 处理 SLE 下行数据：按模块开关分发到对应子口。
 *
 * 设计说明：
 * 1) 只向“已启用模块 + 已启用子口”发送，避免误写其它业务口；
 * 2) 使用驱动层统一写接口，应用层不触碰寄存器与底层 UART 细节。
 */
#if (WS63_SLE_CORE_ENABLE == 1U)
static errcode_t ws63_sle_downlink_handler(const uint8_t *data, uint16_t len)
{
    uint8_t sent_count = 0U;

    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

#if (WS63_SLE_LD2402_ENABLE == 1U)
    if (ws63_is_subport_enabled(WS63_SLE_LD2402_SUBPORT)) {
        if (wk2114_subport_write(WS63_SLE_LD2402_SUBPORT, data, len) == ERRCODE_SUCC) {
            sent_count++;
        }
    }
#endif

#if (WS63_SLE_ZW101_ENABLE == 1U)
    if (ws63_is_subport_enabled(WS63_SLE_ZW101_SUBPORT)) {
        if (wk2114_subport_write(WS63_SLE_ZW101_SUBPORT, data, len) == ERRCODE_SUCC) {
            sent_count++;
        }
    }
#endif

#if (WS63_SLE_CAMERA_ENABLE == 1U)
    if (ws63_is_subport_enabled(WS63_SLE_CAMERA_SUBPORT)) {
        if (wk2114_subport_write(WS63_SLE_CAMERA_SUBPORT, data, len) == ERRCODE_SUCC) {
            sent_count++;
        }
    }
#endif

    if (sent_count == 0U) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}
#endif

/**
 * @brief 初始化 SLE 从机桥接。
 */
static void ws63_sle_bridge_init(void)
{
#if (WS63_SLE_CORE_ENABLE == 1U)
    errcode_t ret;

    ret = ws63_sle_init(ws63_sle_downlink_handler);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] sle bridge init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    osal_printk("[wk2114 final task] sle bridge init ok\r\n");
#endif
}

/**
 * @brief 默认接收回调。
 *
 * 作用：
 * 1) 在你还未接入具体业务模块时，先给出链路活性日志；
 * 2) 输出做节流，防止高频数据淹没调试口。
 */
static void ws63_default_rx_callback(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint32_t now_ms;

    if ((sub_port > WS63_SUBPORT_MAX) || (data == NULL) || (len == 0U)) {
        return;
    }

    now_ms = ws63_os_tick_ms();
    if ((now_ms - g_ws63_last_log_ms[sub_port]) < WS63_LOG_GAP_MS) {
        return;
    }

    g_ws63_last_log_ms[sub_port] = now_ms;
    osal_printk("[wk2114 final task] U%u rx len=%u first=0x%02x\r\n",
        (unsigned int)sub_port,
        (unsigned int)len,
        (unsigned int)data[0]);
}

/**
 * @brief 按配置初始化所有启用的子串口。
 */
static errcode_t ws63_init_enabled_subports(void)
{
    uint8_t sub_port;
    errcode_t ret;

    for (sub_port = 1U; sub_port <= WS63_SUBPORT_MAX; sub_port++) {
        if (!ws63_is_subport_enabled(sub_port)) {
            continue;
        }

        ret = wk2114_subport_init(sub_port,
            ws63_get_subport_baud(sub_port));
        if (ret != ERRCODE_SUCC) {
            osal_printk("[wk2114 final task] sub-uart%u init fail\r\n", (unsigned int)sub_port);
            return ret;
        }

        /* 针对不同外设进行初始化和回调绑定 */
        if ((sub_port == ZW101_SUBPORT) && (WS63_SLE_ZW101_ENABLE == 1U)) {
            if (zw101_init(sub_port) == ERRCODE_SUCC) {
                g_ws63_rx_cb[sub_port] = zw101_process_data;
            } else {
                osal_printk("[wk2114 final task] ZW101 init fail\r\n");
            }
        } else if ((sub_port == LD2402_SUBPORT) && (WS63_SLE_LD2402_ENABLE == 1U)) {
            if (ld2402_init(sub_port) == ERRCODE_SUCC) {
                g_ws63_rx_cb[sub_port] = ld2402_process_data;
            } else {
                osal_printk("[wk2114 final task] LD2402 init fail\r\n");
            }
        }

        if (g_ws63_rx_cb[sub_port] == NULL) {
            g_ws63_rx_cb[sub_port] = ws63_default_rx_callback;
        }
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 轮询并分发子串口接收数据。
 */
static void ws63_poll_and_dispatch(void)
{
    uint8_t sub_port;
    uint8_t len;
    uint8_t rx_buf[WS63_FIFO_CHUNK_MAX];

    for (sub_port = 1U; sub_port <= WS63_SUBPORT_MAX; sub_port++) {
        if (!ws63_is_subport_enabled(sub_port)) {
            continue;
        }

        len = wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf));
        if ((len > 0U) && (g_ws63_rx_cb[sub_port] != NULL)) {
            g_ws63_rx_cb[sub_port](sub_port, rx_buf, len);

#if (WS63_SLE_CORE_ENABLE == 1U)
            /* 上行到 SLE 的模块由中间件按子口映射和模块开关再次过滤。 */
            (void)ws63_sle_send_subport_data(sub_port, rx_buf, len);
#endif
        }
    }
}

/**
 * @brief 初始化 RGB 演示链路。
 */
static void ws63_rgb_demo_init(void)
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
    g_ws63_rgb_last_switch_ms = ws63_os_tick_ms() - WS63_RGB_DEMO_INTERVAL_MS;
    osal_printk("[wk2114 final task] rgb demo start (SPI1/GPIO1+GPIO6)\r\n");
#endif
}

/**
 * @brief 周期驱动 RGB 演示（红绿蓝循环）。
 */
static void ws63_rgb_demo_process(uint32_t now_ms)
{
#if (WS63_RGB_ENABLE == 1U)
    errcode_t ret;

    if (g_ws63_rgb_ready == 0U) {
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
 * @brief 初始化电机与编码器能力。
 */
static void ws63_motor_encoder_init(void)
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
 * @brief 注册子串口回调。
 */
errcode_t ws63_task_register_rx_callback(uint8_t sub_port,
    ws63_rx_callback_t callback)
{
    if (!ws63_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }

    g_ws63_rx_cb[sub_port] = callback;
    return ERRCODE_SUCC;
}

/**
 * @brief 通过子串口发送数据。
 */
errcode_t ws63_task_send(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    return wk2114_subport_write(sub_port, data, len);
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
 * @brief WK2114 最终版业务任务入口。
 */
void *ws63_task_entry(const char *arg)
{
    /* 链路状态类型由驱动层定义，应用层直接复用统一结构。 */
    wk2114_link_status_t status = {0};
    bool wk2114_ready = false;
    errcode_t wdt_ret;

    (void)arg;
    ws63_os_sleep_ms(WS63_BOOT_DELAY_MS);

    /* 电机/编码器初始化独立于 WK2114 链路，失败仅记录日志，不阻断任务。 */
    ws63_motor_encoder_init();

    /* 在线调试串口：用于电机命令控测与状态日志输出。 */
    ws63_task_debug_init();

    /* 先启动 SLE 桥接：即使 WK2114 链路异常，也能保留无线侧诊断日志。 */
    ws63_sle_bridge_init();

    if (wk2114_init() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] driver init fail, keep sle alive\r\n");
    } else {
        wk2114_ready = true;
    }

    if (wk2114_ready) {
        ws63_os_sleep_ms(10U);

        wk2114_get_link_status(&status);
        osal_printk("[wk2114 final task] link matched=%u gena=0x%02x\r\n",
            (unsigned int)status.matched, (unsigned int)status.last_gena);

        if (ws63_init_enabled_subports() != ERRCODE_SUCC) {
            osal_printk("[wk2114 final task] subport init fail\r\n");
            wk2114_ready = false;
        }

        if (wk2114_ready) {
            ws63_rgb_demo_init();
        }
    }

    while (1) {
        uint32_t now_ms;

        if (wk2114_ready) {
            ws63_poll_and_dispatch();
        }
        now_ms = ws63_os_tick_ms();
        if (g_ws63_motor_encoder_ready == 1U) {
            ws63_encoder_sample(now_ms);
        }
        ws63_task_debug_process(now_ms);
        ws63_rgb_demo_process(now_ms);

#if (WS63_SLE_CORE_ENABLE == 1U)
        ws63_sle_process();
#endif

        /* 与 RGB 演示并行时持续喂狗，避免任务轮询窗口过长触发复位。 */
        wdt_ret = uapi_watchdog_kick();
        if (wdt_ret != ERRCODE_SUCC) {
#if (WS63_RGB_LOG_ENABLE == 1U)
            osal_printk("[wk2114 final task] watchdog kick fail, ret=0x%x\r\n", (unsigned int)wdt_ret);
#endif
        }

        ws63_os_sleep_ms(WS63_TASK_POLL_MS);
    }

    return NULL;
}
