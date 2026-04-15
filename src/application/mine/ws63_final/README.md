# WS63 Final Application

## 控制芯片替换
由于实际硬件中触控芯片为 VK36N16I，故将所有相关的 `TTP229` 驱动及 BSP 层配置全部替换为了 `VK36N16I`。

### 修改汇总：
- 文件名重命名：涉及所有的驱动 `Driver/ws63_vk36n16i.c/.h`、硬件 `BSP/ws63_final_bsp_vk36n16i.c`、任务 `App/Task/ws63_final_task_vk36n16i.c`。
- 代码内容中的 `TTP229`, `ttp229` 宏定义、函数名和引脚配置等一并完成替换。
- `CMakeLists.txt` 中的编译项同步更新。

## 任务维护记录

- 2026-04-15 ZW101 详细追踪降噪
	- 关闭 ZW101 驱动与任务层的详细追踪默认开关，仅保留 VERIFY 成功/失败/超时等关键日志。
	- 将 `[zw101 trace]` 级别的状态快照、ACK 预览和重试辅助日志改为可选输出，避免正常认证链路刷屏。
	- 同步更新调试说明，标明详细追踪需要按需开启。
	- 影响文件：`src/application/mine/ws63_final/Driver/zw101.c`、`src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`、`src/application/mine/ws63_final/DEBUG_COMMANDS.md`。
	- 验证：`python3 build.py -c ws63-liteos-app` 通过；`zw101.c` 和 `ws63_final_task_sensor_bridge.c` 已编译通过。
