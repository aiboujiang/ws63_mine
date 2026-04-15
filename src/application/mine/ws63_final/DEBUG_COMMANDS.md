# WS63 Final SLE 调试命令手册

## 1. 适用范围

本手册适用于 `ws63_final` 模块中的电机、编码器、蜂鸣器、RGB、VK36N16I、camera、LD2402 与 ZW101 在线控测命令。

目标：
- 通过主机侧 SLE 下行实时控制电机正反转、停止、刹车与占空比。
- 通过主机侧 SLE 下行实时查看编码器 RPM、窗口增量与累计计数。
- 通过主机侧 SLE 下行实时控制蜂鸣器开关与频率。
- 通过主机侧 SLE 下行实时控制 RGB 颜色、开关与演示模式。
- 通过主机侧 SLE 下行实时读取 VK36N16I 按键掩码并控制状态机/报警开关。
- 通过主机侧 SLE 下行在线触发 camera 人脸新增/查询/删除联调。
- 通过主机侧 SLE 下行在线触发 LD2402/ZW101 初始化与业务命令联调。
- 通过主机侧 SLE 下行在线验证 ZW101 重构能力链路（ENROLL/VERIFY/ECHO/LIST/DEL/CLEAR/CANCEL）。
- 通过统一日志格式快速定位指令执行状态。

调试会话说明：
- 主机输入 `DEBUG INIT` 后，系统进入纯调试模式。
- 纯调试模式下保留全部调试命令，但门锁编排任务会被延后启动或保持挂起。
- 退出纯调试模式使用 `DEBUG EXIT`。

## 2. 默认调试通道配置

- 命令入口：主机侧 SLE 下行
- 调试日志出口：主机侧 SLE 上行（`[DEBUG]` 标签）
- ws63_final 本机调试 UART：默认关闭

说明：
- 若需临时启用 ws63_final 本机调试 UART，可在 `ws63_final_config.h` 中将 `WS63_DEBUG_LOCAL_UART_IO_ENABLE` 置为 `1`。
- 默认推荐保持 `WS63_DEBUG_LOCAL_UART_IO_ENABLE=0`，避免本机串口与 SLE 双入口并发导致命令源不一致。

关键开关：
- `WS63_DEBUG_STRICT_SLE_ONLY=1`：开启严格模式（仅允许 SLE 下行命令 + SLE 上行调试日志）。
- `WS63_DEBUG_SLE_CMD_ENABLE=1`：开启 SLE 下行命令入口。
- `WS63_DEBUG_SLE_LOG_ENABLE=1`：开启调试日志 SLE 上行。
- `WS63_DEBUG_LOCAL_UART_IO_ENABLE=0`：关闭 ws63_final 本机调试 UART 收发。

主机侧推荐开关（`sle_uart_host`）：
- `MINE_HOST_STRICT_CMD_INPUT_ENABLE=1`：只允许指定命令 UART 输入口进入 SLE 下发链路。
- `MINE_HOST_CMD_UART_BUS=UART_BUS_0`：默认主机命令输入口为 UART0。
- `MINE_UART_ENABLE_MASK=MINE_UART_EN_UART0`：默认仅启用 UART0，避免非命令口噪声误下发。

## 3. 命令列表

所有命令均以换行结束（`\r`、`\n` 或 `\r\n`），命令大小写不敏感。

接收机制说明：
- 命令接收采用“SLE 下行 + 行缓冲队列”。
- 下行入口仅负责按行组帧入队，命令执行在任务主循环中出队处理。
- 当串口日志很密集时，若队列已满会丢弃新命令并打印溢出提示。

### 3.1 通用命令

- `DEBUG INIT`
  - 功能：进入纯调试模式。
  - 说明：建议在启动观察窗口内发送该命令，避免门锁编排任务被拉起。

- `DEBUG EXIT`
  - 功能：退出纯调试模式，恢复正常门锁流程。

- `DEBUG STAT`
  - 功能：查询当前调试会话模式是否为纯调试模式。

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

### 3.7 VK36N16I 调试命令

- `VK36N16I INIT`
  - 功能：触发 VK36N16I 状态机重初始化。

- `VK36N16I STAT`
  - 功能：查询 VK36N16I 当前就绪状态、使能状态、报警状态、原始码、按下掩码与可读键名。

- `VK36N16I READ`
  - 功能：读取并输出最近一次采样缓存（`raw/mask/count/keys`）。

- `VK36N16I MASK`
  - 功能：`READ` 命令别名。

- `VK36N16I WATCH ON|OFF`
  - 功能：开启或关闭 VK36N16I 持续检测日志。
  - 说明：开启后按固定周期持续输出 `raw/mask/count`，便于观察按键触发过程。

- `VK36N16I ENABLE ON|OFF`
  - 功能：控制 VK36N16I 状态机启停。

- `VK36N16I ALARM ON|OFF`
  - 功能：控制“多键同时按下报警”开关。

说明：
- VK36N16I 底层已切换为 I2C 读取，GPIO16 / GPIO15 分别作为 SDA / SCL 使用。
- 当前按键语义已统一为“位为 1 表示按下”。
- 实测物理键位映射为：A=0x1000、B=0x2000、C=0x4000、D=0x8000、1=0x0001、2=0x0010、3=0x0100、*=0x0008、0=0x0080、#=0x0800。
- 调试输出里的 `keys=` 会按上述映射把多键组合成 A+B 这种文本，未知位则保留为十六进制，方便排查异常输入。
- 当 `count>=2` 且报警开启时，会输出 `multi-key alarm` 告警日志。

### 3.8 camera 调试命令

- `[camera]add <id>`
  - 功能：新增一张人脸，调试层会自动补前缀后下发到 camera 子口。
  - 示例：`[camera]add 1`

- `[camera]List`
  - 功能：查询当前已登记的人脸列表。
  - 返回格式：`[sum,id1,id2,...]`
  - 说明：`sum` 表示当前人脸数量，后续依次是各人脸 `id`。

- `[camera]Del <id>`
  - 功能：删除指定 `id` 的人脸。
  - 示例：`[camera]Del 1`

- `add` / `Del` 的完成态回包
  - 功能：camera 侧在新增或删除完成后，可能返回类似 `xxx complete` 的文本。
  - 说明：这类回包按普通文本日志处理，不影响上层联调；例如 `add complete`、`Del complete`、`camera complete` 都可视为完成态返回。

- `[camera]start`
  - 功能：camera 生命周期调试别名，实际会下发 `action`。

- `[camera]die`
  - 功能：camera 生命周期调试别名，实际会下发 `Die`。

说明：
- 调试层会把命令统一整理为实际发送文本 `[camera]add <id>`、`[camera]List`、`[camera]Del <id>`、`[camera]action`、`[camera]Die`。
- camera 回包会按原文落到日志里，格式为 `[camera] rx <reply>`。
- 当前实现对 `List` 回包不做二次改写，直接接受并透传上述 `[sum,id1,id2,...]` 形式。
- `add` / `Del` 的完成态回包若出现 `xxx complete`，同样按成功完成文本处理。

### 3.9 LD 调试命令

- `LD HELP`
  - 功能：打印 LD 全部调试命令。

- `LD INIT`
  - 功能：重新初始化 LD 模块并执行握手检测。

- `LD STAT`
  - 功能：输出 LD 就绪状态、配置模式状态、日志开关与距离快照。

- `LD VERSION`
  - 功能：读取 LD 固件版本号。

- `LD SN`
  - 功能：读取 LD 序列号，优先显示字符形式，失败后回退十六进制形式。

- `LD MODE NORMAL|ENGINEERING`
  - 功能：切换 LD 输出模式。

- `LD DIST`
  - 功能：输出最近一次解析到的距离值及其更新时间。

- `LD DIST <0.7~10.0>`
  - 功能：设置 LD 最大探测距离，单位米。

- `LD DELAY <0-65535>`
  - 功能：设置目标消失延迟时间，单位秒。

- `LD GET <param_id>`
  - 功能：读取任意参数 ID 的当前值。

- `LD SET <param_id> <value>`
  - 功能：写入任意参数 ID 的当前值。

- `LD SAVE`
  - 功能：保存当前参数到掉电区。

- `LD GAIN`
  - 功能：触发上电自动增益调节。

- `LD AUTO <trig10x> <hold10x> [static10x]`
  - 功能：开始自动门限生成。

- `LD PROGRESS`
  - 功能：查询自动门限生成进度。

- `LD ALARM`
  - 功能：查询自动门限干扰状态。

- `LD PWR`
  - 功能：读取电源干扰参数。

- `LD SAVE3F`
  - 功能：执行 0x003F 读后回写并保存参数。

- `LD RAW <HEX...>`
  - 功能：向 LD 子口发送原始十六进制命令帧。
  - 示例：`LD RAW FD FC FB FA 02 00 FE 00 04 03 02 01`

- `LD LOG ON|OFF`
  - 功能：开启或关闭 LD 运行态日志输出。

- `LD LOGINT <0-60000>`
  - 功能：设置 LD 运行态日志最小输出间隔（毫秒）。
  - 说明：`0` 表示每包都打印；推荐联调阶段使用 `500~2000`。

- `LD LOGSTAT`
  - 功能：查询 LD 运行态日志开关与当前间隔配置。

说明：
- 当前实现仅接受 `LD ...` 前缀。
- `LD GET/SET` 支持手册 5.2.6/5.2.7 的通用参数读写路径，便于直接调试未单独封装的参数。
- `LD SAVE3F` 对应手册 5.5 的 0x003F 读后回写流程，适合需要掉电保存的参数场景。

- `SLE ULOG ON|OFF`
  - 功能：开启或关闭 SLE 上行 success 日志。

- `SLE ULOGINT <0-60000>`
  - 功能：设置 SLE 上行 success 日志最小输出间隔（毫秒）。
  - 说明：`0` 表示每次 success 都打印。

- `SLE ULOGSTAT`
  - 功能：查询 SLE 上行 success 日志开关与当前间隔配置。

### 3.10 ZW101 调试命令

- `ZW HELP`
  - 功能：打印 ZW 命令帮助。

- `ZW INIT`
  - 功能：重新初始化 ZW101 模块并执行探测。

- `ZW STAT`
  - 功能：输出当前 ZW 命令映射的子串口信息。

- `ZW ECHO`
  - 功能：发送 `GetEcho(0x53)`，用于确认基础通信链路正常。

- `ZW VERIFY [score1-5] [id]`
  - 功能：发送 `AutoIdentify(0x32)` 并打印 `ack/id/score`。
  - 默认：`score=3`、`id=0xFFFF`（1:N）、`param=0x0000`。
  - 示例：`ZW VERIFY`、`ZW VERIFY 4`、`ZW VERIFY 3 1`

- `ZW ENROLL <id> [times2-6]`
  - 功能：发送 `AutoEnroll(0x31)`。
  - 默认：`times=3`，参数位默认禁止重复登记（bit4=1）。
  - 示例：`ZW ENROLL 1`、`ZW ENROLL 1 3`

- `ZW LIST`
  - 功能：发送 `ReadValidTemplateNum(0x1D)` 并打印有效模板数量。

- `ZW DEL <id> [count]`
  - 功能：发送 `Delete(0x0C)` 删除模板。
  - 示例：`ZW DEL 1`、`ZW DEL 1 2`

- `ZW CLEAR`
  - 功能：发送 `Empty(0x0D)` 清空模板库。

- `ZW CANCEL`
  - 功能：发送 `Cancel(0x30)` 终止当前流程。

## 4. 日志格式说明

### 4.1 命令输入日志

- 格式：`[ws63 dbg] cmd<=<COMMAND>`
- 作用：确认从主机 SLE 收到并开始解析该命令。

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

1. 上电后通过主机侧 SLE 发送 `HELP`，确认命令通道在线。
2. 发送 `MOTOR FWD 30`，观察电机正转。
3. 发送 `MOTOR WATCH ON`，连续观察状态变化。
4. 发送 `MOTOR DUTY 60`，确认占空比提升后转速变化。
5. 发送 `MOTOR REV 30`，确认方向切换与 RPM 符号变化。
6. 发送 `MOTOR STOP` 或 `MOTOR BRAKE`，确认停止行为。
7. 发送 `MOTOR WATCH OFF` 结束监控。
8. 发送 `BEEP ON 2000` + `BEEP VOL 30`，验证蜂鸣器频率与音量调节。
9. 发送 `RGB INIT`、`RGB SET 255 0 0`、`RGB DEMO ON`，验证 RGB 固定色与演示模式切换。
10. 发送 `VK36N16I STAT`、`VK36N16I READ`，验证触摸键盘采样与位图语义（位1=按下）。
11. 发送 `VK36N16I WATCH ON` 并按键，观察持续输出的 `raw/mask/count` 是否随按键变化。
12. 发送 `VK36N16I WATCH OFF`，确认持续输出停止。
13. 同时按下两个触摸键，观察是否出现多键报警日志。
14. 发送 `LD INIT`、`ZW INIT`，验证两类模块调试链路。
15. 发送 `ZW ECHO`、`ZW VERIFY`，验证 VERIFY 认证链路。
16. 发送 `ZW LIST`、`ZW DEL 1`、`ZW CLEAR`，验证模板管理命令链路。
17. 若 LD 上电后日志过密，优先执行 `LD LOGINT 1000`，必要时执行 `SLE ULOG OFF`。
18. 主机串口建议仅在 `UART0` 输入命令，确认 `[mine host][DEBUG]` 可稳定回显从机调试日志。

## 6. 常见问题

- 现象：主机 SLE 下发命令无响应。
  - 排查：确认主机与 ws63_final 的 SLE 链路已连接。
  - 排查：确认 `WS63_DEBUG_SLE_CMD_ENABLE` 是否为 `1`。
  - 排查：若主机启用了严格入口，确认命令是否从 `MINE_HOST_CMD_UART_BUS`（默认 UART0）输入。

- 现象：发送 `DEBUG INIT` 后门锁任务仍然起来了。
  - 说明：`DEBUG INIT` 需要在启动观察窗口内生效；窗口大小由 `WS63_DEBUG_BOOT_DECISION_MS` 控制。
  - 处置：重启后尽早发送 `DEBUG INIT`，或在纯调试模式中使用 `DEBUG EXIT` 恢复正常流程。

- 现象：ws63_final 本机串口没有调试日志。
  - 说明：默认行为。当前版本要求调试日志经 SLE 上行，不从 ws63_final 本机串口输出。
  - 排查：确认 `WS63_DEBUG_SLE_LOG_ENABLE` 是否为 `1`。

- 现象：主机 `UART2` 输入命令无效。
  - 说明：严格模式下主机只接受 `MINE_HOST_CMD_UART_BUS` 的命令输入。
  - 处置：改为 `UART0` 输入，或按需调整 `MINE_HOST_CMD_UART_BUS` 并重新编译。

- 现象：同一条 `[ws63 dbg]` 日志重复出现两次。
  - 排查：确认主机侧工具未对同一 SLE 上行数据做重复打印。

- 现象：命令返回 unknown。
  - 排查：检查命令拼写、参数范围、是否包含换行结束符。

- 现象：串口工具回车后只回显、命令不立即执行。
  - 说明：当前解析器已兼容 `CR/LF/CRLF`，并使用行缓冲队列异步执行；若仍异常，优先检查串口工具发送设置是否插入了额外控制字符。

- 现象：偶发提示 `command queue overflow, dropped`。
  - 说明：表示日志压力较高且命令发送过快，建议先 `MOTOR WATCH OFF` 再下发控制命令。

- 现象：LD 一上电日志持续刷屏。
  - 排查：先执行 `LD LOGSTAT` 确认运行态日志开关与间隔配置。
  - 处置：建议 `LD LOGINT 1000`（或更大）；若仍需进一步降噪，可执行 `SLE ULOG OFF`。

- 现象：转速读数不稳定。
  - 排查：检查编码器 A/B 接线与地线；必要时调大采样窗口 `WS63_ENCODER_SAMPLE_MS`。

- 现象：`BEEP ON` 命令返回成功但无声音。
  - 排查：确认蜂鸣器接线为 `GPIO9`，且板卡 IO 复用允许 `GPIO9 mode1 -> PWM1`。
  - 排查：确认未占用同一 PWM 通道；默认蜂鸣器使用 `PWM1`，电机使用 `PWM2/PWM3`。

- 现象：`RGB SET` 返回成功但颜色很快变化。
  - 说明：通常是演示模式仍处于开启状态；可先执行 `RGB DEMO OFF` 再设置固定颜色。
  - 排查：若命令返回失败，检查 `WS63_RGB_ENABLE` 是否开启并确认 SPI1 引脚连接。

- 现象：`VK36N16I STAT` 中 `mask` 始终为 `0x0000` 或 `0xFFFF`。
  - 排查：确认接线为 `SCL->GPIO16`、`SDO(板上标注SDA)->GPIO15`，并检查供电与地线。
  - 排查：若接线无误，按规格书复核时序参数（起始脉冲/时钟脉冲）是否匹配当前模组。

- 现象：`LD RAW` 返回参数错误。
  - 排查：确保使用两位十六进制字节并用空格分隔，例如 `AA 55 01 0F`。
  - 排查：避免输入奇数字符数（例如 `A B2`），解析器会直接判定为非法。

- 现象：`ZW VERIFY` / `ZW ENROLL` / `ZW DEL` 返回 `invalid ... args`。
  - 排查：确认参数个数与顺序匹配帮助信息；`score` 仅支持 `1~5`，`times` 仅支持 `2~6`。
  - 排查：`id/count` 必须在 `0~65535`，且 `count` 不能为 `0`。

- 现象：`ZW ... ack=0x26`。
  - 说明：`0x26` 为等待 ACK 超时，表示命令已发出但在超时窗口内未收到有效应答帧。
  - 排查：优先执行 `ZW INIT` 与 `ZW ECHO` 观察 ACK；若仍超时，检查子口接线、供电与波特率。

- 现象：进入 `ARMED -> VERIFYING` 后“未触摸也快速成功”。
  - 说明：详细追踪默认关闭；需要定位“本次命令真实 ACK”还是“历史/异步帧误命中”时，再临时打开追踪宏。
  - 关注日志1（驱动层，开启详细追踪后）：`[zw101 trace] seq=... send cmd=0x32 ...`，确认每次 VERIFY 都有唯一 `seq`。
  - 关注日志2（驱动层，开启详细追踪后）：`[zw101 trace] ... ack=0x.. payload_len=..` + `ack_payload data=...`，直接看原始 ACK 码和载荷字节。
  - 关注日志3（任务层，开启详细追踪后）：`[zw101 trace] task_before_verify/task_after_verify`，确认请求位、取消位、禁用位与 fail_streak。
  - 关注日志4（锁管理）：`[lock mgr trace] auth_event_enqueue/auth_event_handle`，确认认证结果入队/出队与状态机状态是否一致。
  - 判定建议：若 `seq` 连续但 `ack_payload` 内容固定异常（例如 `id/score` 恒定），优先怀疑模块侧返回语义或参数口径；若 `seq` 与结果错位，优先怀疑串口缓存残留或并发读写时序。

- VERIFY SUCCESS 判定口径（对齐 slave）：
  - 必须满足 `ack=0x00` 且阶段码 `p1=0x05(SEARCH)` 才进入终态成功判定。
  - 终态成功还需 `id != 0xFFFF` 且 `score > 0`；否则按失败处理，避免中间态/异常载荷误判。
  - 若看到 `ack_payload=00 00 FF FF 00 00`，这是 `LEGAL_CHECK` 阶段中间态，不能判定为成功。

- 验证失败重试策略（新增）：
  - ZW101 失败后不再立即重试，而是先进入 `wait_release` 状态。
  - 如果本次结果是 `ACK_TIMEOUT`，且门锁仍处于当前 ARMED 生命周期内，任务层会先自动重拉一次 VERIFY；只有超时重试耗尽后，才会回落到普通失败上报。
  - 任务会周期执行 `CheckSensor(0x36)` 检测是否离手：
  - `ack=0x00` 视为仍按压；`ack=0x02` 视为已离手。
  - 仅在检测到离手后，才会重新排队下一次 VERIFY。
  - 关键日志：`task_wait_finger_release`、`release_check ... finger=RELEASED`、`finger released, retry verify queued`。

- ZW101 禁用作用域（更新）：
  - 连续失败触发的 `VERIFY disabled` 仅在当前 ARMED 窗口内生效。
  - 每次从 IDLE 新进入 ARMED 时，会自动重置 `disabled/fail_streak`，不继承上一个窗口状态。
  - 关键日志：`reset_armed_window_guard`。

- 现象：`request_verify` 已投递，但日志长期显示 `ready=0`，最终 `auth window timeout`。
  - 说明：当前版本已加入 ready 自愈重试；当 ARMED 且有请求时，会周期触发 `ws63_task_ensure_zw101_ready()`。
  - 关注日志：`[zw101 trace] ready_recover ret=0x...` 与 `[wk2114 final task] ZW101 ready=0, force reinit`。
  - 判定建议：
  - 若 `ready_recover ret=0x0` 且随后出现 `VERIFYING`，说明是一次性惰性初始化失败已被自愈。
  - 若连续非 0，优先排查 `ZW101_SUBPORT` 使能配置、供电与子口连线。

- 现象：`0x53/0x35` 偶尔成功，但 `0x36` 连续超时，同时出现 `U1 rx len=12 first=0x..` 默认回调日志。
  - 说明：这是初始化阶段 ACK 被 worker 线程先读走、且未进入 `zw101_process_data` 的典型竞态。
  - 修复点：当前版本已改为“先绑定 ZW101 回调，再执行 `zw101_init`”，并在 force reinit 前再次强制绑定。

- 现象：异常日志提示 `task:ws63_zw101_task stack overflow`。
  - 说明：ZW101 任务链路包含协议等待与自愈初始化，默认 2KB 栈在高日志场景存在溢出风险。
  - 修复点：当前版本已把 ZW101 任务栈提升为 `WS63_ZW101_TASK_STACK_SIZE=3072`。

## 7. 关联文件

- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/Config/ws63_final_config.h`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_vk36n16i.c`
- `src/application/mine/ws63_final/BSP/ws63_final_bsp_beep.c`
- `src/application/mine/ws63_final/Driver/ws63_buzzer.c`
- `src/application/mine/ws63_final/Driver/ws63_vk36n16i.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_rgb.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_vk36n16i.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_sensor_bridge.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_lock_mgr.c`
- `src/application/mine/ws63_final/Driver/zw101.c`

## 8. 2026-04-14 CAMERA 上行分包修复

变更摘要：
- 修复主机侧 `sle->uart` 日志里同一条 camera 结果被拆成两条 `[CAMERA]` 的问题。
- `ws63_final_task` 不再把 CAMERA 子口原始分片直接上行；改为由 `ws63_final_task_camera` 在重组出完整文本后一次性上行。
- 保持现有协议标签不变（仍为 `[CAMERA]`），仅改变上行时机，避免 Host 侧出现 `len:24 + len:10` 这类拆包打印。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task.c`
- `src/application/mine/ws63_final/App/Task/ws63_final_task_camera.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

验证与结果：
- 构建命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 构建结果：通过；日志包含 `Build target:ws63_liteos_app success` 与 `packet success!`。

联调观察要点：
- 修复前：主机可能连续看到 `sle->uart len:24` 与 `sle->uart len:10`，并分别打印 `[CAMERA][name,score` / `[CAMERA]]`。
- 修复后：同一条识别结果应尽量以单条完整 `[CAMERA][name,score]` 形式上行。

## 9. 2026-04-14 CAMERA ADD/DEL 首条无响应修复

变更摘要：
- 修复主机侧执行 `camera add <id>` / `camera Del <id>` 后摄像头偶发无反应、需再发 `camera List` 才开始采集的问题。
- 调试命令链路新增 `ADD` 与 `DEL` 专用流程：先发送 `action` 唤醒采集态，再等待唤醒间隔，随后发送 `add <id>` 或 `Del <id>`，最后自动补一条 `List` 完成刷新/触发。
- `camera` 调试命令统一改为走 `ws63_task_camera_send_message()` 队列发送，复用 camera 任务的串行与重试机制，避免冷启动阶段首包不稳定。
- 新增详细日志：调试层会打印 `camera#N` 序号和 `CAMERA cmd raw/op/send begin/ready/send result`，camera 任务会打印 `queue recv/tx try/tx ok/tx fail`，camera 回包会打印 `rx chunk/full reply/uplink queued`，主机侧会打印 `[mine host][CAMERA]` 预览。

影响文件：
- `src/application/mine/ws63_final/App/Task/ws63_final_task_debug.c`
- `src/application/mine/ws63_final/DEBUG_COMMANDS.md`

验证与结果：
- 构建命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`
- 构建结果：通过；日志包含 `Build target:ws63_liteos_app success` 与 `packet success!`。

联调建议：
- 先执行 `camera add 333`，观察是否直接进入采集流程，无需再补发 `camera List`。
- 再执行 `camera Del 333`，观察是否可直接生效，无需先发 `camera List`。
- 当前实现会在 ADD/DEL 后自动补发 `camera List`，用于兼容 camera 固件把 List 作为最终触发动作的情况。
- 若现场仍异常，请优先对照 `[camera#N]` 序号、`CAMERA send begin`、`queue recv`、`rx chunk`、`full reply`、`uplink queued` 这些日志判断卡点。
- 如需手动控制生命周期，仍可使用 `camera start` / `camera die`。
