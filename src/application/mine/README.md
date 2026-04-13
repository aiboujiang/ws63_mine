# Mine 应用说明

本目录包含 mine 自定义应用，核心是基于 SLE 的主从 UART 双向透传，并在从机侧按模块化方式挂载外设业务（Camera、LD2402、ZW101）。

## 1. 功能概览

1. `sle_uart_host`（主机，SLE Server）：
   - 负责广播、建链、服务端特征通知。
   - 将主机 UART 数据转发到从机。
2. `sle_uart_slave`（从机，SLE Client）：
   - 负责扫描、连接、订阅主机服务。
   - 将从机 UART 数据转发到主机。
   - 可选挂载 Camera、LD2402 雷达与 ZW101 指纹业务模块。

## 2. 目录结构

- `application/mine/sle_uart_host`
  - `src/sle_uart_host.c`：主机主流程。
  - `src/sle_uart_host_adv.c`：广播参数配置。
  - `src/sle_uart_host_ssaps.c`：SSAPS 服务相关。
  - `src/sle_uart_host_oled.c`：主机 OLED 调试显示。
- `application/mine/sle_uart_slave`
  - `src/sle_uart_slave.c`：从机主流程与模块调度。
  - `src/sle_uart_slave_ld2402.c`：LD2402 业务模块。
  - `src/sle_uart_slave_zw101.c`：ZW101 业务模块。
  - `src/sle_uart_slave_ssapc.c`：SSAP 客户端流程。
  - `src/sle_uart_slave_oled.c`：从机 OLED 调试显示。
- `application/mine/common/LD2402`
  - LD2402 协议栈与命令封装。
- `application/mine/common/ZW101`
  - ZW101 协议栈与命令封装。
- `application/mine/common/ssd1306`
  - SSD1306 OLED 驱动。

## 3. 编译配置

在 `src` 目录执行：

```bash
python3 build.py menuconfig
```

在菜单中选择：

1. `Application -> Enable Mine Demos`
2. `Application -> Mine SLE UART Demo Role`
   - 主机镜像：`Support Mine SLE UART Host Demo.`
   - 从机镜像：`Support Mine SLE UART Slave Demo.`

注意：主机与从机需分别编译并烧录到不同板卡。

## 4. 默认关键参数

1. SLE 参数
   - Host 名称：`mine_sle_host`
   - Slave 名称：`mine_sle_slave`
   - Service UUID：`0xABCD`
   - Property UUID：`0xBCDE`
2. UART 参数
   - `UART0`：主链路调试与透传（默认调试口）。
   - `UART2`：默认用于外挂模块（Camera/LD2402/ZW101）。
   - 默认使能掩码：`UART0 + UART2`。
3. 从机业务默认开关（见 `sle_uart_slave/inc/sle_uart_slave.h`）
   - `MINE_UART2_MODE_CAMERA_ENABLE = 0`
   - `MINE_UART2_MODE_LD2402_ENABLE = 1`
   - `MINE_UART2_MODE_ZW101_ENABLE = 0`
   - 三者必须三选一（编译期互斥校验）。
   - `MINE_CAMERA_ENABLE` / `MINE_LD2402_ENABLE` / `MINE_ZW101_ENABLE` 由上述 UART2 模式自动推导。
   - `MINE_CAMERA_UART_BUS = UART2`
   - `MINE_CAMERA_DEBUG_CMD_ENABLE = 1`
   - `MINE_CAMERA_DEBUG_UART_BUS = UART0`
   - `MINE_LD2402_UART_BUS = UART2`
   - `MINE_LD2402_DEBUG_CMD_ENABLE = 1`
   - `MINE_LD2402_DEBUG_UART_BUS = UART0`

## 5. 从机模块化主流程

从机 `sle_uart_slave.c` 主循环仅负责调度，业务能力下沉到独立模块：

1. UART 回调收到数据后，先按设备总线喂入对应协议解析器。
2. 若命中调试命令前缀（例如 Camera 的 `CAM`、LD2402 的 `LD`），本地消费，不透传。
3. 非命令数据继续走原有 UART <-> SLE 双向透传链路。
4. 主循环周期调用各业务模块 `process`，串行执行调试命令，避免并发访问协议上下文。

## 6. Camera 模式（从机）

### 6.1 模块说明

1. 适用场景：UART2 连接摄像头串口时，作为 Camera 控制与数据采集通道。
2. 模式开关：`MINE_UART2_MODE_CAMERA_ENABLE = 1`（其余两个模式必须为 `0`）。
3. 调试命令输入总线：`MINE_CAMERA_DEBUG_UART_BUS`（默认 `UART0`）。

### 6.2 Camera 串口调试命令

在 `MINE_CAMERA_DEBUG_UART_BUS` 输入文本命令并回车：

```text
CAM START
```

命令行为：

1. 识别到 `CAM START` 后，从机会向 `UART2` 输出：`start collect`。
2. 该命令由从机本地消费，不透传到 Host。

## 7. LD2402 雷达模块（从机）

### 7.1 模块说明

1. 协议封装目录：`application/mine/common/LD2402`
2. 业务接入目录：`application/mine/sle_uart_slave/src/sle_uart_slave_ld2402.c`
3. 已支持能力（按手册 V1.08 对齐）：
   - 版本号与 SN 读取。
   - 工作模式切换（普通/工程）。
   - 最大距离、消失延时、自动增益、参数保存。
   - 自动门限触发、进度查询、干扰状态查询。
   - 电源干扰参数查询。
   - `0x003F` 刷新后保存流程（`SAVE3F`）。

### 7.2 LD2402 串口调试命令

在 `MINE_LD2402_DEBUG_UART_BUS` 对应串口输入文本命令并回车。命令前缀支持 `LD` 或 `LD2402`，不区分大小写。

```text
LD HELP
LD STATUS
LD VERSION
LD SN
LD MODE NORMAL
LD MODE ENGINEERING
LD DIST <0.7~10.0>
LD DELAY <sec>
LD GAIN
LD SAVE
LD AUTO <trig10x> <hold10x> [static10x]
LD PROGRESS
LD ALARM
LD PWR
LD SAVE3F
```

参数说明：

1. `LD DIST`：单位米，允许一位小数，范围 `0.7~10.0`。
2. `LD AUTO`：三个参数为 10 倍系数，范围 `10~200`；第三个参数可省略，省略时默认等于第二个参数。
3. `LD PROGRESS`：查询自动门限进度（百分比）。
4. `LD ALARM`：查询自动门限干扰状态与门位图。
5. `LD SAVE3F`：执行 `0x003F` 读后回写，再执行保存。

### 7.3 推荐联调顺序

1. `LD STATUS`
2. `LD VERSION`
3. `LD SN`
4. `LD MODE ENGINEERING`
5. `LD AUTO 120 100 100`
6. 每隔 1~2 秒执行 `LD PROGRESS`
7. `LD ALARM` + `LD PWR`
8. `LD SAVE3F`
9. `LD SAVE`
10. `LD MODE NORMAL`

## 8. ZW101 指纹模块（从机）

1. 协议封装目录：`application/mine/common/ZW101`
2. 业务接入目录：`application/mine/sle_uart_slave/src/sle_uart_slave_zw101.c`
3. 核心流程（按《指纹模组产品用户手册_V1.5.1》实现）：
   - 自动注册模板：`PS_AutoEnroll (0x31)`
   - 自动验证指纹：`PS_AutoIdentify (0x32)`
   - 删除模板：`PS_DeletChar (0x0C)`
4. 常用配置宏：
   - `MINE_UART2_MODE_ZW101_ENABLE`
   - `MINE_ZW101_UART_BUS`
   - `MINE_ZW101_UART_BAUD`
   - `MINE_ZW101_DEBUG_CMD_ENABLE`
   - `MINE_ZW101_DEBUG_UART_BUS`
5. 调试命令前缀：`FP` 或 `ZW101`。
6. 典型命令：
   - `FP HELP`
   - `FP STATUS`
   - `FP LIST`
   - `FP ENROLL <id> [times]`
   - `FP VERIFY [score] [id]`
   - `FP DEL <id> [count]`
   - `FP CLEAR`
   - `FP CANCEL`
7. 日志与状态：
   - 自动注册会输出关键阶段日志（合法性/采图/特征/合模/存储）。
   - 自动验证会输出关键阶段日志（合法性/采图/检索）与匹配 `id/score`。
   - 状态文本通过 `mine_zw101_get_status` 对 OLED 侧输出，便于现场联调。

## 9. WK2114 UART2 扩展模块（可选）

1. 目录：`application/mine/wk2114_uart2_ext`
2. 功能：使用 `UART2` 作为 WK2114 主口，将单路主 UART 扩展为 4 路子 UART。
3. 使能开关：
   - `Application -> Mine -> Support Mine UART2 Expand via WK2114.`
4. OLED 调试输出：
   - 标题与状态：当前初始化进度、错误状态。
   - 数据预览：最近一条主口 RX/TX 命令帧（ASCII 可见字符 + 截断显示）。
   - 统计信息：RX/TX 最近长度与累计次数。
   - 通道信息：当前子串口号与波特率（例如 `CH:U1 B:115200`）。
5. ZW101 独立测试（WK2114 子串口1）：
   - 通信路径：`WS63 UART2 -> WK2114 -> SubPort1 -> ZW101`。
   - 子串口1默认波特率：`57600`（对齐 ZW101 手册默认值）。
   - 调试口：`UART0`，命令前缀固定 `ZW101`。
   - 支持命令：
     - `ZW101 ENROLL <id> [times]`
     - `ZW101 VERIFY [score] [id]`
     - `ZW101 DEL <id> [count]`
   - 说明：命令执行过程中模块会输出 ACK 码与释义，便于现场定位失败原因。

## 9.1 WK2114 最终版分层框架（可选）

1. 目录：`application/mine/ws63_final`
2. 主从定位：
   - 主控是 `WS63`。
   - `WK2114` 是外设扩展芯片，作为 Driver 层管理对象。
3. 使能开关：
   - `Application -> Mine -> Support Mine WS63 final layered framework (WS63 master).`
4. 分层目录结构：

```text
ws63_final/
├── Config/
├── Common/
├── BSP/
├── Driver/
├── Middleware/
├── App/Task/
└── App/Main/
```

5. 依赖规则：
   - 上层只能调用下层，下层绝不调用上层。
   - 硬件访问只允许在 `BSP`（WS63 的 GPIO/UART/IRQ 访问）。
   - `Driver` 只做 WK2114 寄存器/FIFO 封装与统一收发接口。
   - `App` 只做业务编排与模块回调，不允许直接操作硬件。
6. 模块对接方式（后续整合各业务模块时使用）：
   - 使用 `mine_ws63_final_task_register_rx_callback()` 注册子串口接收处理。
   - 使用 `mine_ws63_final_task_send()` 向指定子串口发送业务数据。

## 11. 快速联调步骤



1. 分别编译主机与从机镜像并烧录。
2. 先上电主机，再上电从机，确认从机完成扫描并连接主机。
3. 打开两路串口工具：
   - 主机串口发数据，观察从机串口输出。
   - 从机串口发数据，观察主机串口输出。
4. 若启用 LD2402，在从机调试串口执行 `LD HELP` 验证命令通道是否正常。

## 12. 常见问题

1. 现象：`LD` 命令无响应。
   - 排查 `MINE_LD2402_DEBUG_CMD_ENABLE` 与 `MINE_LD2402_DEBUG_UART_BUS`。
   - 确认命令以回车换行结束。
2. 现象：`RADAR:NOT READY`。
   - 排查 LD2402 所在 UART 引脚、波特率、供电与连线。
3. 现象：编译报错提示 UART2 mode must be mutually exclusive。
   - `MINE_UART2_MODE_CAMERA_ENABLE` / `MINE_UART2_MODE_LD2402_ENABLE` / `MINE_UART2_MODE_ZW101_ENABLE`
     必须且只能有一个为 `1`。
4. 现象：`FP STATUS`、`FP VERIFY` 等短命令无回显。
   - 已兼容“无 CRLF 结尾”的命令帧；若仍无响应，确认命令前缀为 `FP` 或 `ZW101`。

## 13. README 维护约定

1. 每次需求完成后（代码修改 + 编译验证），必须同步更新本 `README`。
2. 更新至少覆盖以下内容：
   - 功能变化（新增/删除/行为变更）。
   - 调试命令变化（命令字、参数、返回说明）。
   - 配置变化（宏开关、默认值、依赖关系）。
   - 验证结果（编译命令与是否通过）。
3. 变更记录使用时间倒序，便于追溯。

## 14. 变更记录

### 2026-04-13

1. 在 `application/mine/wk2114_uart2_ext` 新增 ZW101 独立测试子模块，固定走 WK2114 子串口1链路。
2. 新增 UART0 文本命令解析，仅保留 `ZW101 ENROLL/VERIFY/DEL` 三条核心测试命令。
3. 子串口1波特率改为 `57600`，并新增握手+传感器检测重探测流程，避免上电阶段偶发未就绪。
4. 保留现有 LD2402 子串口2路径，不改其协议流程，仅在主循环中并行调用 ZW101 测试处理。
5. 影响文件：
   - `src/application/mine/wk2114_uart2_ext/src/wk_zw101_test.c`
   - `src/application/mine/wk2114_uart2_ext/inc/wk_zw101_test.h`
   - `src/application/mine/wk2114_uart2_ext/src/mine_wk2114_uart2_ext.c`
   - `src/application/mine/wk2114_uart2_ext/CMakeLists.txt`
   - `src/application/mine/README.md`
6. 验证命令：`cd /home/xixi/code/fbb_ws63_20260114/src && python3 build.py ws63-liteos-app`。
7. 验证结果：构建通过，日志包含 `Build target:ws63_liteos_app success` 与 `packet success!`。

### 2026-03-31

1. 新增 `application/mine/ws63_final` 最终版分层框架（`Config/Common/BSP/Driver/Middleware/App`）。
2. 明确架构定位：`WS63` 为主控，`WK2114` 为外设扩展芯片。
3. 新增编译开关：`Support Mine WS63 final layered framework (WS63 master).`
4. 应用层新增统一对接接口：
   - `mine_ws63_final_task_register_rx_callback()`
   - `mine_ws63_final_task_send()`
5. 编译验证：在 `src` 目录执行 `python3 build.py ws63-liteos-app`，结果通过（见任务执行记录）。

### 2026-03-18

1. UART2 模式统一为三选一：`camera / zw101 / ld2402`。
2. 新增 Camera 调试命令：`CAM START`（输入后 UART2 输出 `start collect`）。
3. 从机上行标签新增 Camera 模式标识：`[CAMERA]`。
4. 编译验证：在 `src` 目录执行 `python3 build.py -c ws63-liteos-app`，结果通过。

### 2026-03-17

1. 按《指纹模组产品用户手册_V1.5.1》重写从机 `ZW101` 业务模块。
2. 主流程切换为自动指令：`PS_AutoEnroll`、`PS_AutoIdentify`、`PS_DeletChar`。
3. 调试命令调整为：`ENROLL <id> [times]`、`VERIFY [score] [id]`、`DEL <id> [count]`、`CANCEL`。
4. 增强现场可观测性：输出自动流程分阶段日志与确认码释义。
5. 编译验证：在 `src` 目录执行 `python3 build.py ws63-liteos-app -c`，结果通过。
6. 新增调试命令 `FP LIST`，通过 `PS_ValidTempleteNum (0x1D)` 查询模板库已录入数量。
7. OLED 布局调整：`STATE` 区域占两行，`DATA` 区域占两行（Host/Slave/WK2114 三处同步）。
