/**
 * @file ws63_final_task_ttp229.c
 * @brief Task 层 TTP229 子模块实现（独立任务 + 状态机）。
 */

#include "ws63_final_task_internal.h"

#include "osal_debug.h"
#include "securec.h"

#include <string.h>

#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_ttp229.h"

#if (WS63_TTP229_ENABLE == 1U)
/**
 * @brief TTP229 任务状态机。
 */
typedef enum {
    WS63_TTP229_STATE_INIT = 0,
    WS63_TTP229_STATE_DISABLED,
    WS63_TTP229_STATE_READY,
    WS63_TTP229_STATE_FAULT
} ws63_ttp229_state_t;

/* 任务运行状态。 */
static uint8_t g_ws63_ttp229_task_started = 0U;
static uint8_t g_ws63_ttp229_ready = 0U;
static uint8_t g_ws63_ttp229_enable = WS63_TTP229_ENABLE_DEFAULT;
static uint8_t g_ws63_ttp229_multi_alarm_enable = WS63_TTP229_MULTI_KEY_ALARM_DEFAULT;
static uint8_t g_ws63_ttp229_multi_alarm_active = 0U;
static uint8_t g_ws63_ttp229_force_reinit = 0U;
static ws63_ttp229_state_t g_ws63_ttp229_state = WS63_TTP229_STATE_INIT;
static ws63_ttp229_sample_t g_ws63_ttp229_last_sample = {
    .raw_code = 0x0000U,
    .pressed_mask = 0x0000U,
    .pressed_count = 0U,
    .multi_key = 0U
};
static uint8_t g_ws63_ttp229_password_session_active = 0U;
static uint8_t g_ws63_ttp229_password_len = 0U;
static uint8_t g_ws63_ttp229_password_overflow = 0U;
static uint16_t g_ws63_ttp229_password_last_mask = 0U;
static char g_ws63_ttp229_password_buf[WS63_TTP229_PASSWORD_LEN + 1U] = {0};

/*
 * 实测得到的按键位图映射。
 *
 * 说明：TTP229 仍然以 raw 位图为准，这里只负责把物理按键标签映射成
 * 可读文本，便于调试输出和现场联调。
 */
typedef struct {
    uint16_t mask;
    const char *label;
} ws63_ttp229_key_map_t;

static const ws63_ttp229_key_map_t g_ws63_ttp229_key_map[] = {
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
static errcode_t ws63_ttp229_format_pressed_text(uint16_t pressed_mask, char *text, uint16_t text_len)
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

    for (i = 0U; i < (uint16_t)(sizeof(g_ws63_ttp229_key_map) / sizeof(g_ws63_ttp229_key_map[0])); i++) {
        const ws63_ttp229_key_map_t *map = &g_ws63_ttp229_key_map[i];

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
 * @brief TTP229 状态锁。
 */
static unsigned int ws63_ttp229_lock(void)
{
    return ws63_os_irq_lock();
}

/**
 * @brief TTP229 状态解锁。
 */
static void ws63_ttp229_unlock(unsigned int irq_status)
{
    ws63_os_irq_unlock(irq_status);
}

/**
 * @brief 更新最新采样缓存。
 */
static void ws63_ttp229_update_sample(const ws63_ttp229_sample_t *sample)
{
    unsigned int irq_status;

    if (sample == NULL) {
        return;
    }

    irq_status = ws63_ttp229_lock();
    g_ws63_ttp229_last_sample = *sample;
    ws63_ttp229_unlock(irq_status);
}

/**
 * @brief 清空采样缓存为“无按键”状态。
 */
static void ws63_ttp229_reset_sample_cache(void)
{
    unsigned int irq_status;

    irq_status = ws63_ttp229_lock();
    g_ws63_ttp229_last_sample.raw_code = 0x0000U;
    g_ws63_ttp229_last_sample.pressed_mask = 0x0000U;
    g_ws63_ttp229_last_sample.pressed_count = 0U;
    g_ws63_ttp229_last_sample.multi_key = 0U;
    g_ws63_ttp229_multi_alarm_active = 0U;
    ws63_ttp229_unlock(irq_status);
}

/**
 * @brief 更新就绪标记。
 */
static void ws63_ttp229_set_ready(uint8_t ready)
{
    unsigned int irq_status;

    irq_status = ws63_ttp229_lock();
    g_ws63_ttp229_ready = (ready != 0U) ? 1U : 0U;
    ws63_ttp229_unlock(irq_status);
}

/**
 * @brief 清空 TTP229 密码输入缓存。
 */
static void ws63_ttp229_password_reset(void)
{
    unsigned int irq_status;

    irq_status = ws63_ttp229_lock();
    (void)memset_s(g_ws63_ttp229_password_buf,
        sizeof(g_ws63_ttp229_password_buf),
        0,
        sizeof(g_ws63_ttp229_password_buf));
    g_ws63_ttp229_password_len = 0U;
    g_ws63_ttp229_password_overflow = 0U;
    g_ws63_ttp229_password_last_mask = 0U;
    g_ws63_ttp229_password_session_active = 0U;
    ws63_ttp229_unlock(irq_status);
}

/**
 * @brief 开启一次新的密码输入会话。
 */
static void ws63_ttp229_password_start_session(void)
{
    unsigned int irq_status;

    irq_status = ws63_ttp229_lock();
    (void)memset_s(g_ws63_ttp229_password_buf,
        sizeof(g_ws63_ttp229_password_buf),
        0,
        sizeof(g_ws63_ttp229_password_buf));
    g_ws63_ttp229_password_len = 0U;
    g_ws63_ttp229_password_overflow = 0U;
    g_ws63_ttp229_password_last_mask = 0U;
    g_ws63_ttp229_password_session_active = 1U;
    ws63_ttp229_unlock(irq_status);
}

/**
 * @brief 停止当前密码输入会话。
 */
static void ws63_ttp229_password_stop_session(void)
{
    ws63_ttp229_password_reset();
}

/**
 * @brief 播放按键轻提示音。
 */
static void ws63_ttp229_password_beep(void)
{
    (void)ws63_task_buzzer_beep_tone(WS63_TTP229_KEY_PROMPT_BEEP_FREQ_HZ,
        WS63_TTP229_KEY_PROMPT_BEEP_VOLUME_PERCENT,
        WS63_TTP229_KEY_PROMPT_BEEP_MS);
}

/**
 * @brief 处理 TTP229 密码输入。
 */
static void ws63_ttp229_handle_password_input(const ws63_ttp229_sample_t *sample)
{
    char key_text[8] = {0};
    uint8_t armed;

    if (sample == NULL) {
        return;
    }

    armed = ws63_lock_mgr_is_armed();
    if (armed == 0U) {
        if (g_ws63_ttp229_password_session_active != 0U) {
            ws63_ttp229_password_stop_session();
        }
        return;
    }

    if (g_ws63_ttp229_password_session_active == 0U) {
        ws63_ttp229_password_start_session();
    }

    if (sample->pressed_mask == 0U) {
        g_ws63_ttp229_password_last_mask = 0U;
        return;
    }

    if (sample->pressed_mask == g_ws63_ttp229_password_last_mask) {
        return;
    }

    g_ws63_ttp229_password_last_mask = sample->pressed_mask;

    if (sample->pressed_count != 1U) {
        return;
    }

    if (ws63_ttp229_format_pressed_text(sample->pressed_mask, key_text, sizeof(key_text)) != ERRCODE_SUCC) {
        return;
    }

    if ((key_text[0] >= '0') && (key_text[0] <= '9') && (key_text[1] == '\0')) {
        if ((g_ws63_ttp229_password_len < WS63_TTP229_PASSWORD_LEN) && (g_ws63_ttp229_password_overflow == 0U)) {
            g_ws63_ttp229_password_buf[g_ws63_ttp229_password_len] = key_text[0];
            g_ws63_ttp229_password_len++;
            g_ws63_ttp229_password_buf[g_ws63_ttp229_password_len] = '\0';
            ws63_ttp229_password_beep();
        } else {
            g_ws63_ttp229_password_overflow = 1U;
        }
        return;
    }

    if ((key_text[0] == '#') && (key_text[1] == '\0')) {
        uint8_t passed = 0U;

        if ((g_ws63_ttp229_password_overflow == 0U) &&
            (g_ws63_ttp229_password_len == WS63_TTP229_PASSWORD_LEN) &&
            (strcmp(g_ws63_ttp229_password_buf, WS63_TTP229_PASSWORD_TEXT) == 0)) {
            passed = 1U;
        }

        osal_printk("[wk2114 final task] TTP229 password %s, input=%s\r\n",
            (passed != 0U) ? "pass" : "fail",
            g_ws63_ttp229_password_buf);
        (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_TTP229, passed);
        ws63_ttp229_password_stop_session();
    }
}

/**
 * @brief 处理多键报警状态变化，避免持续重复刷屏。
 */
static void ws63_ttp229_handle_alarm_transition(const ws63_ttp229_sample_t *sample)
{
    unsigned int irq_status;
    uint8_t alarm_enable;
    uint8_t alarm_active;
    char key_text[64] = {0};

    if (sample == NULL) {
        return;
    }

    if (ws63_ttp229_format_pressed_text(sample->pressed_mask, key_text, sizeof(key_text)) != ERRCODE_SUCC) {
        (void)strncpy_s(key_text, sizeof(key_text), "ERR", 3U);
    }

    irq_status = ws63_ttp229_lock();
    alarm_enable = g_ws63_ttp229_multi_alarm_enable;
    alarm_active = g_ws63_ttp229_multi_alarm_active;
    ws63_ttp229_unlock(irq_status);

    if (alarm_enable == 0U) {
        if (alarm_active != 0U) {
            irq_status = ws63_ttp229_lock();
            g_ws63_ttp229_multi_alarm_active = 0U;
            ws63_ttp229_unlock(irq_status);
        }
        return;
    }

    if ((sample->multi_key != 0U) && (alarm_active == 0U)) {
        osal_printk("[wk2114 final task] TTP229 multi-key alarm mask=0x%04x count=%u keys=%s\r\n",
            (unsigned int)sample->pressed_mask,
            (unsigned int)sample->pressed_count,
            key_text);
        irq_status = ws63_ttp229_lock();
        g_ws63_ttp229_multi_alarm_active = 1U;
        ws63_ttp229_unlock(irq_status);
        return;
    }

    if ((sample->multi_key == 0U) && (alarm_active != 0U)) {
        osal_printk("[wk2114 final task] TTP229 multi-key alarm clear\r\n");
        irq_status = ws63_ttp229_lock();
        g_ws63_ttp229_multi_alarm_active = 0U;
        ws63_ttp229_unlock(irq_status);
    }
}

/**
 * @brief TTP229 独立任务入口。
 *
 * 状态机说明：
 * 1) INIT: 初始化硬件与驱动；
 * 2) DISABLED: 任务保活但不采样；
 * 3) READY: 周期采样并更新缓存；
 * 4) FAULT: I2C 读取失败后退避重试。
 */
static void *ws63_ttp229_task_entry(const char *arg)
{
    (void)arg;

    ws63_ttp229_reset_sample_cache();
    ws63_ttp229_password_stop_session();
    ws63_ttp229_set_ready(0U);

    while (1) {
        uint8_t enabled;
        uint8_t force_reinit;

        {
            unsigned int irq_status = ws63_ttp229_lock();
            enabled = g_ws63_ttp229_enable;
            force_reinit = g_ws63_ttp229_force_reinit;
            if (force_reinit != 0U) {
                g_ws63_ttp229_force_reinit = 0U;
            }
            ws63_ttp229_unlock(irq_status);
        }

        if (force_reinit != 0U) {
            g_ws63_ttp229_state = WS63_TTP229_STATE_INIT;
            ws63_ttp229_set_ready(0U);
            ws63_ttp229_reset_sample_cache();
            ws63_ttp229_password_stop_session();
        }

        switch (g_ws63_ttp229_state) {
            case WS63_TTP229_STATE_INIT: {
                errcode_t ret;

                ret = ws63_ttp229_init();
                if (ret == ERRCODE_SUCC) {
                    ws63_ttp229_set_ready(1U);
                    g_ws63_ttp229_state = (enabled != 0U) ? WS63_TTP229_STATE_READY : WS63_TTP229_STATE_DISABLED;
                    osal_printk("[wk2114 final task] TTP229 init ok (I2C SCL=GPIO16 SDA=GPIO15)\r\n");
                } else {
                    ws63_ttp229_set_ready(0U);
                    g_ws63_ttp229_state = WS63_TTP229_STATE_FAULT;
                    osal_printk("[wk2114 final task] TTP229 init fail, ret=0x%x\r\n", (unsigned int)ret);
                }
                break;
            }
            case WS63_TTP229_STATE_DISABLED:
                if (enabled != 0U) {
                    g_ws63_ttp229_state = WS63_TTP229_STATE_INIT;
                }
                ws63_ttp229_password_stop_session();
                ws63_os_sleep_ms(WS63_TTP229_TASK_POLL_MS);
                break;
            case WS63_TTP229_STATE_READY:
                if (enabled == 0U) {
                    g_ws63_ttp229_state = WS63_TTP229_STATE_DISABLED;
                    ws63_ttp229_reset_sample_cache();
                    ws63_ttp229_password_stop_session();
                    ws63_os_sleep_ms(WS63_TTP229_TASK_POLL_MS);
                    break;
                }

                {
                    ws63_ttp229_sample_t sample;
                    errcode_t ret;

                    ret = ws63_ttp229_read_sample(&sample);
                    if (ret != ERRCODE_SUCC) {
                        ws63_ttp229_set_ready(0U);
                        g_ws63_ttp229_state = WS63_TTP229_STATE_FAULT;
                        osal_printk("[wk2114 final task] TTP229 I2C read fail, ret=0x%x\r\n", (unsigned int)ret);
                        ws63_os_sleep_ms(WS63_TTP229_INIT_RETRY_MS);
                        break;
                    }

                    ws63_ttp229_update_sample(&sample);
                    ws63_ttp229_handle_alarm_transition(&sample);
                    ws63_ttp229_handle_password_input(&sample);
                }

                ws63_os_sleep_ms(WS63_TTP229_TASK_POLL_MS);
                break;
            case WS63_TTP229_STATE_FAULT:
            default:
                if (enabled == 0U) {
                    g_ws63_ttp229_state = WS63_TTP229_STATE_DISABLED;
                    ws63_os_sleep_ms(WS63_TTP229_TASK_POLL_MS);
                } else {
                    ws63_os_sleep_ms(WS63_TTP229_INIT_RETRY_MS);
                    g_ws63_ttp229_state = WS63_TTP229_STATE_INIT;
                }
                break;
        }
    }

    return NULL;
}
#endif

/**
 * @brief 启动 TTP229 独立任务。
 */
errcode_t ws63_ttp229_task_start(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    if (g_ws63_ttp229_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_start_task("ws63_ttp229_task",
        ws63_ttp229_task_entry,
        0U,
        WS63_TTP229_TASK_STACK_SIZE,
        WS63_TTP229_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] TTP229 task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_ttp229_task_started = 1U;
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 触发 TTP229 重初始化。
 */
errcode_t ws63_task_ttp229_reinit(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;

    if (g_ws63_ttp229_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = ws63_ttp229_lock();
    g_ws63_ttp229_force_reinit = 1U;
    ws63_ttp229_unlock(irq_status);
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 设置 TTP229 状态机使能。
 */
errcode_t ws63_task_ttp229_set_enable(uint8_t enable)
{
#if (WS63_TTP229_ENABLE != 1U)
    (void)enable;
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;

    if (g_ws63_ttp229_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = ws63_ttp229_lock();
    g_ws63_ttp229_enable = (enable != 0U) ? 1U : 0U;
    ws63_ttp229_unlock(irq_status);
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 查询 TTP229 是否就绪。
 */
uint8_t ws63_task_ttp229_is_ready(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t ready;

    irq_status = ws63_ttp229_lock();
    ready = g_ws63_ttp229_ready;
    ws63_ttp229_unlock(irq_status);
    return ready;
#endif
}

/**
 * @brief 查询 TTP229 状态机使能状态。
 */
uint8_t ws63_task_ttp229_is_enabled(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t enabled;

    irq_status = ws63_ttp229_lock();
    enabled = g_ws63_ttp229_enable;
    ws63_ttp229_unlock(irq_status);
    return enabled;
#endif
}

/**
 * @brief 设置多键报警开关。
 */
errcode_t ws63_task_ttp229_set_multi_key_alarm(uint8_t enable)
{
#if (WS63_TTP229_ENABLE != 1U)
    (void)enable;
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;

    if (g_ws63_ttp229_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = ws63_ttp229_lock();
    g_ws63_ttp229_multi_alarm_enable = (enable != 0U) ? 1U : 0U;
    if (g_ws63_ttp229_multi_alarm_enable == 0U) {
        g_ws63_ttp229_multi_alarm_active = 0U;
    }
    ws63_ttp229_unlock(irq_status);
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 查询多键报警开关状态。
 */
uint8_t ws63_task_ttp229_is_multi_key_alarm_enable(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t enabled;

    irq_status = ws63_ttp229_lock();
    enabled = g_ws63_ttp229_multi_alarm_enable;
    ws63_ttp229_unlock(irq_status);
    return enabled;
#endif
}

/**
 * @brief 查询当前是否处于多键报警状态。
 */
uint8_t ws63_task_ttp229_is_multi_key_active(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t active;

    irq_status = ws63_ttp229_lock();
    active = g_ws63_ttp229_multi_alarm_active;
    ws63_ttp229_unlock(irq_status);
    return active;
#endif
}

/**
 * @brief 读取最近一次原始 16 位码。
 */
uint16_t ws63_task_ttp229_get_raw_code(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0xFFFFU;
#else
    unsigned int irq_status;
    uint16_t raw_code;

    irq_status = ws63_ttp229_lock();
    raw_code = g_ws63_ttp229_last_sample.raw_code;
    ws63_ttp229_unlock(irq_status);
    return raw_code;
#endif
}

/**
 * @brief 读取最近一次按下掩码（位1=按下）。
 */
uint16_t ws63_task_ttp229_get_pressed_mask(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint16_t pressed_mask;

    irq_status = ws63_ttp229_lock();
    pressed_mask = g_ws63_ttp229_last_sample.pressed_mask;
    ws63_ttp229_unlock(irq_status);
    return pressed_mask;
#endif
}

/**
 * @brief 读取最近一次按下按键数量。
 */
uint8_t ws63_task_ttp229_get_pressed_count(void)
{
#if (WS63_TTP229_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t pressed_count;

    irq_status = ws63_ttp229_lock();
    pressed_count = g_ws63_ttp229_last_sample.pressed_count;
    ws63_ttp229_unlock(irq_status);
    return pressed_count;
#endif
}

/**
 * @brief 获取最近一次按键标签文本。
 */
errcode_t ws63_task_ttp229_get_pressed_text(char *text, uint16_t text_len)
{
#if (WS63_TTP229_ENABLE != 1U)
    (void)text;
    (void)text_len;
    return ERRCODE_FAIL;
#else
    unsigned int irq_status;
    ws63_ttp229_sample_t sample;

    if ((text == NULL) || (text_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    text[0] = '\0';

    irq_status = ws63_ttp229_lock();
    sample = g_ws63_ttp229_last_sample;
    ws63_ttp229_unlock(irq_status);

    return ws63_ttp229_format_pressed_text(sample.pressed_mask, text, text_len);
#endif
}
