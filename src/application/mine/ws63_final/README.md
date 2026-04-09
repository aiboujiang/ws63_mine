# WS63 Final Layered Framework

## 1. 架构定位

1. 主控：WS63。
2. 外设：WK2114（UART 扩展芯片）。
3. 分层原则：上层只能调用下层，下层绝不调用上层。
4. 硬件操作边界：仅 `BSP` 可直接调用 WS63 的 GPIO/UART/IRQ API。

## 2. 目录结构

```text
ws63_final/
├── Config/      # 参数配置层（波特率、引脚、任务参数）
├── Common/      # 公共算法/通用工具（无硬件依赖）
├── BSP/         # WS63 硬件抽象层（GPIO/UART/IRQ）
├── Driver/      # WK2114 设备驱动层（寄存器/FIFO/子串口封装）
├── Middleware/  # OSAL 抽象（任务、延时、tick）
├── App/Task/    # 应用任务层（轮询调度、回调分发）
└── App/Main/    # 应用主入口（任务启动）
```

## 3. 对外接口

1. `ws63_start()`：启动最终版业务任务。
2. `ws63_task_register_rx_callback(sub_port, cb)`：注册子串口接收回调。
3. `ws63_task_send(sub_port, data, len)`：通过指定子串口发送数据。

## 4. 后续模块整合建议

1. 每个业务模块（如 LD2402/ZW101）在 `App/Task` 层注册自己的 `rx_callback`。
2. 业务模块禁止直接访问 BSP/寄存器，只能调用 Task/Driver 暴露的标准接口。
3. 如果新增硬件差异（引脚、波特率、FIFO 阈值），优先改 `Config`，不改业务逻辑。

## 5. 编译开关

在 menuconfig 中开启：

- `Application -> Mine -> Support Mine WS63 final layered framework (WS63 master).`

## 6. 备注

当前框架已完成基础链路：

1. WS63 主口初始化。
2. WK2114 主口自动波特匹配。
3. 子串口按配置初始化。
4. 轮询读取并回调分发。

你后续只需要在 `App/Task` 层扩展业务逻辑，即可完成多模块统一整合。

## 7. 调试命令文档

- 串口在线控测命令与日志说明请查看：`DEBUG_COMMANDS.md`

## 8. 任务维护记录

### 2026-04-09: 电机驱动与编码器测速接入

变更摘要：
- 新增电机驱动能力：正转、反转、滑行停止、刹车急停、占空比调速。
- 新增编码器测速能力：A/B 相判向计数，周期采样输出 RPM。
- 在任务层接入 motor/encoder 初始化与周期采样。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/Driver/ws63_motor.h`
- `src/application/mine/ws63_final/Driver/ws63_motor.c`
- `src/application/mine/ws63_final/Driver/ws63_encoder.h`
- `src/application/mine/ws63_final/Driver/ws63_encoder.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/CMakeLists.txt`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（ws63_liteos_app success）。

后续事项：
- 上板重点确认 GPIO2 对应 PWM2 的实际复用是否与板卡连线一致。

### 2026-04-09: 在线串口控测命令与日志增强

变更摘要：
- 新增调试串口命令能力（默认 UART0：GPIO17/18，115200）。
- 支持命令：`HELP`、`MOTOR FWD/REV/DUTY/STOP/BRAKE/RPM/STAT/WATCH ON|OFF`、`ENCODER RESET`。
- 新增命令输入、执行结果、周期监控三类日志，支持在线控测追踪。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（ws63_liteos_app success）。

后续事项：
- 若现场日志口已占用 UART0，可在配置中切换 `WS63_DEBUG_UART_BUS` 与对应引脚。

### 2026-04-09: 任务收尾文档维护 Skill 落地

变更摘要：
- 新建 `task-md-maintenance` Skill，约束每次任务完成时必须维护至少一个 Markdown 文档。
- 在仓库级 copilot 指令中加入强制文档维护规则，并指定 ws63_final 默认维护文档。
- 明确文档维护条目最小字段：变更摘要、影响文件、验证结果、后续事项。

影响文件：
- `.github/skills/task-md-maintenance/SKILL.md`
- `.github/copilot-instructions.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114 && git status --short`
- 结果：目标文件均有变更并可追踪，规则已落盘。

后续事项：
- 后续每次任务收尾继续在本节追加记录，形成可审计变更轨迹。

### 2026-04-09: 串口调试命令独立文档新增

变更摘要：
- 新增独立文档 `DEBUG_COMMANDS.md`，集中维护串口调试命令、日志格式与联调流程。
- 在主 README 增加文档入口，避免调试命令分散在代码与历史记录中。

影响文件：
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114 && git status --short`
- 结果：文档新增与索引更新均已生效。

后续事项：
- 每次新增/调整串口命令时同步更新 `DEBUG_COMMANDS.md`，保持联调口径一致。

### 2026-04-09: 串口调试命令稳定性修复（重复日志/偶发 ERROR）

变更摘要：
- 修复命令换行兼容：解析器同时支持 `CR`、`LF`、`CRLF`，避免“只回显不执行、后续批量执行”。
- 修复日志重复：调试日志默认仅输出到调试串口，避免同一物理口被 `osal_printk` 与调试口双写。
- 修复 UART 竞争：调试串口初始化前先 `uapi_uart_deinit`，降低与 AT 等已有 UART 用户并发抢读风险。
- 更新调试手册，补充串口冲突规避和日志镜像开关说明。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若现场仍需与 AT 命令并行，建议将调试命令口切换到独立 UART 总线并单独接线。

### 2026-04-09: 调试命令接收改为 UART 回调+行队列，状态日志简化

变更摘要：
- 调试命令接收从“主循环轮询 read”改为“UART 中断回调组帧 + 行缓冲队列出队执行”，降低命令丢失与延迟。
- 新增队列溢出/命令超长/接收错误告警，避免高日志负载场景下静默丢命令。
- 状态快照日志改为仅输出方向与转速（`dir` + `rpm`），并按当前电机状态规范化 RPM 符号。
- 同步更新 `DEBUG_COMMANDS.md` 的接收机制与日志格式说明。

影响文件：
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若现场命令与 WATCH 并发很高，建议先 `MOTOR WATCH OFF` 再连续下发控制命令。

### 2026-04-09: 状态日志增加电机轴 RPM 与输出轴 RPS 同时输出

变更摘要：
- 状态快照日志从单一 `rpm` 扩展为 `motor_rpm` 与 `out_rps` 两个字段，便于同时观察电机轴与输出轴速度。
- 新增减速比配置宏 `WS63_MOTOR_GEAR_RATIO`（默认 150），输出轴 `rps` 按该参数换算。
- 修正负小数显示边界，支持 `-0.xxx` 形式的输出轴转速日志。
- 同步更新 `DEBUG_COMMANDS.md` 的日志格式与换算说明。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若减速箱规格变化，仅需调整 `WS63_MOTOR_GEAR_RATIO` 即可同步修正 `out_rps`。

### 2026-04-09: Task 与 BSP 按功能模块拆分重构

变更摘要：
- 将 `ws63_final_task.c` 中串口调试命令实现拆分到独立子模块，主任务仅保留调度与调用入口。
- 将 `ws63_final_bsp.c` 按功能拆为 UART、RGB/SPI、电机/编码器三个子模块，主 BSP 文件仅保留通用控制能力。
- 更新 CMake 源文件列表，确保拆分后模块参与统一构建，不改变现有对外接口。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_uart.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_rgb.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_motor_encoder.c`
- `src/application/mine/ws63_final/CMakeLists.txt`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若后续继续扩展调试命令，优先修改 `App/Task/ws63_final_task_debug.c`，避免主任务文件再次膨胀。
