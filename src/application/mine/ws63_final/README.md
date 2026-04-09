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

## 7. 任务维护记录

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
