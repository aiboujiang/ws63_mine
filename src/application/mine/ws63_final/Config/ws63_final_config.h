/**
 * @file ws63_final_config.h
 * @brief WK2114 最终版分层框架配置层（按模块分组重构）。
 *
 * 说明：
 * 1) 仅放置常量配置，便于后续项目按板级差异快速调整；
 * 2) 所有层都可读取本文件，但不允许在这里放运行逻辑；
 * 3) 此文件已根据功能模块严格分块，并逐行注释。
 */

#ifndef WS63_CONFIG_H
#define WS63_CONFIG_H

#include "platform_core.h"
#include "uart.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* ============================================================================ */
/*                             【1. 全局系统与任务配置】                        */
/* ============================================================================ */

/* 自动运行开关：0=仅编译不自启动，1=系统启动后自动创建任务 */
#define WS63_AUTO_RUN 1

/* 系统启动前的延时等待时间（毫秒） */
#define WS63_BOOT_DELAY_MS 1000U

/* 默认任务栈大小，仅保留给非管理场景，避免误用大栈 */
#define WS63_TASK_STACK_SIZE 2048U

/* 默认任务优先级 */
#define WS63_TASK_PRIORITY 26U

/* 管理任务栈大小：负责调试与系统保活，需为命令解析和日志预留空间 */
#define WS63_MGR_TASK_STACK_SIZE 8192U

/* 管理任务优先级：与默认级别保持一致 */
#define WS63_MGR_TASK_PRIORITY WS63_TASK_PRIORITY

/* 默认任务轮询间隔（毫秒） */
#define WS63_TASK_POLL_MS 5U

/* 全局日志输出的节流保护周期（毫秒） */
#define WS63_LOG_GAP_MS 1000U


/* ============================================================================ */
/*                             【2. WK2114 主串口与通用配置】                   */
/* ============================================================================ */

/* WK2114 主口对应 WS63 的物理串口号 */
#define WS63_HOST_UART_BUS UART_BUS_2

/* WK2114 主口 TX 引脚号 */
#define WS63_HOST_UART_TX_PIN 8

/* WK2114 主口 RX 引脚号 */
#define WS63_HOST_UART_RX_PIN 7

/* WK2114 主口引脚复用模式 */
#define WS63_HOST_UART_PIN_MODE 2

/* WK2114 硬件复位引脚号 */
#define WS63_RST_PIN 10

/* WK2114 硬件复位引脚复用模式 */
#define WS63_RST_PIN_MODE 0

/* WK2114 硬件中断请求(IRQ)引脚号 */
#define WS63_IRQ_PIN 13

/* WK2114 硬件中断引脚复用模式 */
#define WS63_IRQ_PIN_MODE 0

/* WK2114 外部晶振频率（默认为 11.0592MHz） */
#ifndef WS63_XTAL_FREQ_HZ
#define WS63_XTAL_FREQ_HZ 11059200U
#endif

/* 主口波特率自动匹配最大重试次数 */
#define WS63_MATCH_MAX_RETRY 3U

/* 主口匹配波特率时发送 0x55 的次数 */
#define WS63_MATCH_SEND55_COUNT 5U

/* 硬件复位引脚拉长低电平的时间（毫秒） */
#define WS63_RESET_LOW_MS 10U

/* 复位后等待芯片准备就绪的时间（毫秒） */
#define WS63_RESET_READY_MS 20U

/* 波特率匹配重发间隔（毫秒） */
#define WS63_MATCH_GAP_MS 5U

/* 波特率匹配成功后的锁定防抖时间（毫秒） */
#define WS63_MATCH_LOCK_MS 20U

/* 串口数据处理最大分块尺寸 */
#define WS63_FIFO_CHUNK_MAX 16U

/* 子串口接收触发阈值（达到此值触发读包） */
#define WS63_RX_TRIGGER_LEVEL 0x40U

/* 子串口发送触发阈值 */
#define WS63_TX_TRIGGER_LEVEL 0x10U

/* WK2114 通信任务的栈大小 */
#define WS63_WK2114_TASK_STACK_SIZE 3072U

/* WK2114 通信任务的优先级（较高） */
#define WS63_WK2114_TASK_PRIORITY 24U

/* WK2114 通信任务的基础轮询周期（毫秒） */
#define WS63_WK2114_TASK_POLL_MS 5U

/* WK2114 状态异常时的重试间隔（毫秒） */
#define WS63_WK2114_RETRY_GAP_MS 1000U

/* WK2114 发送指令缓存队列的深度 */
#define WS63_WK2114_TX_QUEUE_DEPTH 16U


/* ============================================================================ */
/*                             【3. 子串口业务分配】                            */
/* ============================================================================ */

/* ZW101指纹模块绑定的物理子串口号 */
#define ZW101_SUBPORT 1U

/* LD2402雷达模块绑定的物理子串口号 */
#define LD2402_SUBPORT 2U

/* 子串口 1 使能开关（控制 ZW101） */
#define WS63_SUBPORT1_ENABLE 1U

/* 子串口 2 使能开关（控制 LD2402） */
#define WS63_SUBPORT2_ENABLE 1U

/* 子串口 3 使能开关（控制 Camera） */
#define WS63_SUBPORT3_ENABLE 1U

/* 子串口 4 使能开关（预留） */
#define WS63_SUBPORT4_ENABLE 0U

/* ZW101（指纹）子串口默认波特率 */
#define WS63_ZW101_BAUD 57600U

/* 子串口 1 波特率（同步 ZW101） */
#define WS63_SUBPORT1_BAUD WS63_ZW101_BAUD

/* 子串口 2 波特率（雷达默认） */
#define WS63_SUBPORT2_BAUD 115200U

/* 子串口 3 波特率（摄像头默认） */
#define WS63_SUBPORT3_BAUD 115200U

/* 子串口 4 波特率 */
#define WS63_SUBPORT4_BAUD 115200U


/* ============================================================================ */
/*                             【4. 门锁编排任务(Lock_mgr)配置】                */
/* ============================================================================ */

/* 门锁状态机任务栈大小 */
#define WS63_LOCK_MGR_TASK_STACK_SIZE 3072U

/* 门锁状态机任务优先级 */
#define WS63_LOCK_MGR_TASK_PRIORITY 25U

/* 门锁状态机任务轮询周期（毫秒） */
#define WS63_LOCK_MGR_TASK_POLL_MS 20U

/* LD2402 雷达触发接近唤醒（ARMED）的距离阈值 */
#define WS63_LOCK_LD2402_ARM_DISTANCE_MM_DEFAULT 80U

/* 唤醒后的等待认证窗口持续时间（毫秒），窗口内有输入则刷新 */
#define WS63_LOCK_AUTH_WINDOW_MS_DEFAULT 15000U

/* 开锁动作维持电机正转的时长（毫秒） */
#define WS63_LOCK_UNLOCK_DURATION_MS_DEFAULT 1500U

/* 认证成功且处于解锁状态时的提示反馈时长（毫秒：绿灯及蜂鸣） */
#define WS63_LOCK_AUTH_SUCCESS_FEEDBACK_MS_DEFAULT 800U

/* 认证失败且门锁保持状态时的警示反馈时长（毫秒：红灯及蜂鸣） */
#define WS63_LOCK_AUTH_FAIL_FEEDBACK_MS_DEFAULT 800U

/* 开锁完成后保持开启状态（不自动回锁）的时长（毫秒） */
#define WS63_LOCK_HOLD_OPEN_MS_DEFAULT 10000U

/* 连续失败达标后的惩罚封印避让期时长（毫秒） */
#define WS63_LOCK_FAIL_LOCKOUT_MS_DEFAULT 10000U

/* 开锁时电机的正转占空比负载百分比 (0-100) */
#define WS63_LOCK_MOTOR_OPEN_DUTY_DEFAULT 40U

/* 关锁时电机的反转占空比负载百分比 (0-100) */
#define WS63_LOCK_MOTOR_CLOSE_DUTY_DEFAULT 40U

/* Camera 唤醒事件和上报之间的防抖间隔保护（毫秒） */
#define WS63_LOCK_CAMERA_WAKE_GAP_MS_DEFAULT 500U


/* ============================================================================ */
/*                             【5. 指纹 ZW101 配置】                           */

/* 是否开启 ZW101 详细流程追踪日志（1：开启，0：关闭）以减少调试过程中的日志刷屏 */
#define WS63_ZW101_TRACE_ENABLE 0U

#if (WS63_ZW101_TRACE_ENABLE == 1U)
#define ZW101_TRACE(...) osal_printk(__VA_ARGS__)
#else
#define ZW101_TRACE(...)
#endif
/* ============================================================================ */

/* ZW101 处理任务栈大小（因为包含同步认证自愈链路，故单独放大） */
#define WS63_ZW101_TASK_STACK_SIZE 3072U


/* ============================================================================ */
/*                             【6. 摄像头 Camera 配置】                        */
/* ============================================================================ */

/* Camera 总功能使能开关 */
#define WS63_CAMERA_ENABLE 1U

/* Camera 控制任务的栈大小 */
#define WS63_CAMERA_TASK_STACK_SIZE 4096U

/* Camera 控制任务优先级 */
#define WS63_CAMERA_TASK_PRIORITY 26U

/* Camera 交互指令消息队列深度 */
#define WS63_CAMERA_CMD_QUEUE_DEPTH 8U


/* ============================================================================ */
/*                             【7. 触摸键盘 VK36N16I 配置】                      */
/* ============================================================================ */

/* VK36N16I 触摸键盘功能使能总开关 */
#define WS63_VK36N16I_ENABLE 1U

/* VK36N16I 模拟 I2C SCL引脚 */
#define WS63_VK36N16I_SCL_PIN GPIO_16

/* VK36N16I 模拟 I2C SDA引脚 */
#define WS63_VK36N16I_SDA_PIN GPIO_15

/* VK36N16I I2C接口复用模式 */
#define WS63_VK36N16I_I2C_PIN_MODE 2U

/* VK36N16I 挂载的物理 I2C 总线编号 */
#define WS63_VK36N16I_I2C_BUS 1U

/* VK36N16I I2C 总线速率（赫兹） */
#define WS63_VK36N16I_I2C_SPEED 100000U

/* VK36N16I 硬件地址（7bit = 0x65） */
#define WS63_VK36N16I_I2C_ADDR 0x65U

/* 每次读取 I2C 地址的字节长度 */
#define WS63_VK36N16I_I2C_READ_LEN 2U

/* VK36N16I 工作任务栈大小 */
#define WS63_VK36N16I_TASK_STACK_SIZE 2048U

/* VK36N16I 工作任务优先级 */
#define WS63_VK36N16I_TASK_PRIORITY 27U

/* VK36N16I 主事件轮询周期（毫秒） */
#define WS63_VK36N16I_TASK_POLL_MS 10U

/* 初始化失败时的退避重试时间（毫秒） */
#define WS63_VK36N16I_INIT_RETRY_MS 500U

/* VK36N16I 状态机初始化默认使能 */
#define WS63_VK36N16I_ENABLE_DEFAULT 1U

/* 开启多键防误触报警功能：2键或以上同时按下时报警 */
#define WS63_VK36N16I_MULTI_KEY_ALARM_DEFAULT 1U

/* I2C 读数发生偶发错误时的尝试重复次数 */
#define WS63_VK36N16I_READ_RETRY_MAX 3U

/* I2C 读取重试间隔（不能太大以免影响 10ms 轮询节奏） */
#define WS63_VK36N16I_READ_RETRY_GAP_MS 1U

/* ---------------- 键盘密码与防爆破 ---------------- */
/* VK36N16I 内置比对的静态密码文本 */
#define WS63_VK36N16I_PASSWORD_TEXT "123456"

/* VK36N16I 内置静态密码文本对应的长度 */
#define WS63_VK36N16I_PASSWORD_LEN 6U

/* 连续密码输入错误的封禁惩罚阈值（当前唤醒周期内） */
#define WS63_VK36N16I_PASSWORD_FAIL_DISABLE_THRESHOLD 5U

/* 按键被有效识别后的反馈蜂鸣器频率（赫兹） */
#define WS63_VK36N16I_KEY_PROMPT_BEEP_FREQ_HZ 1200U

/* 按键反馈蜂鸣器的相对响度百分比 (0-100) */
#define WS63_VK36N16I_KEY_PROMPT_BEEP_VOLUME_PERCENT 12U

/* 按键反馈蜂鸣器的发声持续时间（毫秒） */
#define WS63_VK36N16I_KEY_PROMPT_BEEP_MS 20U


/* ============================================================================ */
/*                             【8. SLE 星闪与网络配置】                        */
/* ============================================================================ */

/* --- 星闪宏与组件关联开关 --- */
/* 根据 Kconfig 使能 SLE 主开关。供业务逻辑判断 */
#ifdef CONFIG_MINE_WS63_FINAL_SLE_SLAVE_CORE
#define WS63_SLE_CORE_ENABLE 1U
#else
#define WS63_SLE_CORE_ENABLE 0U
#endif

/* 星闪是否拉起 LD2402 映射节点 */
#ifdef CONFIG_MINE_WS63_FINAL_SLE_LD2402
#define WS63_SLE_LD2402_ENABLE 1U
#else
#define WS63_SLE_LD2402_ENABLE 0U
#endif

/* 星闪是否拉起 ZW101 映射节点 */
#ifdef CONFIG_MINE_WS63_FINAL_SLE_ZW101
#define WS63_SLE_ZW101_ENABLE 1U
#else
#define WS63_SLE_ZW101_ENABLE 0U
#endif

/* 星闪是否拉起 Camera 映射节点 */
#ifdef CONFIG_MINE_WS63_FINAL_SLE_CAMERA
#define WS63_SLE_CAMERA_ENABLE 1U
#else
#define WS63_SLE_CAMERA_ENABLE 0U
#endif

/* SLE 模块侧的指纹映射逻辑端口号 */
#define WS63_SLE_ZW101_SUBPORT ZW101_SUBPORT

/* SLE 模块侧的雷达映射逻辑端口号 */
#define WS63_SLE_LD2402_SUBPORT LD2402_SUBPORT

/* SLE 模块侧的摄象头映射逻辑端口号 */
#define WS63_SLE_CAMERA_SUBPORT 3U

/* SLE 模块侧的纯逻辑服务端口（路由 Lock 事件，无对应实体 UART） */
#define WS63_SLE_LOCK_SUBPORT 4U

/* 星闪服务的 UUID 标识 */
#define WS63_SLE_SERVICE_UUID 0xABCDU

/* 星闪特征值的 UUID 标识 */
#define WS63_SLE_PROPERTY_UUID 0xBCDEU

/* 要连接的星闪 Host 端名称 */
#define WS63_SLE_HOST_NAME "mine_sle_host"

/* 本机的星闪 Slave 广播名称 */
#define WS63_SLE_LOCAL_NAME "mine_sle_slave_final"

/* SLE 单次封包 MTU 大小 */
#define WS63_SLE_DEFAULT_MTU_SIZE 512U

/* SLE 保证不分包丢帧的安全片段上限 */
#define WS63_SLE_SAFE_CHUNK_LEN 200U

/* SLE 寻找主机（Seek）的重试频率参数 */
#define WS63_SLE_SEEK_INTERVAL 100U

/* SLE 寻找主机（Seek）的响应探测窗口 */
#define WS63_SLE_SEEK_WINDOW 100U

/* 系统 MAC 取不到时的备用本地 MAC 地址 */
#define WS63_SLE_FALLBACK_MAC {0xE2, 0x00, 0x73, 0xC8, 0x11, 0x02}

/* SLE 通信任务栈大小 */
#define WS63_SLE_TASK_STACK_SIZE 4096U

/* SLE 通信任务优先级 */
#define WS63_SLE_TASK_PRIORITY 25U

/* SLE 通信底层轮询周期 */
#define WS63_SLE_TASK_POLL_MS 5U

/* SLE 断连重连的基础间隙（毫秒） */
#define WS63_SLE_RETRY_GAP_MS 1000U

/* 任务间队列通用载荷上限尺寸（对齐 SLE 安全分发长） */
#define WS63_TASK_QUEUE_PAYLOAD_MAX WS63_SLE_SAFE_CHUNK_LEN

/* 缓存应用层向星闪发送数据的消息队列深度 */
#define WS63_SLE_UPLINK_QUEUE_DEPTH 16U

/* SLE 报文追踪日志使能 */
#define WS63_SLE_LOG_ENABLE 1U

/* SLE 上行 Success 日志是否默认开启：0=关闭（减少无效刷屏），1=开启 */
#define WS63_SLE_UPLINK_SUCCESS_LOG_ENABLE_DEFAULT 0U

/* SLE 上行 Success 日志节流窗口：0=全数打印，其他=指定毫秒内最多一跳 */
#define WS63_SLE_UPLINK_SUCCESS_LOG_GAP_MS_DEFAULT 2000U


/* ============================================================================ */
/*                             【9. 雷达 LD2402 配置】                          */
/* ============================================================================ */

/* 默认是否开启雷达上报的数据日志追踪（1=开，0=关） */
#define WS63_LD2402_DATA_LOG_ENABLE_DEFAULT 1U

/* 默认雷达日志连续输出的最小限流间隔（以免堵塞串口） */
#define WS63_LD2402_DATA_LOG_GAP_MS_DEFAULT 1000U


/* ============================================================================ */
/*                             【10. RGB 状态指示灯配置】                       */
/* ============================================================================ */

/* RGB WS2812 总功能使能开关 */
#define WS63_RGB_ENABLE 1U

/* RGB 引脚挂载的物理 SPI 线序列号 */
#define WS63_RGB_SPI_BUS SPI_BUS_1

/* SPI MOSI (数据线) 引脚号 */
#define WS63_RGB_DATA_PIN GPIO_01

/* SPI MOSI (数据线) 引脚复用模式 */
#define WS63_RGB_DATA_PIN_MODE 3

/* SPI SCK (时钟线) 引脚号 */
#define WS63_RGB_CLK_PIN GPIO_06

/* SPI SCK (时钟线) 引脚复用模式 */
#define WS63_RGB_CLK_PIN_MODE 3

/* SPI 总线模拟波特率，用于严格对齐 WS2812 协议 (单元1.25us，4MHz+5bit) */
#define WS63_RGB_SPI_FREQ_MHZ 4U

/* SPI 传输防死锁的默认超时限定 */
#define WS63_RGB_SPI_TIMEOUT_MS 0xFFFFFFFFU

/* 系统开机后演示灯效默认开关（0=不上演，1=自动上电流水灯） */
#define WS63_RGB_DEMO_ENABLE_DEFAULT 0U

/* 彩带灯效步进间隔（毫秒） */
#define WS63_RGB_DEMO_INTERVAL_MS 500U

/* RGB 任务栈大小 */
#define WS63_RGB_TASK_STACK_SIZE 2048U

/* RGB 任务优先级 */
#define WS63_RGB_TASK_PRIORITY 27U

/* RGB 模块接收指令的轮询感知周期（毫秒） */
#define WS63_RGB_TASK_POLL_MS 10U

/* RGB 命令队列的承载量上限 */
#define WS63_RGB_CTRL_QUEUE_DEPTH 8U

/* RGB 调试追踪日志使能 */
#define WS63_RGB_LOG_ENABLE 1U


/* ============================================================================ */
/*                             【11. 蜂鸣器 (Beep) 配置】                       */
/* ============================================================================ */

/* 蜂鸣器总功能使能开关 */
#define WS63_BEEP_ENABLE 1U

/* 无源蜂鸣器的 PWM 控制引脚号 */
#define WS63_BEEP_GPIO_PIN GPIO_09

/* 无源蜂鸣器 GPIO 原始模式（初始化安全过渡使用） */
#define WS63_BEEP_GPIO_MODE 0

/* 引脚复用至 PWM 波形生成的模式位 */
#define WS63_BEEP_PWM_PIN_MODE 1

/* 分配给蜂鸣器的系统硬件 PWM 逻辑通道 */
#define WS63_BEEP_PWM_CHANNEL 1U

/* 分配给蜂鸣器的系统硬件 PWM 控制组 */
#define WS63_BEEP_PWM_GROUP 1U

/* 蜂鸣器允许响应的最小振荡频率（赫兹） */
#define WS63_BEEP_MIN_FREQ_HZ 100U

/* 蜂鸣器允许响应的最大振荡频率（赫兹） */
#define WS63_BEEP_MAX_FREQ_HZ 5000U

/* 蜂鸣器的默认响应提示音频频率（赫兹） */
#define WS63_BEEP_DEFAULT_FREQ_HZ 2000U

/* 蜂鸣器音量计算下限阈值百分比 */
#define WS63_BEEP_MIN_VOLUME_PERCENT 0U

/* 蜂鸣器最高响度控制百分比 */
#define WS63_BEEP_MAX_VOLUME_PERCENT 95U

/* 提醒警报和常规操作时的默认发声音量负荷百分比 */
#define WS63_BEEP_DEFAULT_VOLUME_PERCENT 50U

/* PWM 底层分频位宽上限值（16 bit 系统） */
#define WS63_BEEP_PWM_PERIOD_TICKS_MAX 0xFFFFU

/* Beep 处理任务栈大小 */
#define WS63_BEEP_TASK_STACK_SIZE 2048U

/* Beep 任务优先级 */
#define WS63_BEEP_TASK_PRIORITY 27U

/* Beep 控制消息接收缓冲深度 */
#define WS63_BEEP_CTRL_QUEUE_DEPTH 8U


/* ============================================================================ */
/*                             【12. 电机与编码器配置】                         */
/* ============================================================================ */

/* 电机控制器 A 相 (IA) 引脚 */
#define WS63_MOTOR_IA_PIN GPIO_02

/* 电机控制器 B 相 (IB) 引脚 */
#define WS63_MOTOR_IB_PIN GPIO_03

/* IA 引脚普通逻辑控制模式 */
#define WS63_MOTOR_IA_GPIO_MODE 0

/* IB 引脚普通逻辑控制模式 */
#define WS63_MOTOR_IB_GPIO_MODE 0

/* IA 引脚向电机输出 PWM 控制的模式位 */
#define WS63_MOTOR_IA_PWM_MODE 1

/* IB 引脚向电机输出 PWM 控制的模式位 */
#define WS63_MOTOR_IB_PWM_MODE 1

/* 电机 A 相分配的 PWM 通道 (区分 Beep) */
#define WS63_MOTOR_IA_PWM_CHANNEL 2U

/* 电机 B 相分配的 PWM 通道 */
#define WS63_MOTOR_IB_PWM_CHANNEL 3U

/* 电机 A 相控制组别 */
#define WS63_MOTOR_IA_PWM_GROUP 2U

/* 电机 B 相控制组别 */
#define WS63_MOTOR_IB_PWM_GROUP 3U

/* 电机载波脉冲生成周期 */
#define WS63_MOTOR_PWM_PERIOD_TICKS 40000U

/* 门锁电机的默认预备占空比上限 */
#define WS63_MOTOR_DEFAULT_DUTY_PERCENT 40U

/* 编码器反馈接收 A 相引脚 */
#define WS63_ENCODER_A_PIN GPIO_11

/* 编码器反馈接收 B 相引脚 */
#define WS63_ENCODER_B_PIN GPIO_12

/* 编码器引脚的基础读取模式 */
#define WS63_ENCODER_PIN_MODE 0

/* 编码器引脚的电平上拉使能配置 */
#define WS63_ENCODER_PULL_MODE 3

/* 编码器转动测速样本的时间窗口（毫秒） */
#define WS63_ENCODER_SAMPLE_MS 100U

/* 编码盘对应的物理分度基数 */
#define WS63_ENCODER_PPR 7U

/* 电机对外侧执行器主输出的机械减速比 */
#define WS63_MOTOR_GEAR_RATIO 150U


/* ============================================================================ */
/*                             【13. 调试终端与内部命令配置】                   */
/* ============================================================================ */

/* UART 本地调试控制外壳程序总开关：0=关闭，1=开启命令解析引擎及路由 */
#define WS63_DEBUG_UART_ENABLE 1U

/* 系统开机时评估“是否只启动 Debug 模式”的等待缓冲时间（毫秒） */
#define WS63_DEBUG_BOOT_DECISION_MS 2000U

/* 当使用严格星闪传输模式时，只信任下行命令且限制上行终端通道 */
#define WS63_DEBUG_STRICT_SLE_ONLY 1U

/* 是否打开 WS63 自己的本地物理调试串口通道：0=禁用物理接收，仅依靠星闪虚拟通道 */
#define WS63_DEBUG_LOCAL_UART_IO_ENABLE 0U

/* 是否允许星闪传递的控制命令进入解析引擎 */
#define WS63_DEBUG_SLE_CMD_ENABLE 1U

/* 内部生成的系统调试日志，是否同意打包上传到星闪网络 */
#define WS63_DEBUG_SLE_LOG_ENABLE 1U

/* 本机调试串口总线编号 (当 LOCAL_UART_IO 开启时生效) */
#define WS63_DEBUG_UART_BUS UART_BUS_0

/* 本机调试串口 TX 硬件引脚 */
#define WS63_DEBUG_UART_TX_PIN GPIO_17

/* 本机调试串口 RX 硬件引脚 */
#define WS63_DEBUG_UART_RX_PIN GPIO_18

/* 本机调试串口引脚复用选择 */
#define WS63_DEBUG_UART_PIN_MODE 1

/* 本机调试串口物理传输波特率 */
#define WS63_DEBUG_UART_BAUD 115200U

/* 是否把所有本地应用层输出再强制投递回系统内核打印层（osal_printk）：0=不双重打印 */
#define WS63_DEBUG_LOG_MIRROR_SYS 0U

/* 系统终端交互命令读缓冲区大小 */
#define WS63_DEBUG_UART_RX_BUF_SIZE 256U

/* 单条调试命令文本的输入字长安全限制 */
#define WS63_DEBUG_CMD_MAX_LEN 96U

/* 电机位置及系统看门狗巡检探针的观测频率（毫秒） */
#define WS63_DEBUG_WATCH_PERIOD_MS 500U

/* --- 非法态守卫约束判断 --- */
#if (WS63_DEBUG_STRICT_SLE_ONLY == 1U)
#if (WS63_DEBUG_LOCAL_UART_IO_ENABLE != 0U)
#error "Strict SLE mode requires WS63_DEBUG_LOCAL_UART_IO_ENABLE=0"
#endif
#if (WS63_DEBUG_SLE_CMD_ENABLE != 1U)
#error "Strict SLE mode requires WS63_DEBUG_SLE_CMD_ENABLE=1"
#endif
#if (WS63_DEBUG_SLE_LOG_ENABLE != 1U)
#error "Strict SLE mode requires WS63_DEBUG_SLE_LOG_ENABLE=1"
#endif
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* WS63_CONFIG_H */
