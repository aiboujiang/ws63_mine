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
/* camera 分数阈值：score>=0.75 判定为通过（定点千分比，避免浮点依赖）。 */
#define WS63_CAMERA_SCORE_PASS_MILLI 750U

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
/* camera 接收重组缓冲：处理 WK2114 分包导致的半包文本。 */
static char g_ws63_camera_rx_assembled[WS63_CAMERA_REPLY_TEXT_MAX] = {0};
static uint16_t g_ws63_camera_rx_assembled_len = 0U;

/**
 * @brief camera 状态锁。
 */
static unsigned int ws63_camera_lock(void)
{
    return ws63_os_irq_lock();
}

/**
 * @brief 将一条完整 camera 文本回包投递到 SLE 上行队列。
 *
 * 说明：
 * 1) 仅在回包完成重组后上行，避免主机侧出现 [CAMERA] 分包重复标签；
 * 2) 复用 Task 层统一上行队列，不在接收回调里直接走 SLE 发送路径。
 */
static void ws63_camera_post_uplink_reply(const char *reply_text)
{
    ws63_sle_uplink_msg_t msg;
    uint16_t text_len;
    errcode_t ret;

    if ((reply_text == NULL) || (reply_text[0] == '\0')) {
        return;
    }

    text_len = (uint16_t)strlen(reply_text);
    if ((text_len == 0U) || (text_len > WS63_TASK_QUEUE_PAYLOAD_MAX)) {
        return;
    }

    msg.sub_port = WS63_SLE_CAMERA_SUBPORT;
    msg.len = text_len;
    if (memcpy_s(msg.data, sizeof(msg.data), reply_text, text_len) != EOK) {
        osal_printk("[camera] uplink copy fail, len=%u\r\n", (unsigned int)text_len);
        return;
    }

    ret = ws63_task_post_sle_uplink(&msg, WS63_OS_NO_WAIT);
    osal_printk("[camera] uplink queued len=%u ret=0x%x ts=%u\r\n",
        (unsigned int)text_len,
        (unsigned int)ret,
        (unsigned int)ws63_os_tick_ms());
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
 * @brief 从回包文本中解析 score 小数字段（如 score=0.82 / score:0.75）。
 *
 * 解析结果使用千分比：
 * 1) 0.75 -> 750；
 * 2) 1.00 -> 1000；
 * 3) 非法格式返回失败。
 */
static uint8_t ws63_camera_parse_score_milli(const char *text, uint16_t *score_milli_out)
{
    size_t i;
    size_t text_len;
    const char *p;
    const char *comma;
    uint8_t need_kv_sep = 0U;
    uint32_t int_part = 0U;
    uint32_t frac_part = 0U;
    uint32_t frac_base = 1000U;
    uint8_t has_digit = 0U;
    uint8_t frac_digit_count = 0U;

    if ((text == NULL) || (score_milli_out == NULL)) {
        return 0U;
    }

    text_len = strlen(text);
    if (text_len == 0U) {
        return 0U;
    }

    p = NULL;
    for (i = 0U; i + 4U < text_len; i++) {
        if ((tolower((unsigned char)text[i]) == 's') &&
            (tolower((unsigned char)text[i + 1U]) == 'c') &&
            (tolower((unsigned char)text[i + 2U]) == 'o') &&
            (tolower((unsigned char)text[i + 3U]) == 'r') &&
            (tolower((unsigned char)text[i + 4U]) == 'e')) {
            p = &text[i + 5U];
            need_kv_sep = 1U;
            break;
        }
    }

    if (p == NULL) {
        /* 兼容另一类回包格式："[name,0.77]"，分数位于逗号后。 */
        comma = strrchr(text, ',');
        if (comma == NULL) {
            return 0U;
        }
        p = comma + 1;
    }

    while ((*p == ' ') || (*p == '\t')) {
        p++;
    }
    if (need_kv_sep != 0U) {
        if ((*p != '=') && (*p != ':')) {
            return 0U;
        }
        p++;
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
    }

    while ((*p >= '0') && (*p <= '9')) {
        has_digit = 1U;
        int_part = int_part * 10U + (uint32_t)(*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        while ((*p >= '0') && (*p <= '9') && (frac_digit_count < 3U)) {
            frac_part = frac_part * 10U + (uint32_t)(*p - '0');
            frac_digit_count++;
            frac_base /= 10U;
            p++;
            has_digit = 1U;
        }
        while ((*p >= '0') && (*p <= '9')) {
            p++;
        }
    }

    if (has_digit == 0U) {
        return 0U;
    }

    if (int_part > 1U) {
        return 0U;
    }

    frac_part *= frac_base;
    *score_milli_out = (uint16_t)(int_part * 1000U + frac_part);
    return 1U;
}

/**
 * @brief 从 camera 回包中提取标签文本（如 [Noah_Xiang,0.77] -> Noah_Xiang）。
 *
 * @param text 输入回包。
 * @param label_out 输出标签缓冲。
 * @param label_out_len 输出标签缓冲长度。
 * @return uint8_t 1=提取成功，0=提取失败。
 */
static uint8_t ws63_camera_parse_label(const char *text, char *label_out, uint16_t label_out_len)
{
    const char *left;
    const char *comma;
    size_t copy_len;

    if ((text == NULL) || (label_out == NULL) || (label_out_len <= 1U)) {
        return 0U;
    }

    label_out[0] = '\0';

    left = strchr(text, '[');
    comma = strrchr(text, ',');
    if ((left == NULL) || (comma == NULL) || (comma <= (left + 1))) {
        return 0U;
    }

    copy_len = (size_t)(comma - (left + 1));
    if (copy_len >= (size_t)label_out_len) {
        copy_len = (size_t)label_out_len - 1U;
    }

    if (copy_len == 0U) {
        return 0U;
    }

    if (memcpy_s(label_out, label_out_len, left + 1, copy_len) != EOK) {
        return 0U;
    }
    label_out[copy_len] = '\0';
    return 1U;
}

/**
 * @brief 保存最近一次 camera 回包文本。
 *
 * 只保留可打印字符，避免二进制回包污染调试口。
 */
static void ws63_camera_store_reply_text(const char *text)
{
    char reply_buf[WS63_CAMERA_REPLY_TEXT_MAX] = {0};
    size_t copy_len;
    size_t i;
    unsigned int irq_status;

    if ((text == NULL) || (text[0] == '\0')) {
        return;
    }

    copy_len = strnlen(text, sizeof(reply_buf) - 1U);
    for (i = 0U; i < copy_len; i++) {
        uint8_t ch = (uint8_t)text[i];

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
    uint16_t score_milli;
    char label[32] = {0};

    if (reply_text == NULL) {
        return;
    }

    if (ws63_camera_parse_score_milli(reply_text, &score_milli) != 0U) {
        if (score_milli >= WS63_CAMERA_SCORE_PASS_MILLI) {
            (void)ws63_camera_parse_label(reply_text, label, sizeof(label));
            ws63_lock_mgr_update_camera_label(label);
            osal_printk("[camera] score pass, score=%u/1000\r\n", (unsigned int)score_milli);
            (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_CAMERA, 1U);
            return;
        }
    }

    if ((ws63_camera_text_contains_ci(reply_text, "pass") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "success") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "ok") != 0U) ||
        (ws63_camera_text_contains_ci(reply_text, "allow") != 0U)) {
        (void)ws63_camera_parse_label(reply_text, label, sizeof(label));
        ws63_lock_mgr_update_camera_label(label);
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
 * @brief 处理一条完整 camera 文本回包。
 */
static void ws63_camera_handle_full_reply(const char *reply_text)
{
    uint16_t reply_len;

    if ((reply_text == NULL) || (reply_text[0] == '\0')) {
        return;
    }

    reply_len = (uint16_t)strlen(reply_text);
    osal_printk("[camera] full reply len=%u text=%s\r\n",
        (unsigned int)reply_len,
        reply_text);

    /* 先投递完整上行，再做本地状态更新，保证主机与本地日志观察到同一条完整文本。 */
    ws63_camera_post_uplink_reply(reply_text);
    ws63_camera_store_reply_text(reply_text);
    ws63_camera_try_report_auth_result(reply_text);
}

/**
 * @brief 把子口数据追加到重组缓冲，并按结束符产出完整文本。
 */
static void ws63_camera_feed_rx_bytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    osal_printk("[camera] rx chunk len=%u assembled=%u ts=%u\r\n",
        (unsigned int)len,
        (unsigned int)g_ws63_camera_rx_assembled_len,
        (unsigned int)ws63_os_tick_ms());

    for (i = 0U; i < len; i++) {
        uint8_t ch = data[i];

        if ((ch == '\r') || (ch == '\n')) {
            if (g_ws63_camera_rx_assembled_len > 0U) {
                g_ws63_camera_rx_assembled[g_ws63_camera_rx_assembled_len] = '\0';
                osal_printk("[camera] rx delim=CRLF assembled_len=%u ts=%u\r\n",
                    (unsigned int)g_ws63_camera_rx_assembled_len,
                    (unsigned int)ws63_os_tick_ms());
                ws63_camera_handle_full_reply(g_ws63_camera_rx_assembled);
                g_ws63_camera_rx_assembled_len = 0U;
                g_ws63_camera_rx_assembled[0] = '\0';
            }
            continue;
        }

        if (g_ws63_camera_rx_assembled_len >= (uint16_t)(sizeof(g_ws63_camera_rx_assembled) - 1U)) {
            /* 避免异常长文本一直占用缓冲，直接丢弃当前半包并重新同步。 */
            osal_printk("[camera] rx overflow drop partial len=%u ts=%u\r\n",
                (unsigned int)g_ws63_camera_rx_assembled_len,
                (unsigned int)ws63_os_tick_ms());
            g_ws63_camera_rx_assembled_len = 0U;
            g_ws63_camera_rx_assembled[0] = '\0';
        }

        if ((ch < 0x20U) && (ch != '\t')) {
            g_ws63_camera_rx_assembled[g_ws63_camera_rx_assembled_len++] = '?';
        } else {
            g_ws63_camera_rx_assembled[g_ws63_camera_rx_assembled_len++] = (char)ch;
        }

        if (ch == ']') {
            /* Noah_Xiang,0.77 这类协议以 ']' 结束，收到即产出，解决分包问题。 */
            g_ws63_camera_rx_assembled[g_ws63_camera_rx_assembled_len] = '\0';
            osal_printk("[camera] rx delim=] assembled_len=%u ts=%u\r\n",
                (unsigned int)g_ws63_camera_rx_assembled_len,
                (unsigned int)ws63_os_tick_ms());
            ws63_camera_handle_full_reply(g_ws63_camera_rx_assembled);
            g_ws63_camera_rx_assembled_len = 0U;
            g_ws63_camera_rx_assembled[0] = '\0';
        }
    }
}

/**
 * @brief camera 接收回调：保存最近回包并尝试识别认证结果。
 */
void ws63_task_camera_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    (void)sub_port;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    osal_printk("[camera] process sub_port=%u len=%u ts=%u\r\n",
        (unsigned int)sub_port,
        (unsigned int)len,
        (unsigned int)ws63_os_tick_ms());

    /* camera 有任意有效输入就续命，避免人脸链路处理时被唤醒窗口提前切回。 */
    (void)ws63_lock_mgr_refresh_auth_window();

    ws63_camera_feed_rx_bytes(data, len);
}

/**
 * @brief 向 camera 子口发送一条业务文本。
 *
 * @param payload 不含前缀的业务文本，例如 `action` 或 `Die`。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_camera_send_message(const char *payload)
{
    ws63_camera_ctrl_msg_t msg;

    if ((payload == NULL) || (payload[0] == '\0')) {
        return ERRCODE_INVALID_PARAM;
    }

    if (g_ws63_camera_task_started == 0U) {
        osal_printk("[camera] send reject, task not started payload=%s ts=%u\r\n",
            payload,
            (unsigned int)ws63_os_tick_ms());
        return ERRCODE_FAIL;
    }

    msg.type = WS63_CAMERA_CMD_SEND_TEXT;
    if (strncpy_s(msg.text, sizeof(msg.text), payload, sizeof(msg.text) - 1U) != EOK) {
        osal_printk("[camera] send payload copy fail payload=%s ts=%u\r\n",
            payload,
            (unsigned int)ws63_os_tick_ms());
        return ERRCODE_FAIL;
    }

    osal_printk("[camera] queue push payload=%s ts=%u\r\n",
        payload,
        (unsigned int)ws63_os_tick_ms());

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

        osal_printk("[camera] queue recv type=%u text=%s size=%u ts=%u\r\n",
            (unsigned int)msg.type,
            msg.text,
            (unsigned int)size,
            (unsigned int)ws63_os_tick_ms());

        if (msg.type != WS63_CAMERA_CMD_SEND_TEXT) {
            continue;
        }

        ret = ERRCODE_FAIL;
        for (retry = 0U; retry < WS63_CAMERA_SEND_RETRY_MAX; retry++) {
            uint8_t send_buf[WS63_TASK_QUEUE_PAYLOAD_MAX + 16U] = {0};

            if (ws63_camera_format_send_text(msg.text, (char *)send_buf, sizeof(send_buf)) != ERRCODE_SUCC) {
                ret = ERRCODE_FAIL;
                osal_printk("[camera] format fail text=%s ts=%u\r\n",
                    msg.text,
                    (unsigned int)ws63_os_tick_ms());
                break;
            }

            osal_printk("[camera] tx try=%u payload=%s ts=%u\r\n",
                (unsigned int)(retry + 1U),
                send_buf,
                (unsigned int)ws63_os_tick_ms());

            ret = ws63_task_send(WS63_SLE_CAMERA_SUBPORT, send_buf, (uint16_t)strlen((char *)send_buf));
            if (ret == ERRCODE_SUCC) {
                osal_printk("[camera] tx ok try=%u payload=%s ts=%u\r\n",
                    (unsigned int)(retry + 1U),
                    send_buf,
                    (unsigned int)ws63_os_tick_ms());
                break;
            }

            osal_printk("[camera] tx fail try=%u ret=0x%x payload=%s ts=%u\r\n",
                (unsigned int)(retry + 1U),
                (unsigned int)ret,
                send_buf,
                (unsigned int)ws63_os_tick_ms());

            ws63_os_sleep_ms(WS63_CAMERA_SEND_RETRY_GAP_MS);
        }

        if (ret != ERRCODE_SUCC) {
            osal_printk("[camera] send fail, text=%s ts=%u\r\n",
                msg.text,
                (unsigned int)ws63_os_tick_ms());
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
        osal_printk("[camera] task already started ts=%u\r\n", (unsigned int)ws63_os_tick_ms());
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_camera_q",
        WS63_CAMERA_CMD_QUEUE_DEPTH,
        (uint16_t)sizeof(ws63_camera_ctrl_msg_t),
        &g_ws63_camera_cmd_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[camera] queue create fail, ret=0x%x ts=%u\r\n",
            (unsigned int)ret,
            (unsigned int)ws63_os_tick_ms());
        return ret;
    }

    osal_printk("[camera] queue create ok depth=%u ts=%u\r\n",
        (unsigned int)WS63_CAMERA_CMD_QUEUE_DEPTH,
        (unsigned int)ws63_os_tick_ms());

    ret = ws63_os_start_task("ws63_camera_task",
        ws63_camera_task_entry,
        0U,
        WS63_CAMERA_TASK_STACK_SIZE,
        WS63_CAMERA_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_camera_cmd_queue);
        g_ws63_camera_cmd_queue = 0UL;
        osal_printk("[camera] task start fail, ret=0x%x ts=%u\r\n",
            (unsigned int)ret,
            (unsigned int)ws63_os_tick_ms());
        return ret;
    }

    g_ws63_camera_task_started = 1U;
    osal_printk("[camera] task start ok, subport=%u baud=%u ts=%u\r\n",
        (unsigned int)WS63_SLE_CAMERA_SUBPORT,
        (unsigned int)WS63_SUBPORT3_BAUD,
        (unsigned int)ws63_os_tick_ms());
    return ERRCODE_SUCC;
}
#endif