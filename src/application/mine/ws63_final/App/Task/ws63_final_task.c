/**
 * @file ws63_final_task.c
 * @brief WK2114 最终版应用任务层实现。
 */

#include "ws63_final_task.h"

#include "osal_debug.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "ws63_final_driver.h"
#include "ws63_final_osal.h"

#define MINE_WS63_FINAL_SUBPORT_MAX 4U

/* 各子串口回调表，下标与子串口号一一对应（0 号位保留不用）。 */
static mine_ws63_final_rx_callback_t g_mine_ws63_final_rx_cb[MINE_WS63_FINAL_SUBPORT_MAX + 1U] = {0};
static uint32_t g_mine_ws63_final_last_log_ms[MINE_WS63_FINAL_SUBPORT_MAX + 1U] = {0};

/**
 * @brief 默认接收回调。
 *
 * 作用：
 * 1) 在你还未接入具体业务模块时，先给出链路活性日志；
 * 2) 输出做节流，防止高频数据淹没调试口。
 */
static void mine_ws63_final_default_rx_callback(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint32_t now_ms;

    if ((sub_port > MINE_WS63_FINAL_SUBPORT_MAX) || (data == NULL) || (len == 0U)) {
        return;
    }

    now_ms = mine_ws63_final_os_tick_ms();
    if ((now_ms - g_mine_ws63_final_last_log_ms[sub_port]) < MINE_WS63_FINAL_LOG_GAP_MS) {
        return;
    }

    g_mine_ws63_final_last_log_ms[sub_port] = now_ms;
    osal_printk("[wk2114 final task] U%u rx len=%u first=0x%02x\r\n",
        (unsigned int)sub_port,
        (unsigned int)len,
        (unsigned int)data[0]);
}

/**
 * @brief 按配置初始化所有启用的子串口。
 */
static errcode_t mine_ws63_final_init_enabled_subports(void)
{
    uint8_t sub_port;
    errcode_t ret;

    for (sub_port = 1U; sub_port <= MINE_WS63_FINAL_SUBPORT_MAX; sub_port++) {
        if (!mine_ws63_final_is_subport_enabled(sub_port)) {
            continue;
        }

        ret = mine_ws63_final_driver_subport_init(sub_port,
            mine_ws63_final_get_subport_baud(sub_port));
        if (ret != ERRCODE_SUCC) {
            osal_printk("[wk2114 final task] sub-uart%u init fail\r\n", (unsigned int)sub_port);
            return ret;
        }

        if (g_mine_ws63_final_rx_cb[sub_port] == NULL) {
            g_mine_ws63_final_rx_cb[sub_port] = mine_ws63_final_default_rx_callback;
        }
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 轮询并分发子串口接收数据。
 */
static void mine_ws63_final_poll_and_dispatch(void)
{
    uint8_t sub_port;
    uint8_t len;
    uint8_t rx_buf[MINE_WS63_FINAL_FIFO_CHUNK_MAX];

    for (sub_port = 1U; sub_port <= MINE_WS63_FINAL_SUBPORT_MAX; sub_port++) {
        if (!mine_ws63_final_is_subport_enabled(sub_port)) {
            continue;
        }

        len = mine_ws63_final_driver_subport_read(sub_port, rx_buf, sizeof(rx_buf));
        if ((len > 0U) && (g_mine_ws63_final_rx_cb[sub_port] != NULL)) {
            g_mine_ws63_final_rx_cb[sub_port](sub_port, rx_buf, len);
        }
    }
}

/**
 * @brief 注册子串口回调。
 */
errcode_t mine_ws63_final_task_register_rx_callback(uint8_t sub_port,
    mine_ws63_final_rx_callback_t callback)
{
    if (!mine_ws63_final_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }

    g_mine_ws63_final_rx_cb[sub_port] = callback;
    return ERRCODE_SUCC;
}

/**
 * @brief 通过子串口发送数据。
 */
errcode_t mine_ws63_final_task_send(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    return mine_ws63_final_driver_subport_write(sub_port, data, len);
}

/**
 * @brief WK2114 最终版业务任务入口。
 */
void *mine_ws63_final_task_entry(const char *arg)
{
    mine_ws63_final_link_status_t status = {0};

    (void)arg;
    mine_ws63_final_os_sleep_ms(MINE_WS63_FINAL_BOOT_DELAY_MS);

    if (mine_ws63_final_driver_init() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] driver init fail\r\n");
        return NULL;
    }

    mine_ws63_final_driver_get_link_status(&status);
    osal_printk("[wk2114 final task] link matched=%u gena=0x%02x\r\n",
        (unsigned int)status.matched, (unsigned int)status.last_gena);

    if (mine_ws63_final_init_enabled_subports() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] subport init fail\r\n");
        return NULL;
    }

    while (1) {
        mine_ws63_final_poll_and_dispatch();
        mine_ws63_final_os_sleep_ms(MINE_WS63_FINAL_TASK_POLL_MS);
    }

    return NULL;
}
