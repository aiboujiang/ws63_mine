# WS63 Final 串口调试命令手册

## 1. 适用范围

本手册适用于 `ws63_final` 模块中的电机、编码器、蜂鸣器、RGB、TTP229、LD2402 与 ZW101 在线控测命令。

目标：
- 通过串口实时控制电机正反转、停止、刹车与占空比。
- 通过串口实时查看编码器 RPM、窗口增量与累计计数。
- 通过串口实时控制蜂鸣器开关与频率。
- 通过串口实时控制 RGB 颜色、开关与演示模式。
- 通过串口实时读取 TTP229 按键掩码并控制状态机/报警开关。
- 通过串口在线触发 LD2402/ZW101 初始化握手及发送原始调试帧。
- 通过串口在线验证 ZW101 ZA 兼容命令链路（回显/自动登录/自动搜索/终止）。
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

### 3.5 蜂鸣器控制命令

- `BEEP ON`
  - 功能：以默认频率开启蜂鸣器连续发声。

- `BEEP ON <100-5000>`
  - 功能：以指定频率开启蜂鸣器连续发声。
  - 示例：`BEEP ON 2000`

- `BEEP FREQ <100-5000>`
  - 功能：调整蜂鸣器发声频率（未开启时会自动开启）。
  - 示例：`BEEP FREQ 1500`

- `BEEP VOL <0-100>`
  - 功能：调整蜂鸣器音量（映射为 PWM 占空比百分比）。
  - 说明：当音量设为 `0` 时会立即静音。
  - 示例：`BEEP VOL 30`

- `BEEP OFF`
  - 功能：关闭蜂鸣器并拉低引脚静音。

- `BEEP STAT`
  - 功能：查询蜂鸣器当前状态与频率。

### 3.6 RGB 控制命令

- `RGB INIT`
  - 功能：重新初始化 RGB 驱动，并恢复演示模式。

- `RGB SET <R0-255> <G0-255> <B0-255>`
  - 功能：设置 RGB 固定颜色。
  - 说明：执行后会自动关闭演示模式，避免颜色被轮询演示覆盖。
  - 示例：`RGB SET 255 80 0`

- `RGB OFF`
  - 功能：关闭 RGB（输出黑色）。

- `RGB DEMO ON`
  - 功能：开启 RGB 演示模式（红绿蓝循环）。

- `RGB DEMO OFF`
  - 功能：关闭 RGB 演示模式。

- `RGB STAT`
  - 功能：查询 RGB 驱动就绪状态与演示模式状态。

### 3.7 TTP229 调试命令

- `TTP229 INIT`
  - 功能：触发 TTP229 状态机重初始化。

- `TTP229 STAT`
  - 功能：查询 TTP229 当前就绪状态、使能状态、报警状态、原始码、按下掩码与可读键名。

- `TTP229 READ`
  - 功能：读取并输出最近一次采样缓存（`raw/mask/count/keys`）。

- `TTP229 MASK`
  - 功能：`READ` 命令别名。

- `TTP229 WATCH ON|OFF`
  - 功能：开启或关闭 TTP229 持续检测日志。
  - 说明：开启后按固定周期持续输出 `raw/mask/count`，便于观察按键触发过程。

- `TTP229 ENABLE ON|OFF`
  - 功能：控制 TTP229 状态机启停。

- `TTP229 ALARM ON|OFF`
  - 功能：控制“多键同时按下报警”开关。

说明：
- TTP229 底层已切换为 I2C 读取，GPIO16 / GPIO15 分别作为 SDA / SCL 使用。
- 当前按键语义已统一为“位为 1 表示按下”。
- 实测物理键位映射为：A=0x1000、B=0x2000、C=0x4000、D=0x8000、1=0x0001、2=0x0010、3=0x0100、*=0x0008、0=0x0080、#=0x0800。
- 调试输出里的 `keys=` 会按上述映射把多键组合成 A+B 这种文本，未知位则保留为十六进制，方便排查异常输入。
- 当 `count>=2` 且报警开启时，会输出 `multi-key alarm` 告警日志。

### 3.8 LD2402 调试命令

- `LD2402 INIT`
  - 功能：重新初始化 LD2402 模块并执行握手检测。

- `LD2402 RAW <HEX...>`
  - 功能：向 LD2402 子口发送原始十六进制命令帧。
  - 示例：`LD2402 RAW FD FC FB FA 02 00 FE 00 04 03 02 01`

- `LD2402 STAT`
  - 功能：输出当前命令映射的子串口信息。

- `LD2402 DIST`
  - 功能：输出最近一次解析到的 `distance:xxx` 距离值及其更新时间。

- `LD2402 LOG ON|OFF`
  - 功能：开启或关闭 LD2402 运行态日志输出。

- `LD2402 LOGINT <0-60000>`
  - 功能：设置 LD2402 运行态日志最小输出间隔（毫秒）。
  - 说明：`0` 表示每包都打印；推荐联调阶段使用 `500~2000`。

- `LD2402 LOGSTAT`
  - 功能：查询 LD2402 运行态日志开关与当前间隔配置。

- `SLE ULOG ON|OFF`
  - 功能：开启或关闭 SLE 上行 success 日志。

- `SLE ULOGINT <0-60000>`
  - 功能：设置 SLE 上行 success 日志最小输出间隔（毫秒）。
  - 说明：`0` 表示每次 success 都打印。

- `SLE ULOGSTAT`
  - 功能：查询 SLE 上行 success 日志开关与当前间隔配置。

### 3.9 ZW101 调试命令

- `ZW101 INIT`
  - 功能：重新初始化 ZW101 模块并执行握手检测。

- `ZW101 HANDSHAKE`
  - 功能：发送标准握手命令（0x35）并返回 ACK。

- `ZW101 CHECKSENSOR`
  - 功能：发送传感器检测命令（0x36）并返回 ACK。

- `ZW101 RAW <HEX...>`
  - 功能：向 ZW101 子口发送原始十六进制命令帧。
  - 示例：`ZW101 RAW EF 01 FF FF FF FF 01 00 03 35 00 39`

- `ZW101 STAT`
  - 功能：输出当前 ZW101 命令映射的子串口信息。

- `ZW101 ZA HELP`
  - 功能：打印 ZA 兼容命令帮助。

- `ZW101 ZA ECHO`
  - 功能：发送 `GetEcho(0x53)`，用于确认基础通信链路正常。

- `ZW101 ZA LOGIN <wait> <interval0-15> <press2|3> <id> <dup0|1>`
  - 功能：发送 `AutoLogin(0x54)`。
  - 示例：`ZW101 ZA LOGIN 10 3 2 1 0`

- `ZW101 ZA SEARCH <wait> <start> <count>`
  - 功能：发送 `AutoSearch(0x55)`，并打印返回的 `ack/id/score`。
  - 示例：`ZW101 ZA SEARCH 20 0 10`

- `ZW101 ZA SEARCHRES <buf1|2> <start> <count>`
  - 功能：发送 `SearchResBack(0x56)`，并打印返回的 `ack/id/score`。
  - 示例：`ZW101 ZA SEARCHRES 1 0 10`

- `ZW101 ZA LOGINLIGHT <wait> <press2|3> <id> <dup0|1>`
  - 功能：发送 `AutoLoginStabLight(0x57)`。
  - 示例：`ZW101 ZA LOGINLIGHT 10 2 1 0`

- `ZW101 ZA SEARCHECHO <wait> <start> <count>`
  - 功能：发送 `AutoSearchWithEcho(0x58)`，并打印返回的 `ack/id/score`。
  - 示例：`ZW101 ZA SEARCHECHO 20 0 10`

- `ZW101 ZA TERM`
  - 功能：发送 `ProcessTerminateCmd(0xAA)`，终止当前流程。

参数说明：
- `wait`：驱动等待应答的周期参数（单位 10ms，传入 `uint8`）。
- `id/start/count`：按协议使用 16 位无符号整数。
- `press2|3`：手指按压次数，当前仅允许 `2` 或 `3`。
- `dup0|1`：重复录入标志，`0` 关闭、`1` 开启。

## 4. 日志格式说明

### 4.1 命令输入日志

- 格式：`[ws63 dbg] cmd<=<COMMAND>`
- 作用：确认串口收到并开始解析该命令。

### 4.2 命令执行结果日志

- 格式：`[ws63 dbg] MOTOR <OP> ... ret=0x<code>`
- 作用：返回底层执行结果码，`0x0` 一般表示成功。

### 4.3 状态快照日志

- 格式：
  - `[ws63 dbg] <tag> dir=<DIR> motor_rpm=<rpm> out_rps=<rps>`
- 字段说明：
  - `dir`：`FWD/REV/STOP`
  - `motor_rpm`：按当前电机状态规范化后的电机轴有符号转速（正转为正，反转为负）
  - `out_rps`：按减速比换算后的输出轴转速（单位 rps，小数 3 位）

换算说明：
- `out_rps = motor_rpm / (60 * WS63_MOTOR_GEAR_RATIO)`
- 默认 `WS63_MOTOR_GEAR_RATIO=150`，可在 `ws63_final_config.h` 中调整。

## 5. 建议联调流程

1. 上电后发送 `HELP`，确认命令通道在线。
2. 发送 `MOTOR FWD 30`，观察电机正转。
3. 发送 `MOTOR WATCH ON`，连续观察状态变化。
4. 发送 `MOTOR DUTY 60`，确认占空比提升后转速变化。
5. 发送 `MOTOR REV 30`，确认方向切换与 RPM 符号变化。
6. 发送 `MOTOR STOP` 或 `MOTOR BRAKE`，确认停止行为。
7. 发送 `MOTOR WATCH OFF` 结束监控。
8. 发送 `BEEP ON 2000` + `BEEP VOL 30`，验证蜂鸣器频率与音量调节。
9. 发送 `RGB INIT`、`RGB SET 255 0 0`、`RGB DEMO ON`，验证 RGB 固定色与演示模式切换。
10. 发送 `TTP229 STAT`、`TTP229 READ`，验证触摸键盘采样与位图语义（位1=按下）。
11. 发送 `TTP229 WATCH ON` 并按键，观察持续输出的 `raw/mask/count` 是否随按键变化。
12. 发送 `TTP229 WATCH OFF`，确认持续输出停止。
13. 同时按下两个触摸键，观察是否出现多键报警日志。
14. 发送 `LD2402 INIT`、`ZW101 HANDSHAKE`，验证两类模块调试链路。
15. 发送 `ZW101 ZA ECHO`、`ZW101 ZA SEARCH 20 0 10`，验证 ZA 兼容命令通信链路。
16. 若 LD2402 上电后日志过密，优先执行 `LD2402 LOGINT 1000`，必要时执行 `SLE ULOG OFF`。

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

- 现象：LD2402 一上电日志持续刷屏。
  - 排查：先执行 `LD2402 LOGSTAT` 确认运行态日志开关与间隔配置。
  - 处置：建议 `LD2402 LOGINT 1000`（或更大）；若仍需进一步降噪，可执行 `SLE ULOG OFF`。

- 现象：转速读数不稳定。
  - 排查：检查编码器 A/B 接线与地线；必要时调大采样窗口 `WS63_ENCODER_SAMPLE_MS`。

- 现象：`BEEP ON` 命令返回成功但无声音。
  - 排查：确认蜂鸣器接线为 `GPIO9`，且板卡 IO 复用允许 `GPIO9 mode1 -> PWM1`。
  - 排查：确认未占用同一 PWM 通道；默认蜂鸣器使用 `PWM1`，电机使用 `PWM2/PWM3`。

- 现象：`RGB SET` 返回成功但颜色很快变化。
  - 说明：通常是演示模式仍处于开启状态；可先执行 `RGB DEMO OFF` 再设置固定颜色。
  - 排查：若命令返回失败，检查 `WS63_RGB_ENABLE` 是否开启并确认 SPI1 引脚连接。

- 现象：`TTP229 STAT` 中 `mask` 始终为 `0x0000` 或 `0xFFFF`。
  - 排查：确认接线为 `SCL->GPIO16`、`SDO(板上标注SDA)->GPIO15`，并检查供电与地线。
  - 排查：若接线无误，按规格书复核时序参数（起始脉冲/时钟脉冲）是否匹配当前模组。

- 现象：`LD2402 RAW` / `ZW101 RAW` 返回参数错误。
  - 排查：确保使用两位十六进制字节并用空格分隔，例如 `AA 55 01 0F`。
  - 排查：避免输入奇数字符数（例如 `A B2`），解析器会直接判定为非法。

- 现象：`ZW101 ZA *` 返回 `invalid ... args`。
  - 排查：确认参数个数与顺序匹配帮助信息；`press` 仅支持 `2/3`，`dup` 仅支持 `0/1`。
  - 排查：`id/start/count` 必须在 `0~65535`。

- 现象：`ZW101 ... ack=0x26`。
  - 说明：`0x26` 为等待 ACK 超时，表示命令已发出但在超时窗口内未收到有效应答帧。
  - 排查：优先执行 `ZW101 HANDSHAKE` 与 `ZW101 CHECKSENSOR` 观察 ACK；若均超时，检查子口接线、供电与波特率。

## 7. 关联文件

- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_ttp229.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_beep.c`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.c`
- `src/application/mine/ws63_final/Driver/ws63_ttp229.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_ttp229.c`
