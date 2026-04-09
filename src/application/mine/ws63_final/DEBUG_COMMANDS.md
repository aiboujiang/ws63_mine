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

## 3. 命令列表

所有命令均以换行结束（`\n` 或 `\r\n`），命令大小写不敏感。

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
  - `[ws63 dbg] <tag> state=<STATE> duty=<duty> rpm=<rpm> delta=<delta> total=<total>`
- 字段说明：
  - `state`：`COAST/FORWARD/REVERSE/BRAKE`
  - `duty`：当前占空比（0~100）
  - `rpm`：有符号转速，正负代表方向
  - `delta`：最近采样窗口的有符号脉冲增量
  - `total`：累计有符号脉冲计数

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

- 现象：命令返回 unknown。
  - 排查：检查命令拼写、参数范围、是否包含换行结束符。

- 现象：转速读数不稳定。
  - 排查：检查编码器 A/B 接线与地线；必要时调大采样窗口 `WS63_ENCODER_SAMPLE_MS`。

## 7. 关联文件

- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
