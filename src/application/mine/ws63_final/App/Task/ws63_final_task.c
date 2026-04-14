/**
 * @file ws63_final_task.c
 * @brief WK2114 最终版应用任务层实现（RTOS 多任务拆分）。
 */

#include "ws63_final_task_internal.h"

#include <stdbool.h>
#include <stddef.h>

#include "osal_debug.h"
#include "securec.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"
#include "ws63_encoder.h"
#include "ws63_final_osal.h"
#include "ws63_final_sle.h"
#include "ws63_final_task_debug.h"

/* 各子串口回调表，下标与子串口号一一对应（0 号位保留不用）。 */
static ws63_rx_callback_t g_ws63_rx_cb[WS63_SUBPORT_MAX + 1U] = {0};
static uint32_t g_ws63_last_log_ms[WS63_SUBPORT_MAX + 1U] = {0};

/* RTOS 队列：WK2114 发送请求与 SLE 上行消息。 */
static unsigned long g_ws63_wk2114_tx_queue = 0UL;
static unsigned long g_ws63_sle_uplink_queue = 0UL;

/* ZW101 上行只保留短预览，避免把二进制原文直接灌到主机串口。 */
#define WS63_TASK_ZW101_HEX_PREVIEW_BYTES 16U

/* 任务状态：用于 API 快速判断是否可投递请求。 */
static uint8_t g_ws63_wk2114_ready = 0U;
static uint8_t g_ws63_wk2114_task_started = 0U;
static uint8_t g_ws63_sle_task_started = 0U;
static uint8_t g_ws63_subport_inited[WS63_SUBPORT_MAX + 1U] = {0};

/**
 * @brief 将二进制缓冲区压缩为短 Hex 预览文本。
 *
 * 说明：主机侧只需要看出帧头和前几个字节是否正常即可，完整原文会直接
 * 挤占串口输出并造成乱码，因此这里只保留一个可读预览。
 *
 * @param data 原始二进制数据。
 * @param len  原始数据长度。
 * @param text 预览输出缓冲区。
 * @param text_len 预览输出缓冲区长度。
 * @return uint16_t 实际写入长度，失败返回 0。
 */
static uint16_t ws63_task_build_hex_preview(const uint8_t *data, uint16_t len, char *text, uint16_t text_len)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    uint16_t i;
    uint16_t preview_len;
    uint16_t pos;

    if ((data == NULL) || (text == NULL) || (text_len <= 6U) || (len == 0U)) {
        return 0U;
    }

    preview_len = (len > WS63_TASK_ZW101_HEX_PREVIEW_BYTES) ? WS63_TASK_ZW101_HEX_PREVIEW_BYTES : len;

    /* 先放固定前缀，再按字节展开成可读的十六进制文本。 */
    if (memcpy_s(text, text_len, "data=", sizeof("data=") - 1U) != EOK) {
        return 0U;
    }

    pos = (uint16_t)(sizeof("data=") - 1U);
    for (i = 0U; i < preview_len; i++) {
        if ((uint32_t)pos + 3U >= text_len) {
            break;
        }

        text[pos++] = hex_digits[(data[i] >> 4U) & 0x0FU];
        text[pos++] = hex_digits[data[i] & 0x0FU];
        if ((uint16_t)(i + 1U) < preview_len) {
            text[pos++] = ' ';
        }
    }

    /* 预览不追求完整性，只要主机能看出还有未展开的数据即可。 */
    if ((len > preview_len) && ((uint32_t)pos + 4U < text_len)) {
        text[pos++] = ' ';
        text[pos++] = '.';
        text[pos++] = '.';
        text[pos++] = '.';
    }

    text[pos] = '\0';
    return pos;
}

/**
 * @brief 设置 WK2114 链路就绪状态。
 */
static void ws63_set_wk2114_ready(uint8_t ready)
{
    unsigned int irq_status;

    irq_status = ws63_os_irq_lock();
    g_ws63_wk2114_ready = (ready != 0U) ? 1U : 0U;
    ws63_os_irq_unlock(irq_status);
}

/**
 * @brief 获取 WK2114 链路就绪状态。
 */
uint8_t ws63_task_wk2114_is_ready(void)
{
    unsigned int irq_status;
    uint8_t ready;

    irq_status = ws63_os_irq_lock();
    ready = g_ws63_wk2114_ready;
    ws63_os_irq_unlock(irq_status);
    return ready;
}

/**
 * @brief 向 WK2114 发送队列投递消息。
 */
errcode_t ws63_task_post_wk2114_tx(const ws63_wk2114_tx_msg_t *msg, uint32_t timeout)
{
    if ((msg == NULL) || (g_ws63_wk2114_tx_queue == 0UL)) {
        return ERRCODE_FAIL;
    }

    if ((msg->sub_port == 0U) || (msg->sub_port > WS63_SUBPORT_MAX) || (msg->len == 0U) ||
        (msg->len > WS63_TASK_QUEUE_PAYLOAD_MAX)) {
        return ERRCODE_INVALID_PARAM;
    }

    return ws63_os_msg_queue_send(g_ws63_wk2114_tx_queue,
        msg,
        (uint16_t)sizeof(ws63_wk2114_tx_msg_t),
        timeout);
}

/**
 * @brief 从 WK2114 发送队列读取消息。
 */
errcode_t ws63_task_recv_wk2114_tx(ws63_wk2114_tx_msg_t *msg, uint32_t timeout)
{
    uint32_t size;

    if ((msg == NULL) || (g_ws63_wk2114_tx_queue == 0UL)) {
        return ERRCODE_FAIL;
    }

    size = (uint32_t)sizeof(ws63_wk2114_tx_msg_t);
    return ws63_os_msg_queue_recv(g_ws63_wk2114_tx_queue, msg, &size, timeout);
}

/**
 * @brief 向 SLE 上行队列投递消息。
 */
errcode_t ws63_task_post_sle_uplink(const ws63_sle_uplink_msg_t *msg, uint32_t timeout)
{
    if ((msg == NULL) || (g_ws63_sle_uplink_queue == 0UL)) {
        return ERRCODE_FAIL;
    }

    if (ws63_task_debug_is_debug_only_mode() == 0U) {
        /*
         * 未进入 DEBUG INIT 时，设备侧不再向主机上报运行日志，
         * 这样主机串口不会看到 LD2402 / ZW101 这类常规巡航输出。
         */
        return ERRCODE_SUCC;
    }

    if ((msg->sub_port == 0U) || (msg->sub_port > WS63_SUBPORT_MAX) || (msg->len == 0U) ||
        (msg->len > WS63_TASK_QUEUE_PAYLOAD_MAX)) {
        return ERRCODE_INVALID_PARAM;
    }

    /*
     * LD2402 的主机侧可见上行受日志开关控制：关闭时静音，打开时恢复距离行。
     * 这样 `LD LOG OFF/ON` 才能明确控制主机是否看到距离刷屏，同时不影响本地
     * 驱动层的状态处理。
     */
    if (msg->sub_port == LD2402_SUBPORT) {
        if (ws63_task_ld2402_get_log_enable() == 0U) {
            return ERRCODE_SUCC;
        }

        return ws63_sle_send_subport_data(msg->sub_port, msg->data, msg->len);
    }

    /*
     * ZW101 上报仍保留标签，但把原始二进制收敛成 Hex 预览，避免主机终端
     * 直接显示不可读字节流。
     */
    if (msg->sub_port == ZW101_SUBPORT) {
        char preview[WS63_TASK_QUEUE_PAYLOAD_MAX + 1U] = {0};
        uint16_t preview_len;

        preview_len = ws63_task_build_hex_preview(msg->data, msg->len, preview, sizeof(preview));
        if (preview_len == 0U) {
            return ERRCODE_FAIL;
        }

        return ws63_sle_send_subport_data(msg->sub_port, (const uint8_t *)preview, preview_len);
    }

    return ws63_os_msg_queue_send(g_ws63_sle_uplink_queue,
        msg,
        (uint16_t)sizeof(ws63_sle_uplink_msg_t),
        timeout);
}

/**
 * @brief 从 SLE 上行队列读取消息。
 */
errcode_t ws63_task_recv_sle_uplink(ws63_sle_uplink_msg_t *msg, uint32_t timeout)
{
    uint32_t size;

    if ((msg == NULL) || (g_ws63_sle_uplink_queue == 0UL)) {
        return ERRCODE_FAIL;
    }

    size = (uint32_t)sizeof(ws63_sle_uplink_msg_t);
    return ws63_os_msg_queue_recv(g_ws63_sle_uplink_queue, msg, &size, timeout);
}

/**
 * @brief 默认接收回调。
 *
 * 作用：
 * 1) 在还未接入具体业务模块时，先给出链路活性日志；
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
 * @brief 初始化单个子串口并按设备类型绑定回调。
 */
static errcode_t ws63_init_subport_with_device(uint8_t sub_port)
{
    uint32_t sub_baud;
    errcode_t ret;

    if (!ws63_is_subport_enabled(sub_port)) {
        return ERRCODE_FAIL;
    }

    if (!ws63_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (g_ws63_subport_inited[sub_port] != 0U) {
        return ERRCODE_SUCC;
    }

    sub_baud = ws63_get_subport_baud(sub_port);
    ret = wk2114_subport_init(sub_port, sub_baud);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] sub-uart%u init fail\r\n", (unsigned int)sub_port);
        return ret;
    }

    if ((sub_port == ZW101_SUBPORT) && (WS63_SLE_ZW101_ENABLE == 1U)) {
        osal_printk("[wk2114 final task] ZW101 cfg sub-uart%u baud=%u\r\n",
            (unsigned int)sub_port,
            (unsigned int)sub_baud);
        /*
         * 先绑定 ZW101 回调，再执行初始化。
         * 这样即使初始化阶段 worker 线程先读到回包，也会喂入 zw101 缓存，避免 ACK 被默认回调吞掉。
         */
        g_ws63_rx_cb[sub_port] = zw101_process_data;
        if (zw101_init(sub_port) == ERRCODE_SUCC) {
            osal_printk("[wk2114 final task] ZW101 init ok\r\n");
        } else {
            osal_printk("[wk2114 final task] ZW101 init fail\r\n");
        }
    } else if ((sub_port == LD2402_SUBPORT) && (WS63_SLE_LD2402_ENABLE == 1U)) {
        if (ld2402_init(sub_port) == ERRCODE_SUCC) {
            g_ws63_rx_cb[sub_port] = ld2402_process_data;
        } else {
            osal_printk("[wk2114 final task] LD2402 init fail\r\n");
        }
    } else if ((sub_port == WS63_SLE_CAMERA_SUBPORT) && (WS63_CAMERA_ENABLE == 1U)) {
        osal_printk("[wk2114 final task] CAMERA cfg sub-uart%u baud=%u\r\n",
            (unsigned int)sub_port,
            (unsigned int)sub_baud);
        g_ws63_rx_cb[sub_port] = ws63_task_camera_process_data;
    }

    if (g_ws63_rx_cb[sub_port] == NULL) {
        g_ws63_rx_cb[sub_port] = ws63_default_rx_callback;
    }

    g_ws63_subport_inited[sub_port] = 1U;
    return ERRCODE_SUCC;
}

/**
 * @brief 按配置初始化所有启用的子串口。
 */
static errcode_t ws63_init_enabled_subports(void)
{
    /* 首次启动仅拉起 LD2402 子口，降低与 ZW101/camera 的并发冲突概率。 */
    if ((WS63_SLE_LD2402_ENABLE == 1U) && (ws63_is_subport_enabled(LD2402_SUBPORT) != 0U)) {
        return ws63_init_subport_with_device(LD2402_SUBPORT);
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 轮询并分发子串口接收数据，同时转发到 SLE 上行队列。
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
            /*
             * camera 子口上行由 camera 模块在“文本重组完成”后统一投递，
             * 这里不再直通原始分片，避免主机侧看到同一条结果被拆成多条 [CAMERA]。
             */
            if ((sub_port == WS63_SLE_CAMERA_SUBPORT) && (WS63_CAMERA_ENABLE == 1U)) {
                continue;
            }

            /*
             * 上行数据只做“拷贝 + 投递”，由 SLE 任务统一发送。
             * 这样可以避免 WK2114 轮询线程被 SLE 发送路径阻塞。
             */
            ws63_sle_uplink_msg_t uplink_msg;

            uplink_msg.sub_port = sub_port;
            uplink_msg.len = len;
            if (memcpy_s(uplink_msg.data, sizeof(uplink_msg.data), rx_buf, len) == EOK) {
                (void)ws63_task_post_sle_uplink(&uplink_msg, WS63_OS_NO_WAIT);
            }
#endif
        }
    }
}

/**
 * @brief 处理 WK2114 发送队列中的待发数据。
 */
static void ws63_wk2114_process_tx_queue(void)
{
    while (1) {
        ws63_wk2114_tx_msg_t tx_msg;
        errcode_t ret;

        ret = ws63_task_recv_wk2114_tx(&tx_msg, WS63_OS_NO_WAIT);
        if (ret != ERRCODE_SUCC) {
            break;
        }

        if (!ws63_is_subport_enabled(tx_msg.sub_port)) {
            continue;
        }

        (void)wk2114_subport_write(tx_msg.sub_port, tx_msg.data, tx_msg.len);
    }
}

/**
 * @brief 处理 SLE 上行队列中的待发数据。
 */
static void ws63_sle_process_uplink_queue(void)
{
#if (WS63_SLE_CORE_ENABLE == 1U)
    while (1) {
        ws63_sle_uplink_msg_t uplink_msg;

        if (ws63_task_recv_sle_uplink(&uplink_msg, WS63_OS_NO_WAIT) != ERRCODE_SUCC) {
            break;
        }

        (void)ws63_sle_send_subport_data(uplink_msg.sub_port, uplink_msg.data, uplink_msg.len);
    }
#else
    return;
#endif
 }

#if (WS63_SLE_CORE_ENABLE == 1U)
/**
 * @brief 按指定子口投递下行数据到 WK2114 发送队列（自动分片）。
 */
static uint8_t ws63_enqueue_downlink_to_subport(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0U;
    uint8_t sent_count = 0U;

    if (!ws63_is_subport_enabled(sub_port)) {
        return 0U;
    }

    while (offset < len) {
        ws63_wk2114_tx_msg_t tx_msg;
        uint16_t remain;
        uint16_t chunk;

        remain = (uint16_t)(len - offset);
        chunk = (remain > WS63_TASK_QUEUE_PAYLOAD_MAX) ? WS63_TASK_QUEUE_PAYLOAD_MAX : remain;

        tx_msg.sub_port = sub_port;
        tx_msg.len = chunk;
        if (memcpy_s(tx_msg.data, sizeof(tx_msg.data), data + offset, chunk) != EOK) {
            break;
        }

        if (ws63_task_post_wk2114_tx(&tx_msg, WS63_OS_NO_WAIT) != ERRCODE_SUCC) {
            break;
        }

        offset = (uint16_t)(offset + chunk);
        sent_count++;
    }

    return sent_count;
}
#endif

/**
 * @brief 处理 SLE 下行数据：按模块开关分发到对应子口队列。
 */
#if (WS63_SLE_CORE_ENABLE == 1U)
static errcode_t ws63_sle_downlink_handler(const uint8_t *data, uint16_t len)
{
    uint8_t sent_count = 0U;

    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    /*
     * 优先把文本型下行喂给调试命令解析器。
     * 若已被消费，则不再透传到子口，避免同一帧被重复处理。
     */
    if (ws63_task_debug_try_consume_sle_downlink(data, len) == ERRCODE_SUCC) {
        return ERRCODE_SUCC;
    }

#if (WS63_SLE_LD2402_ENABLE == 1U)
    sent_count = (uint8_t)(sent_count + ws63_enqueue_downlink_to_subport(WS63_SLE_LD2402_SUBPORT, data, len));
#endif

#if (WS63_SLE_ZW101_ENABLE == 1U)
    sent_count = (uint8_t)(sent_count + ws63_enqueue_downlink_to_subport(WS63_SLE_ZW101_SUBPORT, data, len));
#endif

#if (WS63_SLE_CAMERA_ENABLE == 1U)
    sent_count = (uint8_t)(sent_count + ws63_enqueue_downlink_to_subport(WS63_SLE_CAMERA_SUBPORT, data, len));
#endif

    if (sent_count == 0U) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}
#endif

#if (WS63_SLE_CORE_ENABLE == 1U)
/**
 * @brief 初始化 SLE 从机桥接。
 */
static errcode_t ws63_sle_bridge_init(void)
{
    errcode_t ret;

    ret = ws63_sle_init(ws63_sle_downlink_handler);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] sle bridge init fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    osal_printk("[wk2114 final task] sle bridge init ok\r\n");
    return ERRCODE_SUCC;
}
#endif

/**
 * @brief WK2114 通信任务入口。
 */
static void *ws63_wk2114_task_entry(const char *arg)
{
    (void)arg;

    while (1) {
        if (ws63_task_wk2114_is_ready() == 0U) {
            wk2114_link_status_t status = {0};

            if (wk2114_init() != ERRCODE_SUCC) {
                ws63_set_wk2114_ready(0U);
                ws63_os_sleep_ms(WS63_WK2114_RETRY_GAP_MS);
                continue;
            }

            ws63_os_sleep_ms(10U);
            wk2114_get_link_status(&status);
            osal_printk("[wk2114 final task] link matched=%u gena=0x%02x\r\n",
                (unsigned int)status.matched,
                (unsigned int)status.last_gena);

            /* 每次链路重建都重置一次子口初始化标记与运行态门控。 */
            (void)memset_s(g_ws63_subport_inited,
                sizeof(g_ws63_subport_inited),
                0,
                sizeof(g_ws63_subport_inited));
            ws63_reset_subport_runtime_enable();
            (void)ws63_set_subport_runtime_enable(ZW101_SUBPORT, 0U);
            (void)ws63_set_subport_runtime_enable(WS63_SLE_CAMERA_SUBPORT, 0U);

            if (ws63_init_enabled_subports() != ERRCODE_SUCC) {
                ws63_set_wk2114_ready(0U);
                ws63_os_sleep_ms(WS63_WK2114_RETRY_GAP_MS);
                continue;
            }

            ws63_set_wk2114_ready(1U);
            osal_printk("[wk2114 final task] wk2114 worker ready\r\n");
        }

        ws63_wk2114_process_tx_queue();
        ws63_poll_and_dispatch();
        ws63_os_sleep_ms(WS63_WK2114_TASK_POLL_MS);
    }

    return NULL;
}

/**
 * @brief SLE 协议任务入口。
 */
static void *ws63_sle_task_entry(const char *arg)
{
#if (WS63_SLE_CORE_ENABLE == 1U)
    uint8_t sle_inited = 0U;

    (void)arg;
    while (1) {
        if (sle_inited == 0U) {
            if (ws63_sle_bridge_init() != ERRCODE_SUCC) {
                ws63_os_sleep_ms(WS63_SLE_RETRY_GAP_MS);
                continue;
            }
            sle_inited = 1U;
        }

        ws63_sle_process();
        ws63_sle_process_uplink_queue();
        ws63_os_sleep_ms(WS63_SLE_TASK_POLL_MS);
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
 * @brief 启动 WK2114 通信任务与发送队列。
 */
static errcode_t ws63_start_wk2114_task(void)
{
    errcode_t ret;

    if (g_ws63_wk2114_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_wk_tx_q",
        WS63_WK2114_TX_QUEUE_DEPTH,
        (uint16_t)sizeof(ws63_wk2114_tx_msg_t),
        &g_ws63_wk2114_tx_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] wk2114 tx queue create fail\r\n");
        return ret;
    }

    ret = ws63_os_start_task("ws63_wk_task",
        ws63_wk2114_task_entry,
        0U,
        WS63_WK2114_TASK_STACK_SIZE,
        WS63_WK2114_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_wk2114_tx_queue);
        g_ws63_wk2114_tx_queue = 0UL;
        return ret;
    }

    g_ws63_wk2114_task_started = 1U;
    return ERRCODE_SUCC;
}

/**
 * @brief 启动 SLE 协议任务与上行队列。
 */
static errcode_t ws63_start_sle_task(void)
{
    errcode_t ret;

#if (WS63_SLE_CORE_ENABLE != 1U)
    return ERRCODE_SUCC;
#endif

    if (g_ws63_sle_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_sle_up_q",
        WS63_SLE_UPLINK_QUEUE_DEPTH,
        (uint16_t)sizeof(ws63_sle_uplink_msg_t),
        &g_ws63_sle_uplink_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] sle uplink queue create fail\r\n");
        return ret;
    }

    ret = ws63_os_start_task("ws63_sle_task",
        ws63_sle_task_entry,
        0U,
        WS63_SLE_TASK_STACK_SIZE,
        WS63_SLE_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_sle_uplink_queue);
        g_ws63_sle_uplink_queue = 0UL;
        return ret;
    }

    g_ws63_sle_task_started = 1U;
    return ERRCODE_SUCC;
}

/**
 * @brief 注册子串口回调。
 */
errcode_t ws63_task_register_rx_callback(uint8_t sub_port, ws63_rx_callback_t callback)
{
    if (!ws63_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }

    g_ws63_rx_cb[sub_port] = callback;
    return ERRCODE_SUCC;
}

/**
 * @brief 确保 ZW101 子口已初始化（惰性初始化）。
 */
errcode_t ws63_task_ensure_zw101_ready(void)
{
    errcode_t ret;

    if (ws63_task_wk2114_is_ready() == 0U) {
        return ERRCODE_FAIL;
    }

    if ((WS63_SLE_ZW101_ENABLE != 1U) || (ws63_is_subport_config_enabled(ZW101_SUBPORT) == 0U)) {
        return ERRCODE_FAIL;
    }

    if (ws63_set_subport_runtime_enable(ZW101_SUBPORT, 1U) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    ret = ws63_init_subport_with_device(ZW101_SUBPORT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /*
     * 兜底策略：历史上可能出现“子口已初始化但 ZW101 未就绪”的状态。
     * 这里检测 ready 位，必要时补一次显式 init，避免 VERIFY 长时间卡在 ready=0。
     */
    if (zw101_is_ready() == 0U) {
        osal_printk("[wk2114 final task] ZW101 ready=0, force reinit\r\n");
        /*
         * 兜底重初始化前再次强制绑定回调，规避“历史失败后仍停留默认回调”的竞态。
         */
        g_ws63_rx_cb[ZW101_SUBPORT] = zw101_process_data;
        ret = zw101_init(ZW101_SUBPORT);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[wk2114 final task] ZW101 force reinit fail, ret=0x%x\r\n", (unsigned int)ret);
            return ret;
        }

        g_ws63_rx_cb[ZW101_SUBPORT] = zw101_process_data;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 确保 camera 子口已初始化（惰性初始化）。
 */
errcode_t ws63_task_ensure_camera_ready(void)
{
    if (ws63_task_wk2114_is_ready() == 0U) {
        return ERRCODE_FAIL;
    }

    if ((WS63_CAMERA_ENABLE != 1U) || (ws63_is_subport_config_enabled(WS63_SLE_CAMERA_SUBPORT) == 0U)) {
        return ERRCODE_FAIL;
    }

    if (ws63_set_subport_runtime_enable(WS63_SLE_CAMERA_SUBPORT, 1U) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return ws63_init_subport_with_device(WS63_SLE_CAMERA_SUBPORT);
}

/**
 * @brief 通过子串口发送数据（队列化，避免跨线程直接写硬件）。
 */
errcode_t ws63_task_send(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0U;

    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (!ws63_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (ws63_task_wk2114_is_ready() == 0U) {
        return ERRCODE_FAIL;
    }

    while (offset < len) {
        ws63_wk2114_tx_msg_t tx_msg;
        uint16_t remain;
        uint16_t chunk;

        remain = (uint16_t)(len - offset);
        chunk = (remain > WS63_TASK_QUEUE_PAYLOAD_MAX) ? WS63_TASK_QUEUE_PAYLOAD_MAX : remain;

        tx_msg.sub_port = sub_port;
        tx_msg.len = chunk;
        if (memcpy_s(tx_msg.data, sizeof(tx_msg.data), data + offset, chunk) != EOK) {
            return ERRCODE_FAIL;
        }

        if (ws63_task_post_wk2114_tx(&tx_msg, WS63_OS_NO_WAIT) != ERRCODE_SUCC) {
            return ERRCODE_FAIL;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 通过 SLE 向主机侧发送调试日志文本。
 */
errcode_t ws63_task_send_debug_log_to_host(const uint8_t *data, uint16_t len)
{
#if (WS63_SLE_CORE_ENABLE == 1U)
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (ws63_task_debug_is_debug_only_mode() == 0U) {
        return ERRCODE_SUCC;
    }

    return ws63_sle_send_debug_data(data, len);
#else
    (void)data;
    (void)len;
    return ERRCODE_FAIL;
#endif
}

/**
 * @brief WK2114 最终版业务管理任务入口。
 *
 * 管理任务职责：
 * 1) 执行与外设无强耦合的应用流程（调试命令、编码器采样、喂狗）；
 * 2) 启动 WK2114/SLE/RGB/BEEP 独立任务，体现 RTOS 并发架构。
 */
void *ws63_task_entry(const char *arg)
{
    errcode_t wdt_ret;
    uint32_t boot_ms;
    uint8_t lock_mgr_started = 0U;

    (void)arg;
    ws63_os_sleep_ms(WS63_BOOT_DELAY_MS);
    boot_ms = ws63_os_tick_ms();

    /* 电机/编码器初始化独立于 WK2114 链路，失败仅记录日志，不阻断任务。 */
    ws63_motor_encoder_init();

    /* 在线调试串口：用于电机命令控测与状态日志输出。 */
    ws63_task_debug_init();

    /* 每次管理任务启动都先把运行态子口门控恢复到配置默认值。 */
    ws63_reset_subport_runtime_enable();
    (void)ws63_set_subport_runtime_enable(ZW101_SUBPORT, 0U);
    (void)ws63_set_subport_runtime_enable(WS63_SLE_CAMERA_SUBPORT, 0U);

    if (ws63_start_sle_task() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start sle task fail\r\n");
    }

    if (ws63_start_wk2114_task() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start wk2114 task fail\r\n");
    }

    if (ws63_rgb_task_start() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start rgb task fail\r\n");
    }

    if (ws63_beep_task_start() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start beep task fail\r\n");
    }

#if (WS63_TTP229_ENABLE == 1U)
    if (ws63_ttp229_task_start() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start ttp229 task fail\r\n");
    }
#endif

    if (ws63_zw101_task_start() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start zw101 task fail\r\n");
    }

#if (WS63_CAMERA_ENABLE == 1U)
    if (ws63_camera_task_start() != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] start camera task fail\r\n");
    }
#endif

    while (1) {
        uint32_t now_ms;

        now_ms = ws63_os_tick_ms();
        if (ws63_task_motor_encoder_is_ready() == 1U) {
            ws63_encoder_sample(now_ms);
        }

        ws63_task_debug_process(now_ms);

        /*
         * 门锁编排任务采用“延后启动 + 调试模式门控”：
         * 1) 先让调试命令有机会把系统切到 DEBUG_ONLY；
         * 2) 只有在观察窗口结束且未进入纯调试模式时，才拉起门锁任务；
         * 3) 若后续执行 DEBUG EXIT，门锁任务会在下一轮循环恢复启动。
         */
        if ((lock_mgr_started == 0U) &&
            (ws63_task_debug_is_debug_only_mode() == 0U) &&
            ((uint32_t)(now_ms - boot_ms) >= WS63_DEBUG_BOOT_DECISION_MS)) {
            if (ws63_lock_mgr_task_start() == ERRCODE_SUCC) {
                lock_mgr_started = 1U;
            } else {
                osal_printk("[wk2114 final task] start lock mgr task fail\r\n");
            }
        }

        /* 多任务并行后仍在管理线程持续喂狗，避免高负载场景触发复位。 */
        wdt_ret = ws63_os_feed_watchdog();
        if (wdt_ret != ERRCODE_SUCC) {
#if (WS63_RGB_LOG_ENABLE == 1U)
            osal_printk("[wk2114 final task] watchdog kick fail, ret=0x%x\r\n", (unsigned int)wdt_ret);
#endif
        }

        ws63_os_sleep_ms(WS63_TASK_POLL_MS);
    }

    return NULL;
}
