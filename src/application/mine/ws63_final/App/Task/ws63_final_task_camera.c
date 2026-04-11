/**
 * @file ws63_final_task_camera.c
 * @brief Task 层 camera 子模块实现（子口发送队列 + 回包识别）。
 */

#include "ws63_final_task_internal.h"

#include <ctype.h>
#include <string.h>

#include "osal_debug.h"
#include "securec.h"

#include "ws63_final_config.h"
#include "ws63_final_osal.h"

#if (WS63_CAMERA_ENABLE == 1U)
/* camera 业务消息统一前缀，便于外部人脸模块按协议区分命令文本。 */
#define WS63_CAMERA_PREFIX_TEXT "[camera]"
/* camera 任务发送重试次数：wk2114 尚未就绪时，短暂退避再尝试。 */
#define WS63_CAMERA_SEND_RETRY_MAX 3U
/* camera 发送退避时间：避免子串口暂不可写时刷屏。 */
#define WS63_CAMERA_SEND_RETRY_GAP_MS 20U
/* camera 回包缓存长度：只保留一条最近回包，供调试和状态机观察。 */
#define WS63_CAMERA_REPLY_TEXT_MAX 96U

typedef enum {
    WS63_CAMERA_CMD_SEND_TEXT = 0,
} ws63_camera_cmd_type_t;

typedef struct {
    ws63_camera_cmd_type_t type;
    char text[WS63_TASK_QUEUE_PAYLOAD_MAX + 1U];
} ws63_camera_ctrl_msg_t;

static unsigned long g_ws63_camera_cmd_queue = 0UL;
static uint8_t g_ws63_camera_task_started = 0U;
static char g_ws63_camera_last_reply[WS63_CAMERA_REPLY_TEXT_MAX] = {0};
static uint32_t g_ws63_camera_last_reply_ms = 0U;

/**
 * @brief camera 状态锁。
 */
static unsigned int ws63_camera_lock(void)
{
    return ws63_os_irq_lock();
}

/**
 * @brief camera 状态解锁。
 */
static void ws63_camera_unlock(unsigned int irq_status)
{
    ws63_os_irq_unlock(irq_status);
}

/**
 * @brief 判断文本是否包含指定关键字（忽略大小写）。
 *
 * @param text   源文本。
 * @param needle 目标关键字。
 * @return uint8_t 1=包含，0=不包含。
 */
static uint8_t ws63_camera_text_contains_ci(const char *text, const char *needle)
{
    size_t text_len;
    size_t needle_len;
    size_t i;

    if ((text == NULL) || (needle == NULL)) {
        return 0U;
    }

    text_len = strlen(text);
    needle_len = strlen(needle);
    if ((text_len == 0U) || (needle_len == 0U) || (needle_len > text_len)) {
        return 0U;
    }

    for (i = 0U; i + needle_len <= text_len; i++) {
        size_t j;

        for (j = 0U; j < needle_len; j++) {
            char lhs = (char)tolower((unsigned char)text[i + j]);
            char rhs = (char)tolower((unsigned char)needle[j]);

            if (lhs != rhs) {
                break;
            }
        }

        if (j == needle_len) {
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief 保存最近一次 camera 回包文本。
 *
 * 只保留可打印字符，避免二进制回包污染调试口。
 */
static void ws63_camera_store_reply(const uint8_t *data, uint16_t len)
{
    char reply_buf[WS63_CAMERA_REPLY_TEXT_MAX] = {0};
    size_t copy_len;
    size_t i;
    unsigned int irq_status;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    copy_len = (len < (uint16_t)(sizeof(reply_buf) - 1U)) ? (size_t)len : (sizeof(reply_buf) - 1U);
    for (i = 0U; i < copy_len; i++) {
        uint8_t ch = data[i];

        if ((ch < 0x20U) && (ch != '\r') && (ch != '\n') && (ch != '\t')) {
            reply_buf[i] = '?';
        } else {
            reply_buf[i] = (char)ch;
        }
    }
    reply_buf[copy_len] = '\0';

    irq_status = ws63_camera_lock();
    (void)memset_s(g_ws63_camera_last_reply,
        sizeof(g_ws63_camera_last_reply),
        0,
        sizeof(g_ws63_camera_last_reply));
    (void)memcpy_s(g_ws63_camera_last_reply,
        sizeof(g_ws63_camera_last_reply),
        reply_buf,
        copy_len + 1U);
    g_ws63_camera_last_reply_ms = ws63_os_tick_ms();
    ws63_camera_unlock(irq_status);
}

/**
 * @brief 尝试把 camera 回包解析为认证结果。
 *
 * 目前先兼容最常见的文本回应，后续若外部模块协议变更，只需要替换这里的
 * 关键字判定即可，不影响上层门锁状态机。
 */
static void ws63_camera_try_report_auth_result(const char *reply_text)
{
    if (reply_text == NULL) {
        return;
    }

    if ((ws63_camera_text_contains_ci(reply_text, "pass") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "success") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "ok") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "allow") != 0U)) {
        (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_CAMERA, 1U);
        return;
    }

    if ((ws63_camera_text_contains_ci(reply_text, "fail") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "deny") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "error") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "timeout") != 0U)) {
        (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_CAMERA, 0U);
    }
}

/**
 * @brief camera 接收回调：保存最近回包并尝试识别认证结果。
 */
void ws63_task_camera_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    char reply_text[WS63_CAMERA_REPLY_TEXT_MAX] = {0};
    size_t copy_len;

    (void)sub_port;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    copy_len = (len < (uint16_t)(sizeof(reply_text) - 1U)) ? (size_t)len : (sizeof(reply_text) - 1U);
    (void)memcpy_s(reply_text, sizeof(reply_text), data, copy_len);
    reply_text[copy_len] = '\0';

    ws63_camera_store_reply(data, len);
    osal_printk("[camera] rx %s\r\n", reply_text);
    ws63_camera_try_report_auth_result(reply_text);
}

/**
 * @brief 向 camera 子口发送一条业务文本。
 *
 * @param payload 不含前缀的业务文本，例如 `wake distance:123`。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_camera_send_message(const char *payload)
{
    ws63_camera_ctrl_msg_t msg;

    if ((payload == NULL) || (payload[0] == '\0')) {
        return ERRCODE_INVALID_PARAM;
    }

    if (g_ws63_camera_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    msg.type = WS63_CAMERA_CMD_SEND_TEXT;
    if (strncpy_s(msg.text, sizeof(msg.text), payload, sizeof(msg.text) - 1U) != EOK) {
        return ERRCODE_FAIL;
    }

    return ws63_os_msg_queue_send(g_ws63_camera_cmd_queue,
        &msg,
        (uint16_t)sizeof(msg),
        WS63_OS_NO_WAIT);
}

/**
 * @brief 尝试把待发文本封装成 `[camera]...` 格式。
 */
static errcode_t ws63_camera_format_send_text(const char *payload, char *send_text, uint16_t send_text_len)
{
    int32_t ret;

    if ((payload == NULL) || (send_text == NULL) || (send_text_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (strncmp(payload, WS63_CAMERA_PREFIX_TEXT, strlen(WS63_CAMERA_PREFIX_TEXT)) == 0) {
        ret = snprintf_s(send_text,
            (size_t)send_text_len,
            (size_t)(send_text_len - 1U),
            "%s\r\n",
            payload);
    } else {
        ret = snprintf_s(send_text,
            (size_t)send_text_len,
            (size_t)(send_text_len - 1U),
            "%s%s\r\n",
            WS63_CAMERA_PREFIX_TEXT,
            payload);
    }

    if (ret < 0) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief camera 独立任务入口。
 *
 * 任务职责：串行化 camera 文本发送，确保上层状态机只关心“发什么”，
 * 不关心 WK2114 子口是否暂时忙碌。
 */
static void *ws63_camera_task_entry(const char *arg)
{
    (void)arg;

    while (1) {
        ws63_camera_ctrl_msg_t msg;
        uint32_t size;
        errcode_t ret;
        uint8_t retry;

        size = (uint32_t)sizeof(msg);
        ret = ws63_os_msg_queue_recv(g_ws63_camera_cmd_queue, &msg, &size, WS63_OS_WAIT_FOREVER);
        if (ret != ERRCODE_SUCC) {
            continue;
        }

        if (msg.type != WS63_CAMERA_CMD_SEND_TEXT) {
            continue;
        }

        ret = ERRCODE_FAIL;
        for (retry = 0U; retry < WS63_CAMERA_SEND_RETRY_MAX; retry++) {
            uint8_t send_buf[WS63_TASK_QUEUE_PAYLOAD_MAX + 16U] = {0};

            if (ws63_camera_format_send_text(msg.text, (char *)send_buf, sizeof(send_buf)) != ERRCODE_SUCC) {
                ret = ERRCODE_FAIL;
                break;
            }

            ret = ws63_task_send(WS63_SLE_CAMERA_SUBPORT, send_buf, (uint16_t)strlen((char *)send_buf));
            if (ret == ERRCODE_SUCC) {
                break;
            }

            ws63_os_sleep_ms(WS63_CAMERA_SEND_RETRY_GAP_MS);
        }

        if (ret != ERRCODE_SUCC) {
            osal_printk("[camera] send fail, text=%s\r\n", msg.text);
        }
    }

    return NULL;
}

/**
 * @brief 启动 camera 独立任务。
 */
errcode_t ws63_camera_task_start(void)
{
    errcode_t ret;

    if (g_ws63_camera_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_camera_q",
        WS63_CAMERA_CMD_QUEUE_DEPTH,
        (uint16_t)sizeof(ws63_camera_ctrl_msg_t),
        &g_ws63_camera_cmd_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[camera] queue create fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = ws63_os_start_task("ws63_camera_task",
        ws63_camera_task_entry,
        0U,
        WS63_CAMERA_TASK_STACK_SIZE,
        WS63_CAMERA_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_camera_cmd_queue);
        g_ws63_camera_cmd_queue = 0UL;
        osal_printk("[camera] task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_camera_task_started = 1U;
    osal_printk("[camera] task start ok, subport=%u baud=%u\r\n",
        (unsigned int)WS63_SLE_CAMERA_SUBPORT,
        (unsigned int)WS63_SUBPORT3_BAUD);
    return ERRCODE_SUCC;
}
#endif