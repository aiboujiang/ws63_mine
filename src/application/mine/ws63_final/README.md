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
4. `ws63_task_buzzer_on(freq_hz)` / `ws63_task_buzzer_off()`：控制蜂鸣器开关与频率。
5. `ws63_task_buzzer_set_volume(volume_percent)`：设置蜂鸣器音量（占空比映射）。
6. `ws63_task_ld2402_reinit()` / `ws63_task_zw101_reinit()`：触发模块调试握手重初始化。
7. `ws63_task_zw101_za_*`：ZW101 ZA 兼容命令桥接接口（回显/自动登录/自动搜索/终止）。

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

### 2026-04-12: LD2402 命令别名收敛到 LD

变更摘要：
- 将 `ws63_final` 的 LD2402 调试入口收敛为 `LD ...` 前缀，去掉旧 `LD2402 ...` 兼容别名。
- 当前可执行命令仅保留 `LD HELP/INIT/STAT/VERSION/SN/MODE/DIST/DELAY/GET/SET/SAVE/GAIN/AUTO/PROGRESS/ALARM/PWR/SAVE3F/RAW/LOG/LOGINT/LOGSTAT`。
- Task 层仍通过 LD2402 驱动实现协议处理，但对外调试命令面只保留 `LD`，避免现场脚本继续依赖旧别名。
- 同步更新 `DEBUG_COMMANDS.md`，移除旧前缀示例与故障排查中的 `LD2402` 命令写法。

影响文件：
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/Driver/ld2402.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板时优先确认 `LD INIT`、`LD VERSION`、`LD SN` 与 `LD STAT` 输出是否正常。
- 现场脚本如仍调用旧前缀，需要直接切换到 `LD`，不再依赖 `LD2402` 兼容入口。

### 2026-04-12: RGB 上电不再默认进入 demo，LD2402 运行态距离日志统一节流

变更摘要：
- RGB 任务上电后只做硬件初始化，不再默认进入演示循环，避免灯效在未下发命令时自动跑起来。
- 新增 `WS63_RGB_DEMO_ENABLE_DEFAULT` 默认策略宏，便于后续按现场需要重新打开演示模式。
- LD2402 的 `OFF` / `distance` / 普通数据包日志统一走同一条节流判断，避免距离文本分支绕过日志间隔限制。
- 保留最近距离值与更新时间刷新逻辑，仅收紧串口输出频率，减少门锁现场刷屏。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：通过（`Build target:ws63_liteos_app success`）。

### 2026-04-11: TTP229 I2C 上拉容错修复

变更摘要：
- 发现 `GPIO15/16` 的内部上拉配置在当前板级上会返回失败，但这不影响 TTP229 的 I2C 通信本身。
- 将 BSP 中的上拉配置改为“尽力设置、失败仅告警”，避免误把可恢复的引脚能力差异当成初始化失败。
- 保持原有 I2C 读流程、Task 状态机和报警逻辑不变，只修正初始化容错边界。

影响文件：
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过，且 TTP229 BSP 不再因内部上拉返回码直接中止初始化。

后续事项：
- 上板时继续优先观察 TTP229 的 `init ok` / `init fail` 日志，再确认按键读数是否稳定。

### 2026-04-11: TTP229 改为 I2C 读取

变更摘要：
- 将 `ws63_final` 的 TTP229 从旧的 SDO/SCL 逐位扫描改为标准 I2C 主机读流程。
- 保持 GPIO16 / GPIO15 不变，改为对应 I2C SDA / SCL 引脚复用与上拉配置。
- 按手册统一为 2 字节直接读取，按键位语义保持为 `1=按下`。
- Task 层保留原有状态机与多键报警逻辑，仅同步更新采样默认值与错误日志。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.h`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板后优先确认 TTP229 的 `INIT/READ/WATCH` 日志与实际触摸结果一致。
- 若需要进一步降低 I2C 读取抖动，可结合 `INT` 引脚再补事件唤醒策略。

### 2026-04-11: TTP229 持续检测命令 + LD2402 命令统一

变更摘要：
- 调试串口新增 `TTP229 WATCH ON|OFF` 命令，用于启动/停止矩阵键盘持续检测日志。
- 持续检测复用现有 WATCH 周期调度，在固定节拍下输出 `raw/mask/count`，便于观察按键实时变化。
- `ws63_final` 范围内彻底移除旧雷达命令前缀，统一为 `LD2402 INIT/RAW/STAT/LOG/LOGINT/LOGSTAT`。
- 同步修正文档手册与 README 的命令口径，保证现场联调命令与帮助输出一致。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板建议先执行 `TTP229 WATCH ON` 再按键，确认持续检测日志正常；完成后执行 `TTP229 WATCH OFF` 关闭持续输出。
- 旧雷达指令前缀已移除，如现场脚本仍使用旧前缀需统一替换为 `LD2402 ...`。

### 2026-04-11: LD2402 上电刷屏限制（运行态节流 + 运行时开关）

变更摘要：
- 修复 `ws63_final` 中 LD2402 上电后运行态日志持续刷屏问题：在 Driver 层对 `LD2402 processing ...` 增加时间节流，默认每 1000ms 最多输出一次。
- 保留初始化/失败诊断日志（如 `init try`、`init failed`），确保链路故障定位能力不受影响。
- SLE 中间件将 `uplink send success` 从“每包打印”改为“可开关 + 间隔节流 + 抑制计数”，默认关闭 success 逐包日志，失败日志保持即时输出。
- 调试串口新增运行时控制命令：`LD2402 LOG ON|OFF`、`LD2402 LOGINT <ms>`、`LD2402 LOGSTAT`、`SLE ULOG ON|OFF`、`SLE ULOGINT <ms>`、`SLE ULOGSTAT`，无需重编译即可现场调节日志强度。
- 配置层新增默认策略宏，统一控制 LD2402 与 SLE success 日志初始行为。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/Driver/ld2402.h`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/Middleware/ws63_final_sle.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_sle.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板可先执行 `LD2402 LOGSTAT` 与 `SLE ULOGSTAT` 确认默认策略，再按现场需要用 `LOGINT` 动态调整节流窗口。
- 若需临时抓全量包级日志，可将间隔设为 `0`（例如 `LD2402 LOGINT 0` / `SLE ULOGINT 0`）。

### 2026-04-10: WK2114 主口读路径防卡死修复（规避 uapi_uart_read 长轮询）

变更摘要：
- 修复 `ws63_final` 主口 UART 读路径在当前驱动配置下可能出现的长轮询卡死问题，避免 `ws63_wk_task` 持续 100% 占用后触发 NMI 重启。
- 在 BSP 层新增“按字节 + FIFO 非空预判 + 软超时”的安全读取封装，替换主口/调试口原有直接 `uapi_uart_read` 调用。
- 重写主口 `flush_rx` 为安全非阻塞逐字节清空，避免 `len>1` 读取在无数据时陷入不可退出轮询。
- 保持 `Driver/App` 协议流程不变，仅修复底层读取语义与超时行为。

影响文件：
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_uart.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板重点观察 `ld2402_init` 阶段是否不再出现 `ws63_wk_task` 长时间 100% 占用与 NMI 重启。
- 若现场仍有偶发超时，可再补充 `ws63_bsp_host_uart_read` 的超时日志（开始/结束/返回字节数）用于进一步定位链路抖动。

### 2026-04-10: 调试命令日志换行格式修复（\r\n 字面量问题）

变更摘要：
- 修复 `ws63_final` 调试命令日志中错误使用 `\\r\\n` 字面量的问题，统一改为 `\r\n` 控制符输出。
- 修复后 `uart ready` 与 `command list` 等日志按行显示，不再把 `\r\n` 作为可见字符打印。
- 本次仅调整日志格式，不改动命令解析、驱动交互与业务逻辑。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板观察 `ws63 dbg` 启动日志应逐行换行显示；
- 若现场串口工具仍显示转义文本，需检查上位机是否启用了“显示控制字符转义”选项。

### 2026-04-10: ZW101 初始化超时最小修复（子口波特率对齐 57600）

变更摘要：
- 修复 `ws63_final` 中 ZW101 子口默认波特率与模组默认值不一致的问题：将子口1（ZW101）波特率从 `115200` 对齐到 `57600`。
- 新增 ZW101 初始化配置日志，启动时打印 `ZW101 cfg sub-uartX baud=Y`，便于现场快速确认配置是否生效。
- 保持现有握手流程与 ACK 解析策略不变（仍为 `0x53 -> 0x35 -> check_sensor` 路径），确保本轮改动风险最小。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板后重点核对日志：`sub-uart1 init ok, baud=57600` 与 `ZW101 cfg sub-uart1 baud=57600`；
- 若仍出现 `ack=0x26`，再进入第二阶段（双波特率探测 + 接收统计日志）排查物理层差异。

### 2026-04-10: ZW101/LD2402 初始化失败诊断增强（握手命令拆分 + 失败观测）

变更摘要：
- 将调试命令中的 `ZW101 HANDSHAKE` 从 `INIT` 别名改为“仅发送 0x35 握手并回显 ACK”，便于快速判断基础链路是否可达。
- 新增 `ZW101 CHECKSENSOR` 命令（0x36），用于与握手命令配合定位“握手成功但传感器检测失败”的场景。
- ZW101 驱动补充初始化分步日志：每轮打印 `echo/handshake/check_sensor` 的 `ret` 与 `ack`，并在命令等待 ACK 超时时输出命令码。
- LD2402 驱动补充初始化分步日志：每轮打印 `rx_total/valid_frame/enable_ack`，用于区分“完全无回包”与“收到帧但非目标 ACK”。
- 同步更新调试手册，新增 `ack=0x26`（ACK 超时）释义与排查顺序。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
后续事项：
- 上板建议先执行 `ZW101 HANDSHAKE`、`ZW101 CHECKSENSOR`、`ZW101 ZA ECHO`，并结合新增 `init try` 日志判断是否为“串口无回包”。
- 对 LD2402 建议关注 `init tryN rx_total`，若连续为 `0`，优先排查模块供电、TX/RX 交叉与子口连线。

### 2026-04-12: 系统设计稿对齐现网能力

变更摘要：
- 重写根目录 [系统设计.md](/home/xixi/code/系统设计.md)，将文档从概念方案收敛为“现网能力 + 演进路线”双层结构。
- 补齐 ws63_final 已实现的模块边界、能力矩阵、门锁状态机、认证来源与调试命令口径。
- 明确 camera 仅作为外部视觉模块桥接，不再把本地人脸算法写成 WS63 现网能力。
- 将本地密码哈希认证、关门检测等未落地能力移动到演进路线，避免与现网实现混写。

影响文件：
- `/home/xixi/code/系统设计.md`

验证结果：
- 对照 `src/application/mine/ws63_final/README.md`、`src/application/mine/ws63_final/DEBUG_COMMANDS.md` 以及现网任务代码完成人工核对。
- 未执行构建；本次仅修改设计文档，未涉及源码。

后续事项：
- 如后续 ws63_final 增加密码认证或关门检测，再把演进路线中的对应条目提升为现网能力说明。

### 2026-04-10: 串口实测日志问题修复（RGB 默认可用 + STOP 符号显示）

变更摘要：
- 根据现场串口日志定位 `RGB INIT/SET/DEMO` 返回 `0xffffffff` 的根因：`WS63_RGB_ENABLE` 处于关闭配置。
- 将 `WS63_RGB_ENABLE` 默认值从 `0` 调整为 `1`，使 `RGB INIT` 等调试命令在默认构建下可直接联调。
- 修正电机状态日志的 RPM 归一化逻辑：在 `STOP/BRAKE` 状态下保留编码器原始符号，便于观察反转后惯性衰减方向。
- 保持分层边界不变，仅修改配置层与 Task 调试显示逻辑。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板建议复测：`RGB INIT` -> `RGB SET 255 0 0` -> `RGB DEMO ON`；
- 反转后执行 `MOTOR STOP`，确认短暂余转期间 `motor_rpm/out_rps` 符号与实际方向一致。

### 2026-04-09: ZW101 ZA 兼容命令调试接入 + 指令族函数补齐

变更摘要：
- 基于 `mine/lib/指纹模组产品用户手册_V1.5.1.pdf` 的基础通信流程，实现 ZW101 协议帧组包、收包提帧、应答等待与校验路径。
- 驱动层补齐 ZA 兼容命令函数：`GetEcho(0x53)`、`AutoLogin(0x54)`、`AutoSearch(0x55)`、`SearchResBack(0x56)`、`AutoLoginStabLight(0x57)`、`AutoSearchWithEcho(0x58)`、`ProcessTerminateCmd(0xAA)`。
- 按手册补齐业务类/维护类/定制类命令函数实现（当前阶段先实现函数，不在任务主流程默认调用）。
- 调试命令新增 `ZW101 ZA` 子命令入口，支持在线联调并打印关键返回字段（ack/id/score）。

影响文件：
- `src/application/mine/ws63_final/Driver/zw101.h`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板优先执行 `ZW101 ZA ECHO` 与 `ZW101 ZA SEARCH`，确认回包时序与 ACK 码和手册一致，再逐步联调业务类/维护类命令。

### 2026-04-09: 蜂鸣器音量调节 + LD2402/ZW101 调试命令扩展

变更摘要：
- 蜂鸣器新增音量能力，音量参数映射为 PWM 占空比，支持 `0~100%` 调节。
- 调试串口新增 `BEEP VOL <0-100>` 命令，状态日志增加 `vol` 字段。
- 新增 LD2402 调试命令：`INIT/RAW/STAT`，支持在线下发十六进制原始帧。
- 新增 ZW101 调试命令：`INIT(HANDSHAKE)/RAW/STAT`，支持握手复测与原始帧联调。
- 任务层新增模块调试桥接接口，统一由 App/Task 层调用 Driver，维持分层边界。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_beep.c`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.h`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若现场板卡蜂鸣器输出偏弱，可优先提高 `BEEP VOL`，再结合 `BEEP FREQ` 微调听感。

### 2026-04-09: 蜂鸣器（beep）移植到 ws63_final

变更摘要：
- 新增蜂鸣器 BSP 子模块，封装 GPIO9/PWM1 底层初始化、发声与静音控制。
- 新增蜂鸣器 Driver 层，提供开关与频率语义接口，并缓存当前状态。
- 任务层新增蜂鸣器初始化与任务接口，支持应用层统一调用。
- 调试串口命令新增 `BEEP ON/OFF/FREQ/STAT`，可在线控测蜂鸣器。
- 同步更新调试命令手册，补充蜂鸣器联调与排障说明。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_beep.c`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.h`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若板卡蜂鸣器非 GPIO9，请仅调整 `WS63_BEEP_*` 配置宏，不要改 Driver/App 逻辑。

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

### 2026-04-09: 分层边界整改（App 越层与 Driver 反向依赖收口）

变更摘要：
- 新增调试 UART 驱动门面 `ws63_debug_uart`，由 Driver 层承接调试串口初始化/发送/回调注册，App 不再直接调用 BSP 调试 UART 接口。
- Middleware OSAL 新增 `ws63_os_irq_lock/ws63_os_irq_unlock/ws63_os_feed_watchdog`，用于承接 App 的临界区与喂狗需求，避免 App 直连 RTOS/HAL 原语。
- App 主任务将 `uapi_watchdog_kick` 替换为 `ws63_os_feed_watchdog`，并移除对 `watchdog.h` 与 `ws63_final_bsp.h` 的直接依赖。
- `zw101`、`ld2402` 驱动去除对 `ws63_final_osal` 的反向依赖，延时调用统一改为 `ws63_bsp_sleep_ms`，恢复 Driver -> BSP 单向依赖。
- 更新 `CMakeLists.txt`，将新驱动源文件纳入编译。

影响文件：
- `src/application/mine/ws63_final/Driver/ws63_debug_uart.h`
- `src/application/mine/ws63_final/Driver/ws63_debug_uart.c`
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/Driver/zw101.c`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && find output/ws63 -type f \( -name 'ws63_final_task.c.obj' -o -name 'ws63_final_task_debug.c.obj' -o -name 'ws63_debug_uart.c.obj' \)`
- 结果：当前输出目录未命中上述对象文件；结合当前 menuconfig 状态，`ws63_final` 模块未被纳入本次目标镜像编译链路。

后续事项：
- 若需对本次改动做“真实编译链路”验证，请先在 menuconfig 中重新启用 `CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED` 后再构建。

### 2026-04-09: Task 主文件按功能拆分（核心调度/设备控制/RGB/传感器桥接）

变更摘要：
- 将 `ws63_final_task.c` 收敛为“核心调度 + 子口初始化 + 主循环”，移除电机/蜂鸣器、RGB、传感器桥接等实现细节。
- 新增 `ws63_final_task_internal.h` 作为 Task 内部模块共享接口，统一声明内部能力初始化与状态查询接口。
- 新增 `ws63_final_task_device_ctrl.c`，集中承载电机/编码器/蜂鸣器初始化与控制 API。
- 新增 `ws63_final_task_rgb.c`，独立维护 RGB 演示状态机与周期驱动逻辑。
- 新增 `ws63_final_task_sensor_bridge.c`，独立维护 LD2402/ZW101 调试桥接接口。
- 更新 `CMakeLists.txt` 源清单，将拆分文件纳入 `mine_ws63_final` 组件编译。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_device_ctrl.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py -c ws63-liteos-app`
- 结果：clean+全量构建通过（`Build target:ws63_liteos_app success`），并生成拆分对象文件：
	`ws63_final_task.c.obj`、`ws63_final_task_device_ctrl.c.obj`、`ws63_final_task_rgb.c.obj`、`ws63_final_task_sensor_bridge.c.obj`。

后续事项：
- 若后续继续扩展业务，请优先在对应子模块文件中新增逻辑，避免再次回流到主任务文件。

### 2026-04-09: 新增 RGB 在线调试命令（INIT/SET/OFF/DEMO/STAT）

变更摘要：
- Task 层新增 RGB 控制接口：驱动重初始化、手动设色、关灯、演示模式开关、状态查询。
- RGB 子模块新增演示开关状态管理：手动设色后自动关闭演示，避免颜色被周期任务覆盖。
- 调试命令新增 `RGB INIT`、`RGB SET <R> <G> <B>`、`RGB OFF`、`RGB DEMO ON|OFF`、`RGB STAT`。
- `HELP` 输出与调试手册同步补齐 RGB 命令说明与参数约束。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若现场需要“固定色常亮”，建议先执行 `RGB DEMO OFF`，再执行 `RGB SET R G B`。

### 2026-04-10: ws63_final RTOS 多任务化改造（WK2114/SLE/RGB/BEEP 解耦）

变更摘要：
- 将原 `ws63_final_task.c` 单循环重构为“管理任务 + WK2114 通信任务 + SLE 协议任务”，并引入任务间消息队列。
- `WK2114` 与 `SLE` 之间改为队列桥接：上行由 WK2114 投递到 SLE 队列，下行由 SLE 回调投递到 WK2114 发送队列。
- `RGB` 子模块改为独立 RTOS 任务，新增控制队列，`SET/OFF/DEMO/REINIT` 命令由队列串行执行，避免与其它模块抢占执行链路。
- `BEEP` 子模块改为独立 RTOS 任务，新增控制队列，`ON/OFF/VOL` 命令由任务串行落地，减少并发硬件访问冲突。
- 中间件 `ws63_final_osal` 新增消息队列封装接口，保持 App 层不直接依赖底层 OSAL 队列细节。
- 配置层新增多任务参数（栈大小、优先级、队列深度），默认保留现有行为并支持后续按负载调参。

影响文件：
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.h`
- `src/application/mine/ws63_final/Middleware/ws63_final_osal.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_device_ctrl.c`
- `src/application/mine/ws63_final/App/Main/ws63_final_main.c`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 若现场并发负载持续升高，可优先调大 `WS63_WK2114_TX_QUEUE_DEPTH` 与 `WS63_SLE_UPLINK_QUEUE_DEPTH`。
- 若发现任务栈水位偏低，建议先提高 `WS63_SLE_TASK_STACK_SIZE`，再评估 `WS63_WK2114_TASK_STACK_SIZE`。

### 2026-04-11: TTP229 触摸键盘接入 ws63_final（GPIO16/15）

变更摘要：
- 新增 TTP229 BSP/Driver/Task 三层实现，遵循 `ws63_final` 分层架构，硬件操作仅放在 BSP。
- 固定接线配置：`SCL=GPIO16`、`SDO(板上标注 SDA)=GPIO15`，并在配置层新增时序与任务参数宏。
- 新增 TTP229 独立任务与状态机（`INIT/DISABLED/READY/FAULT`），支持运行时启停与重初始化。
- 统一按键语义为“位为 1 表示按下”，并新增“多键同时按下报警”机制。
- 调试串口新增 `TTP229 INIT/STAT/READ/MASK/WATCH/ENABLE/ALARM` 命令，便于现场联调与排障。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.h`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板重点验证 `TTP229 READ` 返回位图是否满足“位1=按下”语义。
- 若 `mask` 长期固定不变，优先检查 GPIO15/16 接线与 TTP229 时序参数。

### 2026-04-11: TTP229 实测映射落地

变更摘要：
- 按现场实测确认 TTP229 raw 映射：A=0x1000、B=0x2000、C=0x4000、D=0x8000、1=0x0001、2=0x0010、3=0x0100、*=0x0008、0=0x0080、#=0x0800。
- 多键同时按下保持按位相加语义，调试输出新增 `keys=` 可读标签，便于直接对照物理键位。
- BSP 去掉内部上拉尝试，保持只使用板上外置上拉，不再把可恢复的引脚差异当成初始化路径的一部分。

影响文件：
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`
- `src/application/mine/ws63_final/README.md`

验证结果：
- 命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 结果：构建通过（`Build target:ws63_liteos_app success`）。

后续事项：
- 上板时优先验证 `TTP229 READ` / `TTP229 WATCH ON` 的 `keys=` 输出是否与 A/B/C/D/1/2/3/*/0/# 一致。
- 如后续补充更多按键标签，只需扩展 Task 层映射表，不影响 BSP 读码。

### 2026-04-11: 智能门锁编排骨架启动（LD2402 distance + camera 子口）

变更摘要：
- LD2402 驱动开始解析 `distance:xxx` 文本输出，并把最近一次距离值暴露给任务层。
- 新增 camera 任务，统一把业务文本封装为 `[camera]xxx` 后通过 WK2114 扩展串口 3 / 115200 发送。
- 新增门锁编排任务骨架：按 LD2402 距离阈值进入接近窗口，并接收 camera 认证结果驱动开锁/锁定状态。
- 调试串口新增 `LD2402 DIST`，可直接查看最近一次距离值与更新时间。

影响文件：
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/Driver/ld2402.c`
- `src/application/mine/ws63_final/Driver/ld2402.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_internal.h`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_camera.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/CMakeLists.txt`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

验证结果：
- 命令：`python3 /home/xixi/code/fbb_ws63_20260114/src/build.py -c ws63-liteos-app`
- 结果：本次改动涉及的文件静态检查通过；完整工程构建仍被仓库内既有的 driver/pwm、driver/i2c、hal/efuse、main.c 等独立错误阻断。

后续事项：
- 待补齐 ZW101 / TTP229 到 lock manager 的认证事件上报接口，再把成功/失败消息接到 SLE 路径。
- 待确认外部 camera 模块的回包关键字后，可把 `pass/fail` 判定收敛得更严格。
