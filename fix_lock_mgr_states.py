import re
import os

file_path = "/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c"
with open(file_path, "r", encoding="utf-8") as f:
    code = f.read()

old_unlocking = """        } else if (g_ws63_lock_state == WS63_LOCK_STATE_UNLOCKING) {
            if (now_ms >= g_ws63_lock_unlock_deadline_ms) {
                (void)ws63_task_motor_coast_stop();
                ws63_lock_mgr_clear_feedback();
                g_ws63_lock_state = WS63_LOCK_STATE_IDLE;
                ws63_lock_mgr_set_ld2402_quiet_mode(0U);
                ws63_lock_mgr_set_ld2402_channel(1U);
                osal_printk("[lock mgr] unlock finished\\r\\n");
            } else if ((g_ws63_lock_feedback_mode == 1U) && (now_ms >= g_ws63_lock_feedback_deadline_ms)) {
                (void)ws63_task_buzzer_off();
                (void)ws63_task_rgb_off();
                g_ws63_lock_feedback_mode = 0U;
            }
        }"""

new_unlocking = """        } else if (g_ws63_lock_state == WS63_LOCK_STATE_UNLOCKING) {
            if (now_ms >= g_ws63_lock_unlock_deadline_ms) {
                (void)ws63_task_motor_coast_stop();
                g_ws63_lock_state = WS63_LOCK_STATE_HOLD_OPEN;
                g_ws63_lock_hold_deadline_ms = now_ms + WS63_LOCK_HOLD_OPEN_MS_DEFAULT;
                osal_printk("[lock mgr] unlock finished, entering hold open\\r\\n");
            } else if ((g_ws63_lock_feedback_mode == 1U) && (now_ms >= g_ws63_lock_feedback_deadline_ms)) {
                (void)ws63_task_buzzer_off();
                (void)ws63_task_rgb_off();
                g_ws63_lock_feedback_mode = 0U;
            }
        } else if (g_ws63_lock_state == WS63_LOCK_STATE_HOLD_OPEN) {
            ws63_lock_mgr_clear_feedback();
            if (now_ms >= g_ws63_lock_hold_deadline_ms) {
                (void)ws63_task_motor_reverse(WS63_LOCK_MOTOR_CLOSE_DUTY_DEFAULT);
                g_ws63_lock_state = WS63_LOCK_STATE_LOCKING;
                g_ws63_lock_locking_deadline_ms = now_ms + WS63_LOCK_UNLOCK_DURATION_MS_DEFAULT;
                osal_printk("[lock mgr] hold open finished, entering locking (auto relock)\\r\\n");
            }
        } else if (g_ws63_lock_state == WS63_LOCK_STATE_LOCKING) {
            if (now_ms >= g_ws63_lock_locking_deadline_ms) {
                (void)ws63_task_motor_coast_stop();
                g_ws63_lock_state = WS63_LOCK_STATE_IDLE;
                ws63_lock_mgr_set_ld2402_quiet_mode(0U);
                ws63_lock_mgr_set_ld2402_channel(1U);
                osal_printk("[lock mgr] locking finished, back to IDLE\\r\\n");
            }
        }"""

code = code.replace(old_unlocking, new_unlocking)

with open(file_path, "w", encoding="utf-8") as f:
    f.write(code)

print("Transitions updated.")
