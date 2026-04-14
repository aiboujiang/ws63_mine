/**
 * @file ws63_final_task_lock_mgr.c
 * @brief 智能门锁编排任务实现。
 */

#include "ws63_final_task_internal.h"

#include <stddef.h>

#include "osal_debug.h"

#include "ld2402.h"

#include "ws63_final_config.h"
#include "ws63_final_osal.h"

#if (WS63_CAMERA_ENABLE == 1U)
/* 距离更新有效期：如果雷达长时间没有新距离，则认为接近事件已过期。 */
#define WS63_LOCK_DISTANCE_STALE_MS 1000U
/* camera 启停控制文本：仅在接近/离开时各发送一次。 */
#define WS63_LOCK_CAMERA_ACTION_TEXT "action"
#define WS63_LOCK_CAMERA_CLOSE_TEXT "Die"
/* ZW101 ACK=0x09：本次未检测到有效按压，属于可恢复状态。 */
#define WS63_ZW101_ACK_NOT_PRESSED 0x09U

typedef enum {
    WS63_LOCK_STATE_INIT = 0,
    WS63_LOCK_STATE_IDLE,
    WS63_LOCK_STATE_ARMED,
    WS63_LOCK_STATE_UNLOCKING
} ws63_lock_state_t;

typedef enum {
    WS63_LOCK_EVENT_AUTH_RESULT = 0
} ws63_lock_event_type_t;

typedef struct {
    ws63_lock_event_type_t type;
    ws63_lock_auth_source_t source;
    uint8_t passed;
} ws63_lock_event_t;

static unsigned long g_ws63_lock_event_queue = 0UL;
static uint8_t g_ws63_lock_task_started = 0U;
static ws63_lock_state_t g_ws63_lock_state = WS63_LOCK_STATE_INIT;
static uint8_t g_ws63_lock_camera_active = 0U;
static uint8_t g_ws63_lock_feedback_mode = 0U;
static uint8_t g_ws63_lock_ld2402_log_prev_enable = 0U;
static uint8_t g_ws63_lock_ld2402_log_forced_off = 0U;
static uint8_t g_ws63_lock_debug_suspended = 0U;
static uint32_t g_ws63_lock_auth_window_deadline_ms = 0U;
static uint32_t g_ws63_lock_unlock_deadline_ms = 0U;
static uint32_t g_ws63_lock_feedback_deadline_ms = 0U;
static uint32_t g_ws63_lock_last_distance_tick_ms = 0U;

static void ws63_lock_mgr_try_send_camera_close(void);
static void ws63_lock_mgr_clear_feedback(void);
static void ws63_lock_mgr_set_ld2402_quiet_mode(uint8_t enable);
static void ws63_lock_mgr_set_ld2402_channel(uint8_t enable);

/**
 * @brief 生成门锁状态文本，便于调试日志输出。
 */
static const char *ws63_lock_mgr_state_to_text(ws63_lock_state_t state)
{
    switch (state) {
        case WS63_LOCK_STATE_IDLE:
            return "IDLE";
        case WS63_LOCK_STATE_ARMED:
            return "ARMED";
        case WS63_LOCK_STATE_UNLOCKING:
            return "UNLOCKING";
        case WS63_LOCK_STATE_INIT:
        default:
            return "INIT";
    }
}

/**
 * @brief 播放门锁失败提示音。
 */
static void ws63_lock_mgr_play_fail_prompt(void)
{
    (void)ws63_task_buzzer_beep_tone(WS63_TTP229_KEY_PROMPT_BEEP_FREQ_HZ,
        WS63_TTP229_KEY_PROMPT_BEEP_VOLUME_PERCENT,
        WS63_TTP229_KEY_PROMPT_BEEP_MS);
}

/**
 * @brief 在认证失败时统一触发蜂鸣和 RGB 红灯提示。
 *
 * 说明：来源任务（TTP229/ZW101）只负责上报认证失败；门锁编排层统一做声光反馈，
 *       避免不同来源之间提示行为不一致。
 */
static void ws63_lock_mgr_start_fail_feedback(uint32_t now_ms)
{
    ws63_lock_mgr_play_fail_prompt();
    (void)ws63_task_rgb_set_color(220U, 0U, 0U);
    g_ws63_lock_feedback_mode = 2U;
    g_ws63_lock_feedback_deadline_ms = now_ms + WS63_TTP229_KEY_PROMPT_BEEP_MS;
}

/**
 * @brief 调试模式下挂起门锁编排流程并清空历史状态。
 */
static void ws63_lock_mgr_suspend_for_debug(void)
{
    ws63_lock_event_t event;
    uint32_t size;

    ws63_task_zw101_cancel_verify_request();
    ws63_lock_mgr_try_send_camera_close();
    (void)ws63_task_motor_coast_stop();
    (void)ws63_task_buzzer_off();
    (void)ws63_task_rgb_off();
    ws63_lock_mgr_clear_feedback();

    /* 调试态不恢复上一轮的距离日志开关，避免 DEBUG INIT 后又刷 distance。 */
    ws63_lock_mgr_set_ld2402_quiet_mode(1U);
    ws63_lock_mgr_set_ld2402_channel(1U);

    g_ws63_lock_state = WS63_LOCK_STATE_IDLE;
    g_ws63_lock_auth_window_deadline_ms = 0U;
    g_ws63_lock_unlock_deadline_ms = 0U;
    g_ws63_lock_feedback_deadline_ms = 0U;

    /* 丢弃队列中尚未处理的认证结果，避免退出调试后误消费旧事件。 */
    while (1) {
        size = (uint32_t)sizeof(event);
        if (ws63_os_msg_queue_recv(g_ws63_lock_event_queue, &event, &size, WS63_OS_NO_WAIT) != ERRCODE_SUCC) {
            break;
        }
    }
}

/**
 * @brief 推送一次认证结果到门锁事件队列。
 */
errcode_t ws63_lock_mgr_report_auth_result(ws63_lock_auth_source_t source, uint8_t passed)
{
    ws63_lock_event_t event;
    errcode_t ret;

    if (ws63_task_debug_is_debug_only_mode() != 0U) {
        osal_printk("[lock mgr trace] auth_event_drop source=%u passed=%u debug_only=1\r\n",
            (unsigned int)source,
            (unsigned int)((passed != 0U) ? 1U : 0U));
        return ERRCODE_FAIL;
    }

    if (g_ws63_lock_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    event.type = WS63_LOCK_EVENT_AUTH_RESULT;
    event.source = source;
    event.passed = (passed != 0U) ? 1U : 0U;
    ret = ws63_os_msg_queue_send(g_ws63_lock_event_queue,
        &event,
        (uint16_t)sizeof(event),
        WS63_OS_NO_WAIT);

    osal_printk("[lock mgr trace] auth_event_enqueue ret=0x%x source=%u passed=%u state=%s\r\n",
        (unsigned int)ret,
        (unsigned int)source,
        (unsigned int)event.passed,
        ws63_lock_mgr_state_to_text(g_ws63_lock_state));

    return ret;
}

/**
 * @brief 查询门锁是否处于接近唤醒窗口。
 */
uint8_t ws63_lock_mgr_is_armed(void)
{
    unsigned int irq_status;
    uint8_t armed;

    if (ws63_task_debug_is_debug_only_mode() != 0U) {
        return 0U;
    }

    irq_status = ws63_os_irq_lock();
    armed = (g_ws63_lock_state == WS63_LOCK_STATE_ARMED) ? 1U : 0U;
    ws63_os_irq_unlock(irq_status);

    return armed;
}

/**
 * @brief 查询当前接近唤醒窗口的截止时间。
 *
 * 说明：仅用于任务层日志打印，不参与状态机决策。
 */
uint32_t ws63_lock_mgr_get_auth_window_deadline_ms(void)
{
    unsigned int irq_status;
    uint32_t deadline_ms;

    if (ws63_task_debug_is_debug_only_mode() != 0U) {
        return 0U;
    }

    irq_status = ws63_os_irq_lock();
    deadline_ms = g_ws63_lock_auth_window_deadline_ms;
    ws63_os_irq_unlock(irq_status);

    return deadline_ms;
}

/**
 * @brief 刷新接近唤醒窗口超时。
 *
 * 只有在 ARMED 状态下才更新 deadline，避免无关输入把待机状态误拉长。
 */
errcode_t ws63_lock_mgr_refresh_auth_window(void)
{
    unsigned int irq_status;
    uint32_t now_ms;

    if (ws63_task_debug_is_debug_only_mode() != 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = ws63_os_irq_lock();
    if (g_ws63_lock_state != WS63_LOCK_STATE_ARMED) {
        ws63_os_irq_unlock(irq_status);
        return ERRCODE_FAIL;
    }

    now_ms = ws63_os_tick_ms();
    g_ws63_lock_auth_window_deadline_ms = now_ms + WS63_LOCK_AUTH_WINDOW_MS_DEFAULT;
    ws63_os_irq_unlock(irq_status);
    return ERRCODE_SUCC;
}

/**
 * @brief 向 camera 发送接近唤醒命令。
 */
static void ws63_lock_mgr_try_send_camera_action(void)
{
    if (g_ws63_lock_camera_active != 0U) {
        return;
    }

    if (ws63_task_camera_send_message(WS63_LOCK_CAMERA_ACTION_TEXT) == ERRCODE_SUCC) {
        g_ws63_lock_camera_active = 1U;
        osal_printk("[lock mgr] camera action sent\r\n");
    } else {
        osal_printk("[lock mgr] camera action send fail\r\n");
    }
}

/**
 * @brief 向 camera 发送关闭命令。
 */
static void ws63_lock_mgr_try_send_camera_close(void)
{
    if (g_ws63_lock_camera_active == 0U) {
        return;
    }

    if (ws63_task_camera_send_message(WS63_LOCK_CAMERA_CLOSE_TEXT) == ERRCODE_SUCC) {
        g_ws63_lock_camera_active = 0U;
        osal_printk("[lock mgr] camera Die sent\r\n");
    } else {
        osal_printk("[lock mgr] camera Die send fail\r\n");
    }
}

/**
 * @brief 启动一次开锁动作。
 */
static void ws63_lock_mgr_start_unlock(uint32_t now_ms, ws63_lock_auth_source_t source)
{
    ws63_lock_mgr_try_send_camera_close();
    (void)ws63_task_motor_forward(WS63_LOCK_MOTOR_OPEN_DUTY_DEFAULT);
    (void)ws63_task_buzzer_on(2200U);
    (void)ws63_task_rgb_set_color(0U, 180U, 0U);

    g_ws63_lock_feedback_mode = 1U;
    g_ws63_lock_feedback_deadline_ms = now_ms + 120U;
    g_ws63_lock_unlock_deadline_ms = now_ms + WS63_LOCK_UNLOCK_DURATION_MS_DEFAULT;
    g_ws63_lock_state = WS63_LOCK_STATE_UNLOCKING;

    osal_printk("[lock mgr] unlock start, source=%u\r\n", (unsigned int)source);
}

/**
 * @brief 退出提示状态并关闭蜂鸣器/RGB。
 */
static void ws63_lock_mgr_clear_feedback(void)
{
    if (g_ws63_lock_feedback_mode != 0U) {
        (void)ws63_task_buzzer_off();
        (void)ws63_task_rgb_off();
        g_ws63_lock_feedback_mode = 0U;
    }
}

/**
 * @brief 切换 LD2402 运行态日志的静默模式。
 *
 * 说明：接近唤醒后只保留状态迁移日志，避免 distance 刷屏占满串口。
 */
static void ws63_lock_mgr_set_ld2402_quiet_mode(uint8_t enable)
{
    uint8_t current_enable;

    if (enable != 0U) {
        if (g_ws63_lock_ld2402_log_forced_off != 0U) {
            return;
        }

        current_enable = ws63_task_ld2402_get_log_enable();
        g_ws63_lock_ld2402_log_prev_enable = current_enable;
        g_ws63_lock_ld2402_log_forced_off = 1U;
        if (current_enable != 0U) {
            (void)ws63_task_ld2402_set_log_enable(0U);
        }
        return;
    }

    if (g_ws63_lock_ld2402_log_forced_off == 0U) {
        return;
    }

    if (g_ws63_lock_ld2402_log_prev_enable != 0U) {
        (void)ws63_task_ld2402_set_log_enable(1U);
    }
    g_ws63_lock_ld2402_log_prev_enable = 0U;
    g_ws63_lock_ld2402_log_forced_off = 0U;
}

/**
 * @brief 根据门锁状态切换 LD2402 子口通道。
 *
 * 约束：
 * 1) ARMED 期间关闭 LD2402 通道，减少与 ZW101/camera 的并发冲突；
 * 2) 回到 IDLE 立即恢复 LD2402，保证下一轮接近检测可用。
 */
static void ws63_lock_mgr_set_ld2402_channel(uint8_t enable)
{
    uint8_t current_enable;

    current_enable = ws63_task_ld2402_is_channel_enabled();
    if (current_enable == ((enable != 0U) ? 1U : 0U)) {
        return;
    }

    if (ws63_task_ld2402_set_channel_enable(enable) == ERRCODE_SUCC) {
        osal_printk("[lock mgr] ld2402 channel %s\r\n", (enable != 0U) ? "ON" : "OFF");
    } else {
        osal_printk("[lock mgr] ld2402 channel switch fail, target=%s\r\n", (enable != 0U) ? "ON" : "OFF");
    }
}

/**
 * @brief 处理一条门锁认证事件。
 *
 * 策略：
 * 1) 本模块不再维护跨来源全局失败锁定；
 * 2) 失败 5 次封禁由来源任务各自维护（TTP229/ZW101）；
 * 3) 认证失败的蜂鸣 + RGB 提示在这里统一触发。
 */
static void ws63_lock_mgr_handle_event(const ws63_lock_event_t *event, uint32_t now_ms)
{
    if (event == NULL) {
        return;
    }

    if (event->type != WS63_LOCK_EVENT_AUTH_RESULT) {
        return;
    }

    osal_printk("[lock mgr trace] auth_event_handle source=%u passed=%u state=%s now=%u\r\n",
        (unsigned int)event->source,
        (unsigned int)event->passed,
        ws63_lock_mgr_state_to_text(g_ws63_lock_state),
        (unsigned int)now_ms);

    /* 认证结果本身也是一次有效输入，先续命再做状态机判定。 */
    (void)ws63_lock_mgr_refresh_auth_window();

    if (event->passed != 0U) {
        if (g_ws63_lock_state == WS63_LOCK_STATE_ARMED) {
            ws63_task_zw101_cancel_verify_request();
            ws63_lock_mgr_start_unlock(now_ms, event->source);
        } else {
            osal_printk("[lock mgr] auth pass ignored, state=%s source=%u\r\n",
                ws63_lock_mgr_state_to_text(g_ws63_lock_state),
                (unsigned int)event->source);
        }
        return;
    }

    if (g_ws63_lock_state == WS63_LOCK_STATE_ARMED) {
        uint8_t count_as_fail = 1U;

        if (event->source == WS63_LOCK_AUTH_SOURCE_ZW101) {
            uint8_t last_ack = ws63_task_zw101_get_last_verify_ack();
            if (last_ack == WS63_ZW101_ACK_NOT_PRESSED) {
                /* NOT_PRESSED 仅触发窗口内重试，不作为有效认证失败。 */
                count_as_fail = 0U;
                osal_printk("[lock mgr] zw101 no-press, skip fail_feedback\r\n");
            }
        }

        if (count_as_fail != 0U) {
            osal_printk("[lock mgr] auth fail, source=%u\r\n", (unsigned int)event->source);
            ws63_lock_mgr_start_fail_feedback(now_ms);
        }

        if (event->source == WS63_LOCK_AUTH_SOURCE_ZW101) {
            errcode_t retrigger_ret = ws63_task_zw101_request_verify_after_release();
            if (retrigger_ret != ERRCODE_SUCC) {
                osal_printk("[lock mgr] zw101 verify retrigger skipped\r\n");
            } else {
                osal_printk("[lock mgr trace] zw101 verify retrigger wait-release queued\r\n");
            }
        }
    }
}

/**
 * @brief 门锁编排任务主循环。
 *
 * 状态机说明：
 * 1) IDLE：等待 LD2402 距离进入 80mm 接近阈值；
 * 2) ARMED：已经检测到接近，发送 camera action 并等待 camera / ZW101 / TTP229 任一认证结果；
 * 3) UNLOCKING：电机执行开锁动作，超时后自动停转。
 */
static void *ws63_lock_mgr_task_entry(const char *arg)
{
    (void)arg;

    while (1) {
        uint32_t now_ms;
        int32_t distance_mm;
        uint32_t distance_tick_ms;

        now_ms = ws63_os_tick_ms();
        distance_mm = ld2402_get_last_distance_mm();
        distance_tick_ms = ld2402_get_last_distance_tick_ms();

        if (ws63_task_debug_is_debug_only_mode() != 0U) {
            if (g_ws63_lock_debug_suspended == 0U) {
                ws63_lock_mgr_suspend_for_debug();
                g_ws63_lock_debug_suspended = 1U;
                osal_printk("[lock mgr] debug-only mode active, suspend lock flow\r\n");
            }

            ws63_os_sleep_ms(WS63_LOCK_MGR_TASK_POLL_MS);
            continue;
        }

        if (g_ws63_lock_debug_suspended != 0U) {
            g_ws63_lock_debug_suspended = 0U;
            osal_printk("[lock mgr] debug-only mode cleared, resume lock flow\r\n");
        }

        /* 先处理认证事件，再根据最新距离状态更新门锁状态机。 */
        while (1) {
            ws63_lock_event_t event;
            uint32_t size = (uint32_t)sizeof(event);

            if (ws63_os_msg_queue_recv(g_ws63_lock_event_queue, &event, &size, WS63_OS_NO_WAIT) != ERRCODE_SUCC) {
                break;
            }

            ws63_lock_mgr_handle_event(&event, now_ms);
        }

        if (g_ws63_lock_state == WS63_LOCK_STATE_INIT) {
            g_ws63_lock_state = WS63_LOCK_STATE_IDLE;
        }

        if (g_ws63_lock_state == WS63_LOCK_STATE_IDLE) {
            if ((distance_mm >= 0) &&
                ((uint32_t)distance_mm < WS63_LOCK_LD2402_ARM_DISTANCE_MM_DEFAULT) &&
                (distance_tick_ms != g_ws63_lock_last_distance_tick_ms) &&
                ((uint32_t)(now_ms - distance_tick_ms) <= WS63_LOCK_DISTANCE_STALE_MS)) {
                g_ws63_lock_last_distance_tick_ms = distance_tick_ms;
                g_ws63_lock_state = WS63_LOCK_STATE_ARMED;
                osal_printk("[lock mgr] armed distance=%ldmm tick=%u\r\n",
                    (long)distance_mm,
                    (unsigned int)distance_tick_ms);

                /* 每次新进入 ARMED，都重置 ZW101 上一窗口遗留的禁用与失败计数。 */
                ws63_task_zw101_reset_armed_window_guard();
                (void)ws63_lock_mgr_refresh_auth_window();
                ws63_lock_mgr_set_ld2402_quiet_mode(1U);
                ws63_lock_mgr_set_ld2402_channel(0U);

                /* 进入 ARMED 后再惰性拉起 camera/ZW101，降低上电阶段并发冲突。 */
                if (ws63_task_ensure_camera_ready() != ERRCODE_SUCC) {
                    osal_printk("[lock mgr] camera lazy init fail\r\n");
                }
                if (ws63_task_ensure_zw101_ready() != ERRCODE_SUCC) {
                    osal_printk("[lock mgr] zw101 lazy init fail\r\n");
                }

                ws63_lock_mgr_try_send_camera_action();
                if (ws63_task_zw101_request_verify() != ERRCODE_SUCC) {
                    osal_printk("[lock mgr trace] zw101 verify request on armed failed\r\n");
                } else {
                    osal_printk("[lock mgr trace] zw101 verify request on armed posted\r\n");
                }
            }
        } else if (g_ws63_lock_state == WS63_LOCK_STATE_ARMED) {
            if ((distance_mm >= 0) &&
                ((uint32_t)distance_mm < WS63_LOCK_LD2402_ARM_DISTANCE_MM_DEFAULT) &&
                (distance_tick_ms != g_ws63_lock_last_distance_tick_ms)) {
                g_ws63_lock_last_distance_tick_ms = distance_tick_ms;
                (void)ws63_lock_mgr_refresh_auth_window();
            }

            if (now_ms >= g_ws63_lock_auth_window_deadline_ms) {
                osal_printk("[lock mgr] auth window timeout\r\n");
                ws63_task_zw101_cancel_verify_request();
                ws63_lock_mgr_try_send_camera_close();
                ws63_lock_mgr_clear_feedback();
                g_ws63_lock_state = WS63_LOCK_STATE_IDLE;
                ws63_lock_mgr_set_ld2402_quiet_mode(0U);
                ws63_lock_mgr_set_ld2402_channel(1U);
            } else if ((g_ws63_lock_feedback_mode == 2U) && (now_ms >= g_ws63_lock_feedback_deadline_ms)) {
                /* 失败反馈是短促提示，超时后立即熄灭红灯。 */
                (void)ws63_task_buzzer_off();
                (void)ws63_task_rgb_off();
                g_ws63_lock_feedback_mode = 0U;
            }
        } else if (g_ws63_lock_state == WS63_LOCK_STATE_UNLOCKING) {
            if (now_ms >= g_ws63_lock_unlock_deadline_ms) {
                (void)ws63_task_motor_coast_stop();
                ws63_lock_mgr_clear_feedback();
                g_ws63_lock_state = WS63_LOCK_STATE_IDLE;
                ws63_lock_mgr_set_ld2402_quiet_mode(0U);
                ws63_lock_mgr_set_ld2402_channel(1U);
                osal_printk("[lock mgr] unlock finished\r\n");
            } else if ((g_ws63_lock_feedback_mode == 1U) && (now_ms >= g_ws63_lock_feedback_deadline_ms)) {
                (void)ws63_task_buzzer_off();
                (void)ws63_task_rgb_off();
                g_ws63_lock_feedback_mode = 0U;
            }
        }

        ws63_os_sleep_ms(WS63_LOCK_MGR_TASK_POLL_MS);
    }

    return NULL;
}

/**
 * @brief 启动门锁编排任务。
 */
errcode_t ws63_lock_mgr_task_start(void)
{
    errcode_t ret;

    if (g_ws63_lock_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_lock_q",
        8U,
        (uint16_t)sizeof(ws63_lock_event_t),
        &g_ws63_lock_event_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[lock mgr] queue create fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = ws63_os_start_task("ws63_lock_mgr_task",
        ws63_lock_mgr_task_entry,
        0U,
        WS63_LOCK_MGR_TASK_STACK_SIZE,
        WS63_LOCK_MGR_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_lock_event_queue);
        g_ws63_lock_event_queue = 0UL;
        osal_printk("[lock mgr] task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_lock_task_started = 1U;
    osal_printk("[lock mgr] task start ok\r\n");
    return ERRCODE_SUCC;
}
#endif