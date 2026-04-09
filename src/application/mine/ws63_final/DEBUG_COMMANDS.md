# WS63 Final 串口调试命令手册

## 1. 适用范围

本手册适用于 `ws63_final` 模块中的电机与编码器在线控测命令。

目标：
- 通过串口实时控制电机正反转、停止、刹车与占空比。
- 通过串口实时查看编码器 RPM、窗口增量与累计计数。
- 通过统一日志格式快速定位指令执行状态。

## 2. 默认串口配置

- 调试串口总线：`UART0`
- TX 引脚：`GPIO17`
- RX 引脚：`GPIO18`
- 波特率：`115200`
- 数据位：`8`
- 停止位：`1`
- 校验位：`None`

说明：
- 若现场串口占用冲突，可在 `ws63_final_config.h` 中修改 `WS63_DEBUG_UART_*` 宏。
- 若仍使用 `UART0`，当前实现会在初始化调试口时先 `uapi_uart_deinit(UART0)` 再 `uapi_uart_init(UART0)`，用于降低与 AT 通道并发抢读风险。

日志镜像开关：
- `WS63_DEBUG_LOG_MIRROR_SYS=0`（默认）：仅输出到调试串口，避免同口重复日志。
- `WS63_DEBUG_LOG_MIRROR_SYS=1`：同时输出到 `osal_printk` 和调试串口，便于双口观测。

## 3. 命令列表

所有命令均以换行结束（`\r`、`\n` 或 `\r\n`），命令大小写不敏感。

接收机制说明：
- 命令接收采用“UART 中断回调 + 行缓冲队列”。
- 回调仅负责按行组帧入队，命令执行在任务主循环中出队处理。
- 当串口日志很密集时，若队列已满会丢弃新命令并打印溢出提示。

### 3.1 通用命令

- `HELP`
  - 功能：打印全部可用命令。

### 3.2 电机控制命令

- `MOTOR FWD <0-100>`
  - 功能：电机正转，IA=0，IB=PWM。
  - 示例：`MOTOR FWD 40`

- `MOTOR REV <0-100>`
  - 功能：电机反转，IA=PWM，IB=0。
  - 示例：`MOTOR REV 35`

- `MOTOR DUTY <0-100>`
  - 功能：调整当前运行方向的占空比。
  - 示例：`MOTOR DUTY 60`

- `MOTOR STOP`
  - 功能：滑行停止，IA=0，IB=0。

- `MOTOR BRAKE`
  - 功能：急停刹车，IA=1，IB=1。

### 3.3 状态查询命令

- `MOTOR RPM`
  - 功能：查询电机状态与 RPM 快照。

- `MOTOR STAT`
  - 功能：查询电机状态与 RPM 快照（别名）。

- `ENCODER RESET`
  - 功能：清零编码器计数与采样状态。

### 3.4 周期监控命令

- `MOTOR WATCH ON`
  - 功能：开启周期状态日志输出。

- `MOTOR WATCH OFF`
  - 功能：关闭周期状态日志输出。

## 4. 日志格式说明

### 4.1 命令输入日志

- 格式：`[ws63 dbg] cmd<=<COMMAND>`
- 作用：确认串口收到并开始解析该命令。

### 4.2 命令执行结果日志

- 格式：`[ws63 dbg] MOTOR <OP> ... ret=0x<code>`
- 作用：返回底层执行结果码，`0x0` 一般表示成功。

### 4.3 状态快照日志

- 格式：
  - `[ws63 dbg] <tag> dir=<DIR> rpm=<rpm>`
- 字段说明：
  - `dir`：`FWD/REV/STOP`
  - `rpm`：按当前电机状态规范化后的有符号转速（正转为正，反转为负）

## 5. 建议联调流程

1. 上电后发送 `HELP`，确认命令通道在线。
2. 发送 `MOTOR FWD 30`，观察电机正转。
3. 发送 `MOTOR WATCH ON`，连续观察状态变化。
4. 发送 `MOTOR DUTY 60`，确认占空比提升后转速变化。
5. 发送 `MOTOR REV 30`，确认方向切换与 RPM 符号变化。
6. 发送 `MOTOR STOP` 或 `MOTOR BRAKE`，确认停止行为。
7. 发送 `MOTOR WATCH OFF` 结束监控。

## 6. 常见问题

- 现象：串口无响应。
  - 排查：确认串口工具参数（115200/8N1）及接线（GPIO17/18）。
  - 排查：若使用 UART0，避免同时让其他上位机工具占用同一串口做 AT 交互。

- 现象：同一条 `[ws63 dbg]` 日志重复出现两次。
  - 排查：确认 `WS63_DEBUG_LOG_MIRROR_SYS` 是否误设为 `1` 且系统日志与调试日志复用同一物理口。

- 现象：命令返回 unknown。
  - 排查：检查命令拼写、参数范围、是否包含换行结束符。

- 现象：串口工具回车后只回显、命令不立即执行。
  - 说明：当前解析器已兼容 `CR/LF/CRLF`，并使用行缓冲队列异步执行；若仍异常，优先检查串口工具发送设置是否插入了额外控制字符。

- 现象：偶发提示 `command queue overflow, dropped`。
  - 说明：表示日志压力较高且命令发送过快，建议先 `MOTOR WATCH OFF` 再下发控制命令。

- 现象：转速读数不稳定。
  - 排查：检查编码器 A/B 接线与地线；必要时调大采样窗口 `WS63_ENCODER_SAMPLE_MS`。

## 7. 关联文件

- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
