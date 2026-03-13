# Mine: UART0 <-> SLE 双向透传 Demo

本目录提供两个可独立编译的示例：

- `sle_uart_host`：主机（SLE Server）
- `sle_uart_slave`：从机（SLE Client）

两端都使用 `UART0`，功能如下：

1. 主机串口收到数据 -> 通过 SLE 发送给从机 -> 从机从 UART0 打印输出。  
2. 从机串口收到数据 -> 通过 SLE 发送给主机 -> 主机从 UART0 打印输出。

---

## 1. 菜单配置

在 `src` 目录执行：

```bash
python build.py menuconfig
```

在菜单中启用：

- `Application -> Enable Mine Demos`
- `Application -> Mine SLE UART Demo Role`
  - 编译主机时选择 `Support Mine SLE UART Host Demo.`
  - 编译从机时选择 `Support Mine SLE UART Slave Demo.`

> 注意：主机和从机需要分别编译、分别烧录到两块芯片。

---

## 2. 代码位置

- 主机：`application/mine/sle_uart_host`
  - `src/mine_sle_uart_host.c`：SLE 服务端 + UART0 桥接主逻辑
  - `src/mine_sle_uart_host_adv.c`：广播参数与广播数据配置
- 从机：`application/mine/sle_uart_slave`
  - `src/mine_sle_uart_slave.c`：SLE 客户端 + UART0 桥接主逻辑
- OLED 驱动（已迁移到 mine，本目录独立维护）
  - `application/mine/common/ssd1306/hal_bsp_ssd1306.c`
  - `application/mine/common/ssd1306/hal_bsp_ssd1306.h`

---

## 3. 关键参数

- 广播名：`mine_sle_host`
- Service UUID：`0xABCD`
- Property UUID：`0xBCDE`
- UART：`UART0`
- 波特率：`115200`
- 数据格式：`8N1`

### 主机 OLED 调试显示（0.96 寸 SSD1306）

- 仅主机端 `sle_uart_host` 集成了 OLED 调试显示。
- SSD1306 驱动已复制到 `application/mine/common/ssd1306`，不再依赖 `samples` 路径。
- 默认使用 I2C1：
  - `SCL`：`GPIO16`
  - `SDA`：`GPIO15`
- OLED 会滚动显示最近 4 条关键事件，例如：
  - 主机启动、UART 初始化状态
  - SLE 初始化状态
  - 连接/断开状态
  - `UART -> SLE` 发送字节数、`SLE -> UART` 接收字节数

---

## 4. 联调建议

1. 先烧录并启动主机，确认串口日志出现 host 初始化成功。  
2. 再烧录并启动从机，确认从机能扫描并连接到 `mine_sle_host`。  
3. 打开两路串口工具：
   - 给主机串口发数据，观察从机串口输出。
   - 给从机串口发数据，观察主机串口输出。

---

## 5. 说明

- 代码中已对主要函数加中文注释，包括：
  - 函数作用
  - 参数解释
  - 关键流程说明
- 使用了消息队列把 UART 中断回调与 SLE 发送逻辑解耦，便于初学者理解“回调采集 + 任务发送”的常见设计模式。
- 主机与从机都补充了 `osal_printk` 调试输出，便于串口观察以下阶段：
  - 初始化（任务创建、UART/SLE 初始化）
  - 链路管理（扫描、连接、配对、发现）
  - 数据路径（UART 入队、UART->SLE 发送、SLE->UART 接收）

---

## 6. ZW101 指纹模块（从机）

- 从机侧已接入 `ZW101` 指纹协议模块（目录：`application/mine/common/ZW101`）。
- 默认开关与总线配置位于 `application/mine/sle_uart_slave/inc/sle_uart_slave.h`：
  - `MINE_ZW101_ENABLE`
  - `MINE_ZW101_UART_BUS`
- 协议解析与握手初始化在从机主逻辑中完成，按总线接收字节流自动喂入解析器。
