/**
 * @file ws63_final_task_sensor_bridge.c
 * @brief Task 层传感器桥接子模块（LD2402/ZW101）。
 */

#include "ws63_final_task_internal.h"

#include <stddef.h>
#include <string.h>

#include "osal_debug.h"
#include "securec.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_final_sle.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"

/* ZW101 VERIFY 任务请求状态：独立于门锁主状态机，避免阻塞其他认证链路。 */
static uint8_t g_ws63_zw101_task_started = 0U;
static uint8_t g_ws63_zw101_verify_requested = 0U;
static uint8_t g_ws63_zw101_verify_cancelled = 0U;
static uint8_t g_ws63_zw101_verify_wait_release = 0U;
static uint8_t g_ws63_zw101_verify_disabled = 0U;
static uint8_t g_ws63_zw101_verify_fail_streak = 0U;
/* ACK_TIMEOUT 独立重试计数：只统计生命周期内的通信超时，不和识别失败混用。 */
static uint8_t g_ws63_zw101_verify_timeout_streak = 0U;
/* 记录最近一次 VERIFY 的 ACK，用于重试策略分支判断。 */
static uint8_t g_ws63_zw101_last_verify_ack = 0xFFU;

/* 默认 VERIFY 参数：与 sle_uart_slave 已验证口径保持一致。 */
#define WS63_ZW101_VERIFY_LEVEL_DEFAULT 3U
#define WS63_ZW101_VERIFY_ID_DEFAULT 0xFFFFU
#define WS63_ZW101_VERIFY_PARAM_DEFAULT 0x0000U

/* 连续失败保护：达到阈值后禁用 VERIFY 并上报报警消息。 */
#define WS63_ZW101_VERIFY_FAIL_DISABLE_THRESHOLD 5U
/* ACK_TIMEOUT 允许的生命周期内自动重拉次数：避免链路瞬态把一次认证直接打成终态失败。 */
#define WS63_ZW101_VERIFY_TIMEOUT_RETRY_THRESHOLD 3U
#define WS63_ZW101_ALARM_TEXT "ZW101 VERIFY DISABLED"
#define WS63_ZW101_READY_RETRY_GAP_MS 1000U
/* 离手检测不需要高频轮询，适当放大间隔可显著降低串口与日志负载。 */
#define WS63_ZW101_RELEASE_CHECK_GAP_MS 500U

/* 详细追踪默认关闭：避免 VERIFY 重试链路在正常场景刷屏，排障时再临时打开。 */
#define WS63_ZW101_TRACE_DETAIL_ENABLE 0U

/* ACK 语义常量：用于避免重试分支中的魔法值。 */
#define WS63_ZW101_ACK_OK 0x00U
#define WS63_ZW101_ACK_NOT_PRESSED 0x09U
#define WS63_ZW101_ACK_TIMEOUT 0x26U
#define WS63_ZW101_ACK_UNKNOWN 0xFFU

/* 仅供本文件内部使用，避免把 ZW101 任务就绪查询扩散到上层接口。 */
uint8_t ws63_task_zw101_is_ready(void);


static void ws63_zw101_reset_timeout_retry_state(void);
static uint8_t ws63_zw101_try_retry_verify_after_timeout(void);

/**
 * @brief 把常见 ACK 码转换为可读文本，便于串口日志快速判断失败类型。
 */
static const char *ws63_zw101_ack_to_text(uint8_t ack_code)
{
    switch (ack_code) {
        case 0x00U:
            return "OK";
        case 0x08U:
            return "NO_MATCH";
        case 0x09U:
            return "NOT_PRESSED";
        case 0x24U:
            return "BAD_IMAGE";
        case 0x26U:
            return "ACK_TIMEOUT";
        default:
            return "OTHER";
    }
}

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
/**
 * @brief 按压状态文本。
 */
static const char *ws63_zw101_finger_text(uint8_t finger_present)
{
    return (finger_present != 0U) ? "PRESSED" : "RELEASED";
}
#endif

/**
 * @brief 打印 VERIFY 状态快照，统一观察请求位/取消位/禁用位变化。
 *
 * 说明：额外带上 timeout 重试计数，便于现场区分“识别失败”和“通信超时重拉”。
 */
static void ws63_zw101_trace_verify_state(const char *tag)
{
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    unsigned int irq_status;
    uint8_t request;
    uint8_t cancelled;
    uint8_t disabled;
    uint8_t wait_release;
    uint8_t fail_streak;
    uint8_t timeout_streak;
    uint8_t armed;
    uint8_t ready;

    if (tag == NULL) {
        return;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    request = g_ws63_zw101_verify_requested;
    cancelled = g_ws63_zw101_verify_cancelled;
    disabled = g_ws63_zw101_verify_disabled;
    wait_release = g_ws63_zw101_verify_wait_release;
    fail_streak = g_ws63_zw101_verify_fail_streak;
    timeout_streak = g_ws63_zw101_verify_timeout_streak;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    armed = ws63_lock_mgr_is_armed();
    ready = ws63_task_zw101_is_ready();

    osal_printk("[zw101 trace] %s req=%u cancel=%u wait_release=%u disabled=%u fail_streak=%u timeout_retry=%u armed=%u ready=%u\r\n",
        tag,
        (unsigned int)request,
        (unsigned int)cancelled,
        (unsigned int)wait_release,
        (unsigned int)disabled,
        (unsigned int)fail_streak,
        (unsigned int)timeout_streak,
        (unsigned int)armed,
        (unsigned int)ready);
#else
    (void)tag;
#endif
}

/**
 * @brief ZW101 VERIFY 任务状态锁。
 */


/**
 * @brief ZW101 VERIFY 任务状态解锁。
 */


/**
 * @brief 清空 ACK_TIMEOUT 的独立重试计数。
 *
 * 说明：该计数只服务于“当前生命周期内的通信超时重拉”，因此在新 ARMED 窗口、
 *       认证成功或超时策略彻底放弃时都需要同步清零，避免把上一轮历史带进下一轮。
 */
static void ws63_zw101_reset_timeout_retry_state(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_verify_timeout_streak = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief 在 ACK_TIMEOUT 且生命周期仍有效时，立即重拉一次 VERIFY。
 *
 * @return uint8_t 1=本次超时已转成内部重试，不再上报门锁失败；0=应继续走失败上报。
 */
static uint8_t ws63_zw101_try_retry_verify_after_timeout(void)
{
    unsigned int irq_status;
    uint8_t timeout_retry_streak;

    if (ws63_lock_mgr_is_armed() == 0U) {
        return 0U;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_zw101_verify_timeout_streak < 0xFFU) {
        g_ws63_zw101_verify_timeout_streak++;
    }
    timeout_retry_streak = g_ws63_zw101_verify_timeout_streak;

    if (timeout_retry_streak < WS63_ZW101_VERIFY_TIMEOUT_RETRY_THRESHOLD) {
        /*
         * 只有“生命周期仍然活着”时才重拉 VERIFY：这样 ACK_TIMEOUT 不会把门锁
         * 误推进 Die/lockout，而是当成一次可恢复的通信异常继续尝试。
         */
        g_ws63_zw101_verify_requested = 1U;
        g_ws63_zw101_verify_cancelled = 0U;
        g_ws63_zw101_verify_wait_release = 0U;
        g_ws63_zw101_last_verify_ack = WS63_ZW101_ACK_TIMEOUT;
        WS63_FINAL_IRQ_UNLOCK(irq_status);

        osal_printk("[zw101] VERIFY timeout, retry queued timeout_retry=%u/%u\r\n",
            (unsigned int)timeout_retry_streak,
            (unsigned int)WS63_ZW101_VERIFY_TIMEOUT_RETRY_THRESHOLD);
        ws63_zw101_trace_verify_state("verify_timeout_retry");
        return 1U;
    }

    /* 超时重试耗尽后回落到普通失败上报，保留现有 lockout / 告警语义。 */
    g_ws63_zw101_verify_timeout_streak = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    osal_printk("[zw101] VERIFY timeout retry exhausted, fall back to fail\r\n");
    return 0U;
}

/**
 * @brief 重新初始化 LD2402。
 */
errcode_t ws63_task_ld2402_reinit(void)
{
    if (!ws63_is_subport_enabled(LD2402_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return ld2402_init(LD2402_SUBPORT);
}

/**
 * @brief 查询 LD2402 是否已完成初始化。
 */
uint8_t ws63_task_ld2402_is_ready(void)
{
    return ld2402_is_ready();
}

/**
 * @brief 查询 LD2402 当前是否处于配置模式。
 */
uint8_t ws63_task_ld2402_is_in_config_mode(void)
{
    return ld2402_is_in_config_mode();
}

/**
 * @brief 向 LD2402 发送原始命令帧。
 */
errcode_t ws63_task_ld2402_send_raw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (!ws63_is_subport_enabled(LD2402_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return wk2114_subport_write(LD2402_SUBPORT, data, len);
}

/**
 * @brief 读取 LD2402 固件版本。
 */
errcode_t ws63_task_ld2402_get_version(char *buf, uint16_t buf_len)
{
    return ld2402_get_version(buf, buf_len);
}

/**
 * @brief 读取 LD2402 字符形式序列号。
 */
errcode_t ws63_task_ld2402_get_sn_char(char *buf, uint16_t buf_len)
{
    return ld2402_get_sn_char(buf, buf_len);
}

/**
 * @brief 读取 LD2402 十六进制形式序列号。
 */
int32_t ws63_task_ld2402_get_sn_hex(uint8_t *buf, uint16_t buf_len)
{
    return ld2402_get_sn_hex(buf, buf_len);
}

/**
 * @brief 读取 LD2402 单个参数值。
 */
errcode_t ws63_task_ld2402_read_param(uint16_t param_id, uint32_t *value)
{
    return ld2402_read_param(param_id, value);
}

/**
 * @brief 写入 LD2402 单个参数值。
 */
errcode_t ws63_task_ld2402_set_param(uint16_t param_id, uint32_t value)
{
    return ld2402_set_param(param_id, value);
}

/**
 * @brief 设置 LD2402 最大距离。
 */
errcode_t ws63_task_ld2402_set_max_distance(float distance_m)
{
    return ld2402_set_max_distance(distance_m);
}

/**
 * @brief 设置 LD2402 目标消失延迟。
 */
errcode_t ws63_task_ld2402_set_disappear_delay(uint16_t seconds)
{
    return ld2402_set_disappear_delay(seconds);
}

/**
 * @brief 切换 LD2402 到正常模式。
 */
errcode_t ws63_task_ld2402_set_normal_mode(void)
{
    return ld2402_set_normal_mode();
}

/**
 * @brief 切换 LD2402 到工程模式。
 */
errcode_t ws63_task_ld2402_set_engineering_mode(void)
{
    return ld2402_set_engineering_mode();
}

/**
 * @brief 保存 LD2402 当前参数。
 */
errcode_t ws63_task_ld2402_save_params(void)
{
    return ld2402_save_params();
}

/**
 * @brief 触发 LD2402 自动增益调节。
 */
errcode_t ws63_task_ld2402_auto_gain_adjust(void)
{
    return ld2402_auto_gain_adjust();
}

/**
 * @brief 开始 LD2402 自动门限生成。
 */
errcode_t ws63_task_ld2402_start_auto_threshold(uint16_t trig_coef_10x,
    uint16_t hold_coef_10x, uint16_t static_coef_10x)
{
    return ld2402_start_auto_threshold(trig_coef_10x, hold_coef_10x, static_coef_10x);
}

/**
 * @brief 查询 LD2402 自动门限生成进度。
 */
errcode_t ws63_task_ld2402_get_auto_threshold_progress(uint16_t *progress_percent)
{
    return ld2402_get_auto_threshold_progress(progress_percent);
}

/**
 * @brief 查询 LD2402 自动门限干扰状态。
 */
errcode_t ws63_task_ld2402_get_auto_threshold_alarm(uint16_t *alarm_status, uint16_t *gate_bitmap)
{
    return ld2402_get_auto_threshold_alarm(alarm_status, gate_bitmap);
}

/**
 * @brief 读取 LD2402 电源干扰参数。
 */
errcode_t ws63_task_ld2402_get_power_interference(uint32_t *value)
{
    return ld2402_get_power_interference(value);
}

/**
 * @brief 执行 LD2402 0x003F 读后回写流程。
 */
errcode_t ws63_task_ld2402_refresh_save_flag(void)
{
    return ld2402_refresh_save_flag();
}

/**
 * @brief 设置 LD2402 运行态日志开关。
 */
errcode_t ws63_task_ld2402_set_log_enable(uint8_t enable)
{
    return ld2402_set_data_log_enable(enable);
}

/**
 * @brief 获取 LD2402 运行态日志开关。
 */
uint8_t ws63_task_ld2402_get_log_enable(void)
{
    return ld2402_get_data_log_enable();
}

/**
 * @brief 设置 LD2402 运行态日志最小输出间隔。
 */
errcode_t ws63_task_ld2402_set_log_gap_ms(uint32_t gap_ms)
{
    return ld2402_set_data_log_gap_ms(gap_ms);
}

/**
 * @brief 获取 LD2402 运行态日志最小输出间隔。
 */
uint32_t ws63_task_ld2402_get_log_gap_ms(void)
{
    return ld2402_get_data_log_gap_ms();
}

/**
 * @brief 获取 LD2402 最近一次解析到的距离值。
 */
int32_t ws63_task_ld2402_get_distance_mm(void)
{
    return ld2402_get_last_distance_mm();
}

/**
 * @brief 获取 LD2402 最近一次有效距离值的更新时间。
 */
uint32_t ws63_task_ld2402_get_distance_tick_ms(void)
{
    return ld2402_get_last_distance_tick_ms();
}

/**
 * @brief 设置 LD2402 子口通道使能状态。
 */
errcode_t ws63_task_ld2402_set_channel_enable(uint8_t enable)
{
    errcode_t ret;

    if (ws63_is_subport_config_enabled(LD2402_SUBPORT) == 0U) {
        return ERRCODE_FAIL;
    }

    ret = wk2114_subport_set_sleep(LD2402_SUBPORT, (enable != 0U) ? 0U : 1U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ws63_set_subport_runtime_enable(LD2402_SUBPORT, (enable != 0U) ? 1U : 0U);
}

/**
 * @brief 查询 LD2402 子口通道是否启用。
 */
uint8_t ws63_task_ld2402_is_channel_enabled(void)
{
    return ws63_is_subport_enabled(LD2402_SUBPORT);
}

/**
 * @brief 查询 ZW101 是否已完成初始化。
 */
uint8_t ws63_task_zw101_is_ready(void)
{
    return zw101_is_ready();
}

/**
 * @brief 通过 SLE 上行队列发送一条 ZW101 报警文本。
 */
static void ws63_task_zw101_notify_alarm(const char *alarm_text)
{
    ws63_sle_uplink_msg_t msg = {0};
    uint16_t text_len;

    if (alarm_text == NULL) {
        return;
    }

    text_len = (uint16_t)strlen(alarm_text);
    if ((text_len == 0U) || (text_len > WS63_TASK_QUEUE_PAYLOAD_MAX)) {
        return;
    }

    msg.sub_port = ZW101_SUBPORT;
    msg.len = text_len;
    if (memcpy_s(msg.data, sizeof(msg.data), alarm_text, text_len) != EOK) {
        return;
    }

    (void)ws63_task_post_sle_uplink(&msg, WS63_OS_NO_WAIT);
}

/**
 * @brief 请求 ZW101 VERIFY 任务执行一次认证。
 */
errcode_t ws63_task_zw101_request_verify(void)
{
    unsigned int irq_status;

    if (g_ws63_zw101_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_zw101_verify_disabled != 0U) {
        WS63_FINAL_IRQ_UNLOCK(irq_status);
        return ERRCODE_FAIL;
    }

    g_ws63_zw101_verify_requested = 1U;
    g_ws63_zw101_verify_cancelled = 0U;
    g_ws63_zw101_verify_wait_release = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("request_verify");

    /* VERIFY 请求一旦入队，就刷新认证窗口，给后续等待 ACK 预留时间。 */
    (void)ws63_lock_mgr_refresh_auth_window();
    return ERRCODE_SUCC;
}

errcode_t ws63_task_zw101_request_verify_after_release(void)
{
    unsigned int irq_status;

    if (g_ws63_zw101_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_zw101_verify_disabled != 0U) {
        WS63_FINAL_IRQ_UNLOCK(irq_status);
        return ERRCODE_FAIL;
    }

    /*
     * VERIFY 失败后统一进入 wait_release：
     * 每 0.3s 轮询一次 PS_GetImageInfo，只有 ACK=0x02（无手指）
     * 才允许排队下一次 VERIFY，避免连续空检把状态机打满。
     */
    ws63_zw101_reset_timeout_retry_state();
    g_ws63_zw101_verify_requested = 0U;
    g_ws63_zw101_verify_cancelled = 0U;
    g_ws63_zw101_verify_wait_release = 1U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("request_verify_after_release");
    (void)ws63_lock_mgr_refresh_auth_window();
    return ERRCODE_SUCC;
}

/**
 * @brief 重置当前 ARMED 窗口内的 ZW101 禁用保护状态。
 */
void ws63_task_zw101_reset_armed_window_guard(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_verify_disabled = 0U;
    g_ws63_zw101_verify_fail_streak = 0U;
    g_ws63_zw101_verify_timeout_streak = 0U;
    g_ws63_zw101_verify_wait_release = 0U;
    g_ws63_zw101_last_verify_ack = 0xFFU;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("reset_armed_window_guard");
}

/**
 * @brief 取消尚未开始的 ZW101 VERIFY 请求。
 */
void ws63_task_zw101_cancel_verify_request(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_verify_requested = 0U;
    g_ws63_zw101_verify_cancelled = 1U;
    g_ws63_zw101_verify_wait_release = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("cancel_verify");
}

/**
 * @brief 取消当前可能正在执行的 ZW101 VERIFY 流程。
 *
 * 说明：DEBUG INIT 会优先调用这个入口，先清掉任务层挂起请求，再向模组下发
 * CANCEL 命令，尽量把“排队中的 VERIFY”和“正在跑的 VERIFY”一起收口。
 */
errcode_t ws63_task_zw101_cancel_active_request(void)
{
    errcode_t ret;

    ws63_task_zw101_cancel_verify_request();

    if (zw101_is_ready() == 0U) {
        return ERRCODE_SUCC;
    }

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    {
        uint8_t ack_code = 0xFFU;

        ret = zw101_cancel(&ack_code);
        osal_printk("[zw101 trace] cancel_active ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)ack_code);
    }
#else
    ret = zw101_cancel(NULL);
#endif
    return ret;
}

/**
 * @brief 获取最近一次 VERIFY ACK。
 *
 * 用途：给 lock_mgr 提供失败类型判别依据（例如 NOT_PRESSED 是否计入锁定失败次数）。
 */
uint8_t ws63_task_zw101_get_last_verify_ack(void)
{
    unsigned int irq_status;
    uint8_t ack_code;

    irq_status = WS63_FINAL_IRQ_LOCK();
    ack_code = g_ws63_zw101_last_verify_ack;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return ack_code;
}

/**
 * @brief 判断 ACK 是否属于“认证失败可继续重试”的类型。
 */
static uint8_t ws63_zw101_is_verify_fail_ack(uint8_t ack_code)
{
    /*
    * NOT_PRESSED 视为“未触发有效按压”的可恢复状态：
    * - 仍允许在当前窗口继续重试；
    * - 不计入 ZW101 连续失败禁用计数。
    *
    * 超时重试已经耗尽；否则会先走“立即重拉 VERIFY”的快捷分支。
     */
    if ((ack_code == 0x08U) || (ack_code == 0x24U) ||  (ack_code == 0x09U)) {
        return 1U;
    }

    return 0U;
}

/**
 * @brief 向门锁结果链路上报一次 ZW101 VERIFY 结果。
 */
static void ws63_zw101_report_verify_result(errcode_t ret, uint8_t ack_code, uint16_t match_id, uint16_t score)
{
    uint8_t passed;
    uint8_t fail_disable = 0U;
    unsigned int irq_status;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    uint8_t fail_streak_after;
    uint8_t disabled_after;
    uint8_t timeout_retry_streak_after;
#endif

    passed = ((ret == ERRCODE_SUCC) && (ack_code == WS63_ZW101_ACK_OK)) ? 1U : 0U;

    /* 成功先把所有重试态清掉，避免下一轮生命周期继承上一轮的超时历史。 */
    if (passed != 0U) {
        ws63_zw101_reset_timeout_retry_state();
        (void)ws63_lock_mgr_refresh_auth_window();
    } else if ((ack_code == WS63_ZW101_ACK_TIMEOUT) && (ws63_zw101_try_retry_verify_after_timeout() != 0U)) {
        /*
         * ACK_TIMEOUT 在生命周期内被转换为内部重试时，不向门锁上报失败，避免
         * lock_mgr 把一次通信超时误判成终态失败并立刻发 Die。
         */
        return;
    } else {
        ws63_zw101_reset_timeout_retry_state();
        if (ack_code != WS63_ZW101_ACK_TIMEOUT) {
            (void)ws63_lock_mgr_refresh_auth_window();
        }
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_last_verify_ack = ack_code;
    if (passed != 0U) {
        g_ws63_zw101_verify_fail_streak = 0U;
    } else {
        if ((ws63_zw101_is_verify_fail_ack(ack_code) != 0U) ||
            ((ret != ERRCODE_SUCC) && (ack_code == WS63_ZW101_ACK_UNKNOWN))) {
            if (g_ws63_zw101_verify_fail_streak < 0xFFU) {
                g_ws63_zw101_verify_fail_streak++;
            }
        }

        if ((g_ws63_zw101_verify_fail_streak >= WS63_ZW101_VERIFY_FAIL_DISABLE_THRESHOLD) &&
            (g_ws63_zw101_verify_disabled == 0U)) {
            g_ws63_zw101_verify_disabled = 1U;
            fail_disable = 1U;
            g_ws63_zw101_verify_requested = 0U;
            g_ws63_zw101_verify_cancelled = 1U;
            g_ws63_zw101_verify_wait_release = 0U;
        }
    }

    WS63_FINAL_IRQ_UNLOCK(irq_status);

    if (passed != 0U) {
        /* 指纹通过时更新 lock_mgr 附加字段，供开锁成功事件拼接 finger_id/score。 */
        ws63_lock_mgr_update_finger_result(match_id, score);
        osal_printk("[zw101] VERIFY SUCCESS id=%u score=%u ack=0x%02x(%s)\r\n",
            (unsigned int)match_id,
            (unsigned int)score,
            (unsigned int)ack_code,
            ws63_zw101_ack_to_text(ack_code));
    } else {
        osal_printk("[zw101] VERIFY FAIL ret=0x%x ack=0x%02x(%s)\r\n",
            (unsigned int)ret,
            (unsigned int)ack_code,
            ws63_zw101_ack_to_text(ack_code));
    }

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    /* 详细结果快照只在打开追踪时输出，避免正常认证路径重复刷屏。 */
    fail_streak_after = g_ws63_zw101_verify_fail_streak;
    disabled_after = g_ws63_zw101_verify_disabled;
    timeout_retry_streak_after = g_ws63_zw101_verify_timeout_streak;
    osal_printk("[zw101 trace] verify_result ret=0x%x ack=0x%02x id=%u score=%u fail_streak=%u timeout_retry=%u disabled=%u\r\n",
        (unsigned int)ret,
        (unsigned int)ack_code,
        (unsigned int)match_id,
        (unsigned int)score,
        (unsigned int)fail_streak_after,
        (unsigned int)timeout_retry_streak_after,
        (unsigned int)disabled_after);
#endif

    if (fail_disable != 0U) {
        osal_printk("[zw101] VERIFY disabled after %u continuous failures\r\n",
            (unsigned int)WS63_ZW101_VERIFY_FAIL_DISABLE_THRESHOLD);
        ws63_task_zw101_notify_alarm(WS63_ZW101_ALARM_TEXT);
        (void)ws63_task_post_lock_event_text("result=locked;source=finger;reason=zw101_fail_5");
    }

    (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_ZW101, passed, ack_code);
}

/**
 * @brief ZW101 VERIFY 任务入口。
 */
static void *ws63_zw101_task_entry(const char *arg)
{
    (void)arg;

    /* 等待原因码用于去重日志，避免在空闲轮询时刷屏。 */
    uint8_t last_wait_reason = 0xFFU;
    uint32_t last_ready_retry_ms = 0U;
    uint32_t last_release_check_ms = 0U;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    uint8_t last_release_finger_present = 0xFFU;
    uint32_t last_release_check_ret = 0xFFFFFFFFU;
#endif

    while (1) {
        uint8_t request;
        uint8_t cancelled;
        uint8_t wait_release;
        uint8_t disabled;
        uint8_t wait_reason = 0U;

        {
            unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
            request = g_ws63_zw101_verify_requested;
            cancelled = g_ws63_zw101_verify_cancelled;
            wait_release = g_ws63_zw101_verify_wait_release;
            disabled = g_ws63_zw101_verify_disabled;
            WS63_FINAL_IRQ_UNLOCK(irq_status);
        }

        if (wait_release != 0U) {
            if (last_wait_reason != 5U) {
                ws63_zw101_trace_verify_state("task_wait_finger_release");
                last_wait_reason = 5U;
                last_release_check_ms = 0U;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
                last_release_finger_present = 0xFFU;
                last_release_check_ret = 0xFFFFFFFFU;
#endif
            }

            if ((disabled == 0U) && (cancelled == 0U) && (ws63_lock_mgr_is_armed() != 0U) &&
                (ws63_task_zw101_is_ready() != 0U)) {
                uint32_t now_ms = ws63_os_tick_ms();
                if ((uint32_t)(now_ms - last_release_check_ms) >= WS63_ZW101_RELEASE_CHECK_GAP_MS) {
                    uint8_t finger_present = 1U;
                    uint8_t ack_code = 0xFFU;
                    errcode_t check_ret;

                    last_release_check_ms = now_ms;
                    check_ret = zw101_check_finger_present(&finger_present, &ack_code);

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
                    /* 仅在状态变化时打印，避免 wait_release 期间固定节拍日志刷屏。 */
                    if (((uint32_t)check_ret != last_release_check_ret) ||
                        (finger_present != last_release_finger_present)) {
                        osal_printk("[zw101 trace] release_check ret=0x%x ack=0x%02x finger=%s\r\n",
                            (unsigned int)check_ret,
                            (unsigned int)ack_code,
                            ws63_zw101_finger_text(finger_present));
                        last_release_check_ret = (uint32_t)check_ret;
                        last_release_finger_present = finger_present;
                    }
#endif

                    if ((check_ret == ERRCODE_SUCC) && (finger_present == 0U)) {
                        unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
                        g_ws63_zw101_verify_wait_release = 0U;
                        g_ws63_zw101_verify_requested = 1U;
                        g_ws63_zw101_verify_cancelled = 0U;
                        WS63_FINAL_IRQ_UNLOCK(irq_status);

                        osal_printk("[zw101] finger released, retry verify queued\r\n");
                        ws63_zw101_trace_verify_state("retry_verify_after_release");
                        (void)ws63_lock_mgr_refresh_auth_window();
                    }
                }
            }

            ws63_os_sleep_ms(20U);
            continue;
        }

        if (request == 0U) {
            wait_reason = 1U;
        } else if (cancelled != 0U) {
            wait_reason = 2U;
        } else if (disabled != 0U) {
            wait_reason = 3U;
        }

        if (wait_reason != 0U) {
            if (wait_reason != last_wait_reason) {
                ws63_zw101_trace_verify_state("task_wait_request_or_disabled");
                last_wait_reason = wait_reason;
            }
            ws63_os_sleep_ms(20U);
            continue;
        }

        if ((ws63_task_zw101_is_ready() == 0U) || (ws63_lock_mgr_is_armed() == 0U)) {
            if (last_wait_reason != 4U) {
                ws63_zw101_trace_verify_state("task_wait_ready_or_armed");
                last_wait_reason = 4U;
            }

            /*
             * 自愈重试：当处于 ARMED 且已有 VERIFY 请求，但设备仍未 ready 时，
             * 周期调用 ensure_zw101_ready，避免单次惰性初始化失败后整窗超时。
             */
            if ((request != 0U) && (cancelled == 0U) && (disabled == 0U) && (ws63_lock_mgr_is_armed() != 0U) &&
                (ws63_task_zw101_is_ready() == 0U)) {
                uint32_t now_ms = ws63_os_tick_ms();
                if ((uint32_t)(now_ms - last_ready_retry_ms) >= WS63_ZW101_READY_RETRY_GAP_MS) {
                    last_ready_retry_ms = now_ms;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
                    {
                        errcode_t recover_ret;

                        recover_ret = ws63_task_ensure_zw101_ready();
                        osal_printk("[zw101 trace] ready_recover ret=0x%x\r\n", (unsigned int)recover_ret);
                    }
#else
                    (void)ws63_task_ensure_zw101_ready();
#endif
                }
            }

            ws63_os_sleep_ms(20U);
            continue;
        }

        last_wait_reason = 0U;

        {
            uint16_t match_id = 0U;
            uint16_t score = 0U;
            uint8_t ack_code = 0xFFU;
            errcode_t ret;

            /* 先清请求位，再进入阻塞式 VERIFY，避免同一窗口内重复并发调用。 */
            ws63_task_zw101_cancel_verify_request();

            /* VERIFY 开始前刷新窗口，防止长耗时认证把唤醒态提前打断。 */
            (void)ws63_lock_mgr_refresh_auth_window();

            osal_printk("[zw101] VERIFYING level=%u target=0x%04X param=0x%04X\r\n",
                (unsigned int)WS63_ZW101_VERIFY_LEVEL_DEFAULT,
                (unsigned int)WS63_ZW101_VERIFY_ID_DEFAULT,
                (unsigned int)WS63_ZW101_VERIFY_PARAM_DEFAULT);
            ws63_zw101_trace_verify_state("task_before_verify");
            ret = zw101_verify(WS63_ZW101_VERIFY_LEVEL_DEFAULT,
                WS63_ZW101_VERIFY_ID_DEFAULT,
                WS63_ZW101_VERIFY_PARAM_DEFAULT,
                &match_id,
                &score,
                &ack_code);
            ws63_zw101_report_verify_result(ret, ack_code, match_id, score);
            ws63_zw101_trace_verify_state("task_after_verify");
        }
    }

    return NULL;
}

/**
 * @brief 启动 ZW101 VERIFY 任务。
 */
errcode_t ws63_zw101_task_start(void)
{
    errcode_t ret;

    if (g_ws63_zw101_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_start_task("ws63_zw101_task",
        ws63_zw101_task_entry,
        0U,
        WS63_ZW101_TASK_STACK_SIZE,
        WS63_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[zw101] task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_zw101_task_started = 1U;
    g_ws63_zw101_verify_requested = 0U;
    g_ws63_zw101_verify_cancelled = 0U;
    g_ws63_zw101_verify_wait_release = 0U;
    g_ws63_zw101_verify_disabled = 0U;
    g_ws63_zw101_verify_fail_streak = 0U;
    g_ws63_zw101_verify_timeout_streak = 0U;
    g_ws63_zw101_last_verify_ack = 0xFFU;
    return ERRCODE_SUCC;
}

/**
 * @brief 设置 SLE 上行 success 日志开关。
 */
errcode_t ws63_task_sle_uplink_log_set_enable(uint8_t enable)
{
    return ws63_sle_set_uplink_success_log_enable(enable);
}

/**
 * @brief 获取 SLE 上行 success 日志开关。
 */
uint8_t ws63_task_sle_uplink_log_get_enable(void)
{
    return ws63_sle_get_uplink_success_log_enable();
}

/**
 * @brief 设置 SLE 上行 success 日志最小输出间隔。
 */
errcode_t ws63_task_sle_uplink_log_set_gap_ms(uint32_t gap_ms)
{
    return ws63_sle_set_uplink_success_log_gap_ms(gap_ms);
}

/**
 * @brief 获取 SLE 上行 success 日志最小输出间隔。
 */
uint32_t ws63_task_sle_uplink_log_get_gap_ms(void)
{
    return ws63_sle_get_uplink_success_log_gap_ms();
}

/**
 * @brief 重新初始化 ZW101（触发握手检测）。
 */
errcode_t ws63_task_zw101_reinit(void)
{
    errcode_t ret;

    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_init(ZW101_SUBPORT);
    if (ret == ERRCODE_SUCC) {
        unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
        g_ws63_zw101_verify_disabled = 0U;
        g_ws63_zw101_verify_fail_streak = 0U;
        g_ws63_zw101_verify_requested = 0U;
        g_ws63_zw101_verify_cancelled = 0U;
        g_ws63_zw101_verify_wait_release = 0U;
        g_ws63_zw101_last_verify_ack = 0xFFU;
        WS63_FINAL_IRQ_UNLOCK(irq_status);
    }

    return ret;
}

/**
 * @brief 执行 ZW101 ECHO 命令。
 */
errcode_t ws63_task_zw101_echo(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_echo(ack_out);
}

/**
 * @brief 执行 ZW101 自动验证（VERIFY）。
 */
errcode_t ws63_task_zw101_verify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_verify(score_level,
        target_id,
        param_flags,
        match_id_out,
        score_out,
        ack_out);
}

/**
 * @brief 执行 ZW101 自动注册（ENROLL）。
 */
errcode_t ws63_task_zw101_enroll(uint16_t page_id,
    uint8_t enroll_times,
    uint16_t param_flags,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_enroll(page_id, enroll_times, param_flags, ack_out);
}

/**
 * @brief 查询 ZW101 有效模板数量（LIST）。
 */
errcode_t ws63_task_zw101_list(uint16_t *valid_num_out, uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_list(valid_num_out, ack_out);
}

/**
 * @brief 璇诲彇 ZW101 绱㈠紩琛?
 */
errcode_t ws63_task_zw101_read_index_table(uint8_t page, uint8_t *index_buf_out, uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_read_index_table(page, index_buf_out, ack_out);
}

/**
 * @brief 删除 ZW101 模板（DEL）。
 */
errcode_t ws63_task_zw101_delete(uint16_t page_id,
    uint16_t count,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_delete(page_id, count, ack_out);
}

/**
 * @brief 清空 ZW101 模板库（CLEAR）。
 */
errcode_t ws63_task_zw101_clear(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_clear(ack_out);
}

/**
 * @brief 取消 ZW101 当前流程（CANCEL）。
 */
errcode_t ws63_task_zw101_cancel(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_cancel(ack_out);
}
