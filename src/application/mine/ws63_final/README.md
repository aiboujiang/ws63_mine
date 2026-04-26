# WS63 Final Application

## 控制芯片替换
由于实际硬件中触控芯片为 VK36N16I，故将所有相关的 `TTP229` 驱动及 BSP 层配置全部替换为了 `VK36N16I`。

### 修改汇总：
- 文件名重命名：涉及所有的驱动 `Driver/ws63_vk36n16i.c/.h`、硬件 `BSP/ws63_final_bsp_vk36n16i.c`、任务 `App/Task/ws63_final_task_vk36n16i.c`。
- 代码内容中的 `TTP229`, `ttp229` 宏定义、函数名和引脚配置等一并完成替换。
- `CMakeLists.txt` 中的编译项同步更新。

## 任务维护记录

- 2026-04-15 修复 ZW101 认证超时时错误地延续生命周期的问题
        - 指纹认证超时（`ACK_TIMEOUT=0x26`）不再调用 `ws63_lock_mgr_refresh_auth_window`，避免导致锁始终保持在 `ARMED` 状态无法休眠。
        - 修复了 `ws63_final_task_sensor_bridge.c` 与 `ws63_final_task_lock_mgr.c` 中相关的逻辑。
        - 影响文件：`App/Task/ws63_final_task_sensor_bridge.c`，`App/Task/ws63_final_task_lock_mgr.c`。
        - 验证：代码编译通过（终端静默），不再报错。

- 2026-04-15 调整认证声光反馈时长
	- 修改了开锁成功和失败后的蜂鸣器与 RGB 红绿灯反馈时间，防止提示过于短促。
	- 新增 `WS63_LOCK_AUTH_SUCCESS_FEEDBACK_MS_DEFAULT` 和 `WS63_LOCK_AUTH_FAIL_FEEDBACK_MS_DEFAULT` 配置宏，默认为 800ms。
	- 修复了认证失败反馈时混用短促按键音（20ms）的问题，改为使用独立的低频长鸣（1200Hz）。
	- 影响文件：`ws63_final_config.h`、`ws63_final_task_lock_mgr.c`。
	- 验证：代码编译通过（终端静默），无语法错误。

- 2026-04-15 ZW101 详细追踪降噪
	- 关闭 ZW101 驱动与任务层的详细追踪默认开关，仅保留 VERIFY 成功/失败/超时等关键日志。
	- 将 `[zw101 trace]` 级别的状态快照、ACK 预览和重试辅助日志改为可选输出，避免正常认证链路刷屏。
	- 同步更新调试说明，标明详细追踪需要按需开启。
	- 影响文件：`src/application/mine/ws63_final/Driver/zw101.c`、`src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`、`src/application/mine/ws63_final/DEBUG_COMMANDS.md`。
	- 验证：`python3 build.py -c ws63-liteos-app` 通过；`zw101.c` 和 `ws63_final_task_sensor_bridge.c` 已编译通过。

- 2026-04-26 修复camera模块init complete回调发送来源
        - 将camera的 init complete 回调发送来源由宿主任务触发改为响应UART的真实上报。
        - 当ws63收到camera发来的“init complete”时，正确打印日志并将该包透传到上行链路中，证明camera初始化完成。
        - 影响文件：`App/Task/ws63_final_task_camera.c`。
        - 验证：执行 `ws63-liteos-app` 的增量编译编译通过。
