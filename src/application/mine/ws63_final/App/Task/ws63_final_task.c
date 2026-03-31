/**
 * @file ws63_final_task.c
 * @brief WK2114 最终版应用任务层实现。
 */

#include "ws63_final_task.h"

#include "osal_debug.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"
#include "ws63_final_osal.h"

#define WS63_SUBPORT_MAX 4U

/* 各子串口回调表，下标与子串口号一一对应（0 号位保留不用）。 */
static ws63_rx_callback_t g_ws63_rx_cb[WS63_SUBPORT_MAX + 1U] = {0};
static uint32_t g_ws63_last_log_ms[WS63_SUBPORT_MAX + 1U] = {0};

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
        if (sub_port == ZW101_SUBPORT) {
            if (zw101_init(sub_port) == ERRCODE_SUCC) {
                g_ws63_rx_cb[sub_port] = zw101_process_data;
            } else {
                osal_printk("[wk2114 final task] ZW101 init fail\r\n");
            }
        } else if (sub_port == LD2402_SUBPORT) {
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
        }
    }
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
 * @brief WK2114 最终版业务任务入口。
 */
void *ws63_task_entry(const char *arg)
{
    ws63_link_status_t status = {0};

    (void)arg;
    ws63_os_sleep_ms(WS63_BOOT_DELAY_MS);

    if (wk2114_init() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] driver init fail\r\n");
        return NULL;
    }

    wk2114_get_link_status(&status);
    osal_printk("[wk2114 final task] link matched=%u gena=0x%02x\r\n",
        (unsigned int)status.matched, (unsigned int)status.last_gena);

    if (ws63_init_enabled_subports() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] subport init fail\r\n");
        return NULL;
    }

    while (1) {
        ws63_poll_and_dispatch();
        ws63_os_sleep_ms(WS63_TASK_POLL_MS);
    }

    return NULL;
}
