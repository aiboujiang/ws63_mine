import re
import os

file_path = "/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c"
with open(file_path, "r", encoding="utf-8") as f:
    code = f.read()

# Add states
code = code.replace("WS63_LOCK_STATE_UNLOCKING", "WS63_LOCK_STATE_UNLOCKING,\n    WS63_LOCK_STATE_HOLD_OPEN,\n    WS63_LOCK_STATE_LOCKING")

# Add stat strings
code = code.replace('case WS63_LOCK_STATE_UNLOCKING:\n            return "UNLOCKING";', 
                    'case WS63_LOCK_STATE_UNLOCKING:\n            return "UNLOCKING";\n        case WS63_LOCK_STATE_HOLD_OPEN:\n            return "HOLD_OPEN";\n        case WS63_LOCK_STATE_LOCKING:\n            return "LOCKING";')

# Add deadline variables
code = code.replace("static uint32_t g_ws63_lock_unlock_deadline_ms = 0U;", 
                    "static uint32_t g_ws63_lock_unlock_deadline_ms = 0U;\nstatic uint32_t g_ws63_lock_hold_deadline_ms = 0U;\nstatic uint32_t g_ws63_lock_locking_deadline_ms = 0U;")

code = code.replace(" * 3) UNLOCKING：电机执行开锁动作，超时后自动停转。", 
                    " * 3) UNLOCKING：电机执行开锁动作，超时后自动停转。\n * 4) HOLD_OPEN：保持开启状态等待（例如 10s）。\n * 5) LOCKING：自动回锁动作，超时后自动停转并回到 IDLE。")


with open(file_path, "w", encoding="utf-8") as f:
    f.write(code)

print("States added.")
