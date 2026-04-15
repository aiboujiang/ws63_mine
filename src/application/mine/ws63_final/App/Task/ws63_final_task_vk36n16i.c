/**
 * @file ws63_final_task_vk36n16i.c
 * @brief Task 层 VK36N16I 子模块实现（独立任务 + 状态机）。
 */

#include "ws63_final_task_internal.h"

#include "osal_debug.h"
#include "securec.h"

#include <string.h>

#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_vk36n16i.h"

#if (WS63_VK36N16I_ENABLE == 1U)
/**
 * @brief VK36N16I 任务状态机。
 */
typedef enum {
    WS63_VK36N16I_STATE_INIT = 0,
    WS63_VK36N16I_STATE_DISABLED,
    WS63_VK36N16I_STATE_READY,
    WS63_VK36N16I_STATE_FAULT
} ws63_vk36n16i_state_t;

/* 任务运行状态。 */
static uint8_t g_ws63_vk36n16i_task_started = 0U;
static uint8_t g_ws63_vk36n16i_ready = 0U;
static uint8_t g_ws63_vk36n16i_enable = WS63_VK36N16I_ENABLE_DEFAULT;
static uint8_t g_ws63_vk36n16i_multi_alarm_enable = WS63_VK36N16I_MULTI_KEY_ALARM_DEFAULT;
static uint8_t g_ws63_vk36n16i_multi_alarm_active = 0U;
static uint8_t g_ws63_vk36n16i_force_reinit = 0U;
static ws63_vk36n16i_state_t g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_INIT;
static ws63_vk36n16i_sample_t g_ws63_vk36n16i_last_sample = {
    .raw_code = 0x0000U,
    .pressed_mask = 0x0000U,
    .pressed_count = 0U,
    .multi_key = 0U
};
static uint8_t g_ws63_vk36n16i_password_session_active = 0U;
static uint8_t g_ws63_vk36n16i_password_len = 0U;
static uint8_t g_ws63_vk36n16i_password_overflow = 0U;
static uint8_t g_ws63_vk36n16i_password_fail_streak = 0U;
static uint8_t g_ws63_vk36n16i_password_disabled = 0U;
static uint16_t g_ws63_vk36n16i_password_last_mask = 0U;
/* 已按下但尚未在抬起时提交的单键缓存。 */
static uint16_t g_ws63_vk36n16i_password_pending_mask = 0U;
static uint32_t g_ws63_vk36n16i_password_deadline_ms = 0U;
static char g_ws63_vk36n16i_password_buf[WS63_VK36N16I_PASSWORD_LEN + 1U] = {0};

/*
 * 实测得到的按键位图映射。
 *
 * 说明：VK36N16I 仍然以 raw 位图为准，这里只负责把物理按键标签映射成
 * 可读文本，便于调试输出和现场联调。
 */
typedef struct {
    uint16_t mask;
    const char *label;
} ws63_vk36n16i_key_map_t;

static const ws63_vk36n16i_key_map_t g_ws63_vk36n16i_key_map[] = {
    {0x1000U, "A"},
    {0x2000U, "B"},
    {0x4000U, "C"},
    {0x8000U, "D"},
    {0x0001U, "1"},
    {0x0002U, "4"},
    {0x0004U, "7"},
    {0x0008U, "*"},
    {0x0010U, "2"},
    {0x0020U, "5"},
    {0x0040U, "8"},
    {0x0080U, "0"},
    {0x0100U, "3"},
    {0x0200U, "6"},
    {0x0400U, "9"},
    {0x0800U, "#"}
};

/**
 * @brief 将按键位图格式化成可读文本。
 *
 * @param pressed_mask 位图（位为 1 表示按下）。
 * @param text 输出缓冲区。
 * @param text_len 输出缓冲区长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
static errcode_t ws63_vk36n16i_format_pressed_text(uint16_t pressed_mask, char *text, uint16_t text_len)
{
    uint16_t known_mask = 0U;
    uint16_t used_len = 0U;
    uint16_t unknown_mask;
    uint16_t i;
    int32_t ret;

    if ((text == NULL) || (text_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    text[0] = '\0';

    for (i = 0U; i < (uint16_t)(sizeof(g_ws63_vk36n16i_key_map) / sizeof(g_ws63_vk36n16i_key_map[0])); i++) {
        const ws63_vk36n16i_key_map_t *map = &g_ws63_vk36n16i_key_map[i];

        known_mask = (uint16_t)(known_mask | map->mask);
        if ((pressed_mask & map->mask) == 0U) {
            continue;
        }

        if (used_len >= text_len) {
            return ERRCODE_INVALID_PARAM;
        }

        /* 先输出分隔符，再输出标签，保证多键结果按 A+B 这种形式展示。 */
        ret = snprintf_s(text + used_len,
            (size_t)(text_len - used_len),
            (size_t)(text_len - used_len - 1U),
            "%s%s",
            (used_len == 0U) ? "" : "+",
            map->label);
        if (ret < 0) {
            return ERRCODE_FAIL;
        }

        used_len = (uint16_t)(used_len + (uint16_t)ret);
    }

    unknown_mask = (uint16_t)(pressed_mask & (uint16_t)(~known_mask));
    if (unknown_mask != 0U) {
        if (used_len >= text_len) {
            return ERRCODE_INVALID_PARAM;
        }

        /* 未知位仍然按十六进制保留，避免现场调试时丢失原始信息。 */
        ret = snprintf_s(text + used_len,
            (size_t)(text_len - used_len),
            (size_t)(text_len - used_len - 1U),
            "%s0x%04x",
            (used_len == 0U) ? "" : "+",
            (unsigned int)unknown_mask);
        if (ret < 0) {
            return ERRCODE_FAIL;
        }

        used_len = (uint16_t)(used_len + (uint16_t)ret);
    }

    if (used_len == 0U) {
        ret = snprintf_s(text, (size_t)text_len, (size_t)(text_len - 1U), "NONE");
        if (ret < 0) {
            return ERRCODE_FAIL;
        }
    }

    return ERRCODE_SUCC;
}

/**
 * @brief VK36N16I 状态锁。
 */

/**
 * @brief VK36N16I 状态解锁。
 */

/**
 * @brief 更新最新采样缓存。
 */
static void ws63_vk36n16i_update_sample(const ws63_vk36n16i_sample_t *sample)
{
    unsigned int irq_status;

    if (sample == NULL) {
        return;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_last_sample = *sample;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief 清空采样缓存为“无按键”状态。
 */
static void ws63_vk36n16i_reset_sample_cache(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_last_sample.raw_code = 0x0000U;
    g_ws63_vk36n16i_last_sample.pressed_mask = 0x0000U;
    g_ws63_vk36n16i_last_sample.pressed_count = 0U;
    g_ws63_vk36n16i_last_sample.multi_key = 0U;
    g_ws63_vk36n16i_multi_alarm_active = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief 更新就绪标记。
 */
static void ws63_vk36n16i_set_ready(uint8_t ready)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_ready = (ready != 0U) ? 1U : 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief 清空 VK36N16I 密码输入缓存。
 */
static void ws63_vk36n16i_password_reset(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    (void)memset_s(g_ws63_vk36n16i_password_buf,
        sizeof(g_ws63_vk36n16i_password_buf),
        0,
        sizeof(g_ws63_vk36n16i_password_buf));
    g_ws63_vk36n16i_password_len = 0U;
    g_ws63_vk36n16i_password_overflow = 0U;
    g_ws63_vk36n16i_password_last_mask = 0U;
    g_ws63_vk36n16i_password_pending_mask = 0U;
    g_ws63_vk36n16i_password_deadline_ms = 0U;
    g_ws63_vk36n16i_password_session_active = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief 开启一次新的密码输入会话。
 */
static void ws63_vk36n16i_password_start_session(void)
{
    unsigned int irq_status;
    uint32_t now_ms;

    now_ms = ws63_os_tick_ms();
    irq_status = WS63_FINAL_IRQ_LOCK();
    (void)memset_s(g_ws63_vk36n16i_password_buf,
        sizeof(g_ws63_vk36n16i_password_buf),
        0,
        sizeof(g_ws63_vk36n16i_password_buf));
    g_ws63_vk36n16i_password_len = 0U;
    g_ws63_vk36n16i_password_overflow = 0U;
    g_ws63_vk36n16i_password_last_mask = 0U;
    g_ws63_vk36n16i_password_pending_mask = 0U;
    g_ws63_vk36n16i_password_deadline_ms = now_ms + WS63_LOCK_AUTH_WINDOW_MS_DEFAULT;
    g_ws63_vk36n16i_password_session_active = 1U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief 停止当前密码输入会话。
 */
static void ws63_vk36n16i_password_stop_session(void)
{
    ws63_vk36n16i_password_reset();
}

/**
 * @brief 重置 VK36N16I 在当前 armed 周期内的失败封禁状态。
 *
 * 说明：一旦门锁退出 armed，失败封禁就只应保留到本次输入周期结束，不应
 * 跨到下一轮接近唤醒窗口。
 */
static void ws63_vk36n16i_password_reset_cycle_guard(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_password_fail_streak = 0U;
    g_ws63_vk36n16i_password_disabled = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    ws63_vk36n16i_password_stop_session();
}

/**
 * @brief 播放按键轻提示音。
 */
static void ws63_vk36n16i_password_beep(void)
{
    (void)ws63_task_buzzer_beep_tone(WS63_VK36N16I_KEY_PROMPT_BEEP_FREQ_HZ,
        WS63_VK36N16I_KEY_PROMPT_BEEP_VOLUME_PERCENT,
        WS63_VK36N16I_KEY_PROMPT_BEEP_MS);
}

/**
 * @brief 刷新 VK36N16I 触发的 armed 续命窗口并打印显式日志。
 *
 * 说明：仅在“抬起后提交成功”时调用，避免按住采样阶段刷屏。
 *
 * @param pressed_mask 本次成功提交的按键位图。
 * @param key_text 本次成功提交的可读按键文本。
 * @param now_ms 当前时间戳，用于同步更新 VK36N16I 密码会话 deadline。
 */
static void ws63_vk36n16i_log_auth_window_renewal(uint16_t pressed_mask, const char *key_text, uint32_t now_ms)
{
    unsigned int irq_status;
    uint32_t deadline_before_ms;
    uint32_t deadline_after_ms;

    if (key_text == NULL) {
        return;
    }

    deadline_before_ms = ws63_lock_mgr_get_auth_window_deadline_ms();
    if (ws63_lock_mgr_refresh_auth_window() != ERRCODE_SUCC) {
        return;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_vk36n16i_password_session_active != 0U) {
        g_ws63_vk36n16i_password_deadline_ms = now_ms + WS63_LOCK_AUTH_WINDOW_MS_DEFAULT;
    }
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    deadline_after_ms = ws63_lock_mgr_get_auth_window_deadline_ms();
    osal_printk("[wk2114 final task] VK36N16I armed renew key=%s mask=0x%04x deadline=%u->%u\r\n",
        key_text,
        (unsigned int)pressed_mask,
        (unsigned int)deadline_before_ms,
        (unsigned int)deadline_after_ms);
}

/**
 * @brief 将已按下但尚未提交的 VK36N16I 键值写入密码缓存。
 *
 * 说明：这里只在抬起态处理提交结果，避免长按或持续采样导致重复入码。
 * 数字键在成功入库后才会续命并打印日志；`#` 则在成功校验后再补一次续命。
 *
 * @param pending_mask 已缓存的单键位图。
 * @param armed 当前是否处于门锁接近唤醒窗口。
 * @param now_ms 当前时间戳，用于同步更新密码会话 deadline。
 */
static void ws63_vk36n16i_password_commit_pending(uint16_t pending_mask, uint8_t armed, uint32_t now_ms)
{
    char key_text[8] = {0};
    uint8_t passed = 0U;

    if (pending_mask == 0U) {
        return;
    }

    if (ws63_vk36n16i_format_pressed_text(pending_mask, key_text, sizeof(key_text)) != ERRCODE_SUCC) {
        return;
    }

    if ((key_text[0] >= '0') && (key_text[0] <= '9') && (key_text[1] == '\0')) {
        if ((g_ws63_vk36n16i_password_len < WS63_VK36N16I_PASSWORD_LEN) && (g_ws63_vk36n16i_password_overflow == 0U)) {
            g_ws63_vk36n16i_password_buf[g_ws63_vk36n16i_password_len] = key_text[0];
            g_ws63_vk36n16i_password_len++;
            g_ws63_vk36n16i_password_buf[g_ws63_vk36n16i_password_len] = '\0';
            if (armed != 0U) {
                ws63_vk36n16i_log_auth_window_renewal(pending_mask, key_text, now_ms);
            }
        } else {
            g_ws63_vk36n16i_password_overflow = 1U;
        }
        return;
    }

    if ((key_text[0] != '#') || (key_text[1] != '\0')) {
        return;
    }

    if (armed == 0U) {
        return;
    }

    if ((g_ws63_vk36n16i_password_overflow == 0U) &&
        (g_ws63_vk36n16i_password_len == WS63_VK36N16I_PASSWORD_LEN) &&
        (strcmp(g_ws63_vk36n16i_password_buf, WS63_VK36N16I_PASSWORD_TEXT) == 0)) {
        passed = 1U;
    }

    osal_printk("[wk2114 final task] VK36N16I password %s, input=%s\r\n",
        (passed != 0U) ? "pass" : "fail",
        g_ws63_vk36n16i_password_buf);
    if (passed != 0U) {
        unsigned int irq_status;

        irq_status = WS63_FINAL_IRQ_LOCK();
        g_ws63_vk36n16i_password_fail_streak = 0U;
        g_ws63_vk36n16i_password_disabled = 0U;
        WS63_FINAL_IRQ_UNLOCK(irq_status);
        ws63_vk36n16i_log_auth_window_renewal(pending_mask, key_text, now_ms);
    } else {
        unsigned int irq_status;

        /*
         * 失败声光提示由 lock_mgr 统一触发：
         * 1) 按键与指纹失败行为保持一致；
         * 2) 避免来源层和编排层双重蜂鸣。
         */
        irq_status = WS63_FINAL_IRQ_LOCK();
        if (g_ws63_vk36n16i_password_fail_streak < 0xFFU) {
            g_ws63_vk36n16i_password_fail_streak++;
        }
        if ((g_ws63_vk36n16i_password_fail_streak >= WS63_VK36N16I_PASSWORD_FAIL_DISABLE_THRESHOLD) &&
            (g_ws63_vk36n16i_password_disabled == 0U)) {
            g_ws63_vk36n16i_password_disabled = 1U;
            osal_printk("[wk2114 final task] VK36N16I password disabled after %u continuous failures\r\n",
                (unsigned int)WS63_VK36N16I_PASSWORD_FAIL_DISABLE_THRESHOLD);
            (void)ws63_task_post_lock_event_text("result=locked;source=key;reason=vk36n16i_fail_5");
        }
        WS63_FINAL_IRQ_UNLOCK(irq_status);
    }
    (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_VK36N16I, passed, 0xFFU);
    ws63_vk36n16i_password_stop_session();
}

/**
 * @brief 处理 VK36N16I 密码输入。
 *
 * 关键流程：
 * 1) 按下时只做蜂鸣提示，不再续命；
 * 2) 抬起时将缓存的单键提交到密码缓冲区，成功后再续命；
 * 3) `#` 只在抬起时触发最终校验，校验成功后补一次续命，避免长按重复提交。
 */
static void ws63_vk36n16i_handle_password_input(const ws63_vk36n16i_sample_t *sample)
{
    uint8_t armed;
    uint32_t now_ms;

    if (sample == NULL) {
        return;
    }

    now_ms = ws63_os_tick_ms();
    armed = ws63_lock_mgr_is_armed();

    /* 当前不在 armed 窗口时，失败封禁必须随窗口一起收口，避免影响下一轮输入。 */
    if (armed == 0U) {
        if ((g_ws63_vk36n16i_password_fail_streak != 0U) || (g_ws63_vk36n16i_password_disabled != 0U)) {
            ws63_vk36n16i_password_reset_cycle_guard();
        } else {
            ws63_vk36n16i_password_stop_session();
        }
        return;
    }

    if (g_ws63_vk36n16i_password_disabled != 0U) {
        ws63_vk36n16i_password_stop_session();
        return;
    }

    if ((g_ws63_vk36n16i_password_session_active != 0U) &&
        (g_ws63_vk36n16i_password_deadline_ms != 0U) &&
        (now_ms >= g_ws63_vk36n16i_password_deadline_ms)) {
        ws63_vk36n16i_password_stop_session();
    }

    if (sample->pressed_mask == 0U) {
        if (g_ws63_vk36n16i_password_pending_mask != 0U) {
            ws63_vk36n16i_password_commit_pending(g_ws63_vk36n16i_password_pending_mask, armed, now_ms);
        }

        /* 已进入抬起态后清空最后一次采样，下一次按下会重新生成边沿。 */
        g_ws63_vk36n16i_password_last_mask = 0U;
        g_ws63_vk36n16i_password_pending_mask = 0U;
        return;
    }

    if (g_ws63_vk36n16i_password_session_active == 0U) {
        ws63_vk36n16i_password_start_session();
    }

    if (sample->pressed_mask == g_ws63_vk36n16i_password_last_mask) {
        return;
    }

    /* 只在采样到新的单键按下时缓存一次，长按不会重复入码。 */
    g_ws63_vk36n16i_password_last_mask = sample->pressed_mask;

    if (sample->pressed_count != 1U) {
        g_ws63_vk36n16i_password_pending_mask = 0U;
        return;
    }

    g_ws63_vk36n16i_password_pending_mask = sample->pressed_mask;
    ws63_vk36n16i_password_beep();
}

/**
 * @brief 查询 VK36N16I 当前 armed 周期是否已被失败封禁。
 */
uint8_t ws63_task_vk36n16i_is_password_disabled(void)
{
    unsigned int irq_status;
    uint8_t disabled;

    irq_status = WS63_FINAL_IRQ_LOCK();
    disabled = g_ws63_vk36n16i_password_disabled;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return disabled;
}

/**
 * @brief 处理多键报警状态变化，避免持续重复刷屏。
 */
static void ws63_vk36n16i_handle_alarm_transition(const ws63_vk36n16i_sample_t *sample)
{
    unsigned int irq_status;
    uint8_t alarm_enable;
    uint8_t alarm_active;
    char key_text[64] = {0};

    if (sample == NULL) {
        return;
    }

    if (ws63_vk36n16i_format_pressed_text(sample->pressed_mask, key_text, sizeof(key_text)) != ERRCODE_SUCC) {
        (void)strncpy_s(key_text, sizeof(key_text), "ERR", 3U);
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    alarm_enable = g_ws63_vk36n16i_multi_alarm_enable;
    alarm_active = g_ws63_vk36n16i_multi_alarm_active;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    if (alarm_enable == 0U) {
        if (alarm_active != 0U) {
            irq_status = WS63_FINAL_IRQ_LOCK();
            g_ws63_vk36n16i_multi_alarm_active = 0U;
            WS63_FINAL_IRQ_UNLOCK(irq_status);
        }
        return;
    }

    if ((sample->multi_key != 0U) && (alarm_active == 0U)) {
        osal_printk("[wk2114 final task] VK36N16I multi-key alarm mask=0x%04x count=%u keys=%s\r\n",
            (unsigned int)sample->pressed_mask,
            (unsigned int)sample->pressed_count,
            key_text);
        irq_status = WS63_FINAL_IRQ_LOCK();
        g_ws63_vk36n16i_multi_alarm_active = 1U;
        WS63_FINAL_IRQ_UNLOCK(irq_status);
        return;
    }

    if ((sample->multi_key == 0U) && (alarm_active != 0U)) {
        osal_printk("[wk2114 final task] VK36N16I multi-key alarm clear\r\n");
        irq_status = WS63_FINAL_IRQ_LOCK();
        g_ws63_vk36n16i_multi_alarm_active = 0U;
        WS63_FINAL_IRQ_UNLOCK(irq_status);
    }
}

/**
 * @brief VK36N16I 独立任务入口。
 *
 * 状态机说明：
 * 1) INIT: 初始化硬件与驱动；
 * 2) DISABLED: 任务保活但不采样；
 * 3) READY: 周期采样并更新缓存；
 * 4) FAULT: I2C 读取失败后退避重试。
 */
static void *ws63_vk36n16i_task_entry(const char *arg)
{
    (void)arg;

    ws63_vk36n16i_reset_sample_cache();
    ws63_vk36n16i_password_stop_session();
    ws63_vk36n16i_set_ready(0U);

    while (1) {
        uint8_t enabled;
        uint8_t force_reinit;

        {
            unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
            enabled = g_ws63_vk36n16i_enable;
            force_reinit = g_ws63_vk36n16i_force_reinit;
            if (force_reinit != 0U) {
                g_ws63_vk36n16i_force_reinit = 0U;
            }
            WS63_FINAL_IRQ_UNLOCK(irq_status);
        }

        if (force_reinit != 0U) {
            g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_INIT;
            ws63_vk36n16i_set_ready(0U);
            ws63_vk36n16i_reset_sample_cache();
            ws63_vk36n16i_password_stop_session();
        }

        switch (g_ws63_vk36n16i_state) {
            case WS63_VK36N16I_STATE_INIT: {
                errcode_t ret;

                ret = ws63_vk36n16i_init();
                if (ret == ERRCODE_SUCC) {
                    ws63_vk36n16i_set_ready(1U);
                    g_ws63_vk36n16i_state = (enabled != 0U) ? WS63_VK36N16I_STATE_READY : WS63_VK36N16I_STATE_DISABLED;
                    osal_printk("[wk2114 final task] VK36N16I init ok (I2C SCL=GPIO16 SDA=GPIO15)\r\n");
                } else {
                    ws63_vk36n16i_set_ready(0U);
                    g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_FAULT;
                    osal_printk("[wk2114 final task] VK36N16I init fail, ret=0x%x\r\n", (unsigned int)ret);
                }
                break;
            }
            case WS63_VK36N16I_STATE_DISABLED:
                if (enabled != 0U) {
                    g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_INIT;
                }
                ws63_vk36n16i_password_stop_session();
                ws63_os_sleep_ms(WS63_VK36N16I_TASK_POLL_MS);
                break;
            case WS63_VK36N16I_STATE_READY:
                if (enabled == 0U) {
                    g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_DISABLED;
                    ws63_vk36n16i_reset_sample_cache();
                    ws63_vk36n16i_password_stop_session();
                    ws63_os_sleep_ms(WS63_VK36N16I_TASK_POLL_MS);
                    break;
                }

                {
                    ws63_vk36n16i_sample_t sample;
                    errcode_t ret;

                    ret = ws63_vk36n16i_read_sample(&sample);
                    if (ret != ERRCODE_SUCC) {
                        ws63_vk36n16i_set_ready(0U);
                        g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_FAULT;
                        osal_printk("[wk2114 final task] VK36N16I I2C read fail, ret=0x%x\r\n", (unsigned int)ret);
                        ws63_os_sleep_ms(WS63_VK36N16I_INIT_RETRY_MS);
                        break;
                    }

                    ws63_vk36n16i_update_sample(&sample);
                    ws63_vk36n16i_handle_alarm_transition(&sample);
                    ws63_vk36n16i_handle_password_input(&sample);
                }

                ws63_os_sleep_ms(WS63_VK36N16I_TASK_POLL_MS);
                break;
            case WS63_VK36N16I_STATE_FAULT:
            default:
                if (enabled == 0U) {
                    g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_DISABLED;
                    ws63_os_sleep_ms(WS63_VK36N16I_TASK_POLL_MS);
                } else {
                    ws63_os_sleep_ms(WS63_VK36N16I_INIT_RETRY_MS);
                    g_ws63_vk36n16i_state = WS63_VK36N16I_STATE_INIT;
                }
                break;
        }
    }

    return NULL;
}
#endif

/**
 * @brief 启动 VK36N16I 独立任务。
 */
errcode_t ws63_vk36n16i_task_start(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    if (g_ws63_vk36n16i_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_start_task("ws63_vk36n16i_task",
        ws63_vk36n16i_task_entry,
        0U,
        WS63_VK36N16I_TASK_STACK_SIZE,
        WS63_VK36N16I_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] VK36N16I task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_vk36n16i_task_started = 1U;
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 触发 VK36N16I 重初始化。
 */
errcode_t ws63_task_vk36n16i_reinit(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;

    if (g_ws63_vk36n16i_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_force_reinit = 1U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 设置 VK36N16I 状态机使能。
 */
errcode_t ws63_task_vk36n16i_set_enable(uint8_t enable)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    (void)enable;
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;

    if (g_ws63_vk36n16i_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_enable = (enable != 0U) ? 1U : 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 查询 VK36N16I 是否就绪。
 */
uint8_t ws63_task_vk36n16i_is_ready(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t ready;

    irq_status = WS63_FINAL_IRQ_LOCK();
    ready = g_ws63_vk36n16i_ready;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return ready;
#endif
}

/**
 * @brief 查询 VK36N16I 状态机使能状态。
 */
uint8_t ws63_task_vk36n16i_is_enabled(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t enabled;

    irq_status = WS63_FINAL_IRQ_LOCK();
    enabled = g_ws63_vk36n16i_enable;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return enabled;
#endif
}

/**
 * @brief 设置多键报警开关。
 */
errcode_t ws63_task_vk36n16i_set_multi_key_alarm(uint8_t enable)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    (void)enable;
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;

    if (g_ws63_vk36n16i_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_vk36n16i_multi_alarm_enable = (enable != 0U) ? 1U : 0U;
    if (g_ws63_vk36n16i_multi_alarm_enable == 0U) {
        g_ws63_vk36n16i_multi_alarm_active = 0U;
    }
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 查询多键报警开关状态。
 */
uint8_t ws63_task_vk36n16i_is_multi_key_alarm_enable(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t enabled;

    irq_status = WS63_FINAL_IRQ_LOCK();
    enabled = g_ws63_vk36n16i_multi_alarm_enable;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return enabled;
#endif
}

/**
 * @brief 查询当前是否处于多键报警状态。
 */
uint8_t ws63_task_vk36n16i_is_multi_key_active(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t active;

    irq_status = WS63_FINAL_IRQ_LOCK();
    active = g_ws63_vk36n16i_multi_alarm_active;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return active;
#endif
}

/**
 * @brief 读取最近一次原始 16 位码。
 */
uint16_t ws63_task_vk36n16i_get_raw_code(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0xFFFFU;
#else
    unsigned int irq_status;
    uint16_t raw_code;

    irq_status = WS63_FINAL_IRQ_LOCK();
    raw_code = g_ws63_vk36n16i_last_sample.raw_code;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return raw_code;
#endif
}

/**
 * @brief 读取最近一次按下掩码（位1=按下）。
 */
uint16_t ws63_task_vk36n16i_get_pressed_mask(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint16_t pressed_mask;

    irq_status = WS63_FINAL_IRQ_LOCK();
    pressed_mask = g_ws63_vk36n16i_last_sample.pressed_mask;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return pressed_mask;
#endif
}

/**
 * @brief 读取最近一次按下按键数量。
 */
uint8_t ws63_task_vk36n16i_get_pressed_count(void)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t pressed_count;

    irq_status = WS63_FINAL_IRQ_LOCK();
    pressed_count = g_ws63_vk36n16i_last_sample.pressed_count;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return pressed_count;
#endif
}

/**
 * @brief 获取最近一次按键标签文本。
 */
errcode_t ws63_task_vk36n16i_get_pressed_text(char *text, uint16_t text_len)
{
#if (WS63_VK36N16I_ENABLE != 1U)
    (void)text;
    (void)text_len;
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;
    ws63_vk36n16i_sample_t sample;

    if ((text == NULL) || (text_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    text[0] = '\0';

    irq_status = WS63_FINAL_IRQ_LOCK();
    sample = g_ws63_vk36n16i_last_sample;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    return ws63_vk36n16i_format_pressed_text(sample.pressed_mask, text, text_len);
#endif
}
