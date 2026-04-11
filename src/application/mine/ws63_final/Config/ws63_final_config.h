/**
 * @file ws63_final_config.h
 * @brief WK2114 最终版分层框架配置层。
 *
 * 说明：
 * 1) 仅放置常量配置，便于后续项目按板级差异快速调整；
 * 2) 所有层都可读取本文件，但不允许在这里放运行逻辑。
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

/* 自动运行开关：0=仅编译不自启动，1=系统启动后自动创建任务。 */
#define WS63_AUTO_RUN 1

/* WK2114 主口（WS63 的 UART2）配置。 */
#define WS63_HOST_UART_BUS      UART_BUS_2
#define WS63_HOST_UART_TX_PIN   8
#define WS63_HOST_UART_RX_PIN   7
#define WS63_HOST_UART_PIN_MODE 2

/* WK2114 复位/中断引脚配置。 */
#define WS63_RST_PIN            10
#define WS63_RST_PIN_MODE       0
#define WS63_IRQ_PIN            13
#define WS63_IRQ_PIN_MODE       0

/* WK2114 晶振配置（根据硬件实际焊接修改）。 */
#ifndef WS63_XTAL_FREQ_HZ
#define WS63_XTAL_FREQ_HZ       11059200U
#endif

/* 主口自动匹配时序参数。 */
#define WS63_MATCH_MAX_RETRY    3U
#define WS63_MATCH_SEND55_COUNT 5U
#define WS63_RESET_LOW_MS       10U
#define WS63_RESET_READY_MS     20U
#define WS63_MATCH_GAP_MS       5U
#define WS63_MATCH_LOCK_MS      20U

/* 子串口默认配置。 */
#define WS63_SUBPORT1_ENABLE    1U
#define WS63_SUBPORT2_ENABLE    1U
#define WS63_SUBPORT3_ENABLE    0U
#define WS63_SUBPORT4_ENABLE    0U

/* 子串口编号。 */
#define ZW101_SUBPORT  1U
#define LD2402_SUBPORT 2U

/* ZW101 模组默认波特率：与 sle_uart_slave 现网配置保持一致。 */
#define WS63_ZW101_BAUD         57600U

#define WS63_SUBPORT1_BAUD      WS63_ZW101_BAUD
#define WS63_SUBPORT2_BAUD      115200U
#define WS63_SUBPORT3_BAUD      115200U
#define WS63_SUBPORT4_BAUD      115200U

/* FIFO 与任务轮询节拍。 */
#define WS63_FIFO_CHUNK_MAX     16U
#define WS63_TASK_POLL_MS       5U
#define WS63_BOOT_DELAY_MS      1000U

/* 子串口触发阈值。 */
#define WS63_RX_TRIGGER_LEVEL   0x40U
#define WS63_TX_TRIGGER_LEVEL   0x10U

/* 任务参数。 */
#define WS63_TASK_STACK_SIZE    2048U
#define WS63_TASK_PRIORITY      26U

/* RTOS 多任务拆分参数：管理任务负责调试与系统保活。 */
#define WS63_MGR_TASK_STACK_SIZE        WS63_TASK_STACK_SIZE
#define WS63_MGR_TASK_PRIORITY          WS63_TASK_PRIORITY

/* WK2114 通信任务：负责子口轮询收发与驱动状态维护。 */
#define WS63_WK2114_TASK_STACK_SIZE     3072U
#define WS63_WK2114_TASK_PRIORITY       24U
#define WS63_WK2114_TASK_POLL_MS        5U
#define WS63_WK2114_RETRY_GAP_MS        1000U

/* SLE 协议任务：负责协议状态机与上下行桥接。 */
#define WS63_SLE_TASK_STACK_SIZE        4096U
#define WS63_SLE_TASK_PRIORITY          25U
#define WS63_SLE_TASK_POLL_MS           5U
#define WS63_SLE_RETRY_GAP_MS           1000U

/* RGB 渲染任务：负责演示模式与手动设色控制。 */
#define WS63_RGB_TASK_STACK_SIZE        2048U
#define WS63_RGB_TASK_PRIORITY          27U
#define WS63_RGB_TASK_POLL_MS           10U

/* 蜂鸣器任务：负责频率/音量/开关控制串行化。 */
#define WS63_BEEP_TASK_STACK_SIZE       2048U
#define WS63_BEEP_TASK_PRIORITY         27U

/* 任务间队列参数：固定上限保证内存可控。 */
#define WS63_TASK_QUEUE_PAYLOAD_MAX     WS63_SLE_SAFE_CHUNK_LEN
#define WS63_WK2114_TX_QUEUE_DEPTH      16U
#define WS63_SLE_UPLINK_QUEUE_DEPTH     16U
#define WS63_RGB_CTRL_QUEUE_DEPTH       8U
#define WS63_BEEP_CTRL_QUEUE_DEPTH      8U

/* 调试输出节流周期。 */
#define WS63_LOG_GAP_MS         1000U

/* LD2402 运行态日志默认策略：1=开启日志（按间隔输出），0=关闭运行态日志。 */
#define WS63_LD2402_DATA_LOG_ENABLE_DEFAULT         1U
/* LD2402 运行态日志默认间隔（ms），0 表示不做时间节流。 */
#define WS63_LD2402_DATA_LOG_GAP_MS_DEFAULT         1000U

/* SLE 上行 success 日志默认策略：1=开启，0=关闭（仅保留失败日志）。 */
#define WS63_SLE_UPLINK_SUCCESS_LOG_ENABLE_DEFAULT  0U
/* SLE 上行 success 日志默认间隔（ms），0 表示每次成功都打印。 */
#define WS63_SLE_UPLINK_SUCCESS_LOG_GAP_MS_DEFAULT  2000U

/* ----------------------------- 调试串口命令配置 ---------------------------- */
/* 在线控测串口开关：0=关闭，1=开启。 */
#define WS63_DEBUG_UART_ENABLE          1U

/* 默认使用 UART0(GPIO17/18, mode1) 作为在线调试命令口。 */
#define WS63_DEBUG_UART_BUS             UART_BUS_0
#define WS63_DEBUG_UART_TX_PIN          GPIO_17
#define WS63_DEBUG_UART_RX_PIN          GPIO_18
#define WS63_DEBUG_UART_PIN_MODE        1
#define WS63_DEBUG_UART_BAUD            115200U

/*
 * 调试日志镜像开关：
 * 0=仅输出到调试串口；1=同时输出到系统日志（osal_printk）。
 * 默认关闭可避免“同一物理串口双写”引发的重复日志。
 */
#define WS63_DEBUG_LOG_MIRROR_SYS       0U

/* 调试串口接收缓存与命令长度限制。 */
#define WS63_DEBUG_UART_RX_BUF_SIZE     256U
#define WS63_DEBUG_CMD_MAX_LEN          96U

/* MOTOR WATCH 周期日志间隔。 */
#define WS63_DEBUG_WATCH_PERIOD_MS      500U

/* ----------------------------- SLE 从机桥接配置 ----------------------------- */
/*
 * SLE 模块采用“双层开关”策略：
 * 1) Kconfig 控制是否编译/使能；
 * 2) 本配置头对外暴露统一宏，供 App/Middleware 直接判断。
 */
#ifdef CONFIG_MINE_WS63_FINAL_SLE_SLAVE_CORE
#define WS63_SLE_CORE_ENABLE            1U
#else
#define WS63_SLE_CORE_ENABLE            0U
#endif

#ifdef CONFIG_MINE_WS63_FINAL_SLE_LD2402
#define WS63_SLE_LD2402_ENABLE          1U
#else
#define WS63_SLE_LD2402_ENABLE          0U
#endif

#ifdef CONFIG_MINE_WS63_FINAL_SLE_ZW101
#define WS63_SLE_ZW101_ENABLE           1U
#else
#define WS63_SLE_ZW101_ENABLE           0U
#endif

#ifdef CONFIG_MINE_WS63_FINAL_SLE_CAMERA
#define WS63_SLE_CAMERA_ENABLE          1U
#else
#define WS63_SLE_CAMERA_ENABLE          0U
#endif

/* SLE 模块与 WK2114 子口映射。 */
#define WS63_SLE_ZW101_SUBPORT          ZW101_SUBPORT
#define WS63_SLE_LD2402_SUBPORT         LD2402_SUBPORT
#define WS63_SLE_CAMERA_SUBPORT         3U

/* SLE 服务/特征与广播名配置。 */
#define WS63_SLE_SERVICE_UUID           0xABCDU
#define WS63_SLE_PROPERTY_UUID          0xBCDEU
#define WS63_SLE_HOST_NAME              "mine_sle_host"
#define WS63_SLE_LOCAL_NAME             "mine_sle_slave_final"

/* SLE 传输参数。 */
#define WS63_SLE_DEFAULT_MTU_SIZE       512U
#define WS63_SLE_SAFE_CHUNK_LEN         200U
#define WS63_SLE_SEEK_INTERVAL          100U
#define WS63_SLE_SEEK_WINDOW            100U

/* SLE 日志开关：稳定后可关闭，减少串口输出。 */
#define WS63_SLE_LOG_ENABLE             1U

/* SLE 地址兜底值：系统地址不可用时使用。 */
#define WS63_SLE_FALLBACK_MAC           {0xE2, 0x00, 0x73, 0xC8, 0x11, 0x02}

/* RGB WS2812 演示开关：0=关闭，1=开启（默认开启，便于在线调试命令直接验证）。 */
#define WS63_RGB_ENABLE                 1U

/* WS2812 使用 SPI1 输出：GPIO1=SPI1_OUT(GPIO1 Mode3), GPIO6=SPI1_SCK(GPIO6 Mode3)。 */
#define WS63_RGB_SPI_BUS                SPI_BUS_1
#define WS63_RGB_DATA_PIN               GPIO_01
#define WS63_RGB_DATA_PIN_MODE          3
#define WS63_RGB_CLK_PIN                GPIO_06
#define WS63_RGB_CLK_PIN_MODE           3

/* WS2812 发送参数：4MHz + 5bit 编码，单 bit 约 1.25us。 */
#define WS63_RGB_SPI_FREQ_MHZ           4U
#define WS63_RGB_SPI_TIMEOUT_MS         0xFFFFFFFFU
#define WS63_RGB_DEMO_INTERVAL_MS       500U

/* 日志开关：调试阶段建议开启，稳定后可关闭降低串口开销。 */
#define WS63_RGB_LOG_ENABLE             1U

/* ----------------------------- 电机与编码器配置 ---------------------------- */
/* 电机控制引脚：IA=GPIO2，IB=GPIO3。 */
#define WS63_MOTOR_IA_PIN               GPIO_02
#define WS63_MOTOR_IB_PIN               GPIO_03

/* 引脚模式：0=GPIO，1=PWM。 */
#define WS63_MOTOR_IA_GPIO_MODE         0
#define WS63_MOTOR_IB_GPIO_MODE         0
#define WS63_MOTOR_IA_PWM_MODE          1
#define WS63_MOTOR_IB_PWM_MODE          1

/* PWM 资源映射：默认按 GPIO2->PWM2，GPIO3->PWM3。 */
#define WS63_MOTOR_IA_PWM_CHANNEL       2U
#define WS63_MOTOR_IB_PWM_CHANNEL       3U
#define WS63_MOTOR_IA_PWM_GROUP         2U
#define WS63_MOTOR_IB_PWM_GROUP         3U

/* PWM 周期与默认占空比（百分比）。 */
#define WS63_MOTOR_PWM_PERIOD_TICKS     40000U
#define WS63_MOTOR_DEFAULT_DUTY_PERCENT 40U

/* 编码器引脚：A 相=GPIO11，B 相=GPIO12。 */
#define WS63_ENCODER_A_PIN              GPIO_11
#define WS63_ENCODER_B_PIN              GPIO_12

/* 编码器输入模式与上下拉（3 对应 PIN_PULL_TYPE_UP）。 */
#define WS63_ENCODER_PIN_MODE           0
#define WS63_ENCODER_PULL_MODE          3

/* 测速采样窗口与编码器参数。 */
#define WS63_ENCODER_SAMPLE_MS          100U
#define WS63_ENCODER_PPR                7U

/* 外部减速齿轮减速比：输出轴转速 = 电机轴转速 / WS63_MOTOR_GEAR_RATIO。 */
#define WS63_MOTOR_GEAR_RATIO           150U

/* ----------------------------- 蜂鸣器配置 ----------------------------- */
/* 蜂鸣器开关：0=关闭，1=开启。 */
#define WS63_BEEP_ENABLE                1U

/* 默认采用无源蜂鸣器接 GPIO9（复用 mode1 对应 PWM1）。 */
#define WS63_BEEP_GPIO_PIN              GPIO_09
#define WS63_BEEP_GPIO_MODE             0
#define WS63_BEEP_PWM_PIN_MODE          1

/* PWM 通道与分组：与电机 PWM2/PWM3 资源隔离。 */
#define WS63_BEEP_PWM_CHANNEL           1U
#define WS63_BEEP_PWM_GROUP             1U

/* 蜂鸣器频率边界与默认频率（Hz）。 */
#define WS63_BEEP_MIN_FREQ_HZ           100U
#define WS63_BEEP_MAX_FREQ_HZ           5000U
#define WS63_BEEP_DEFAULT_FREQ_HZ       2000U

/* 蜂鸣器音量边界与默认值（本质为 PWM 占空比百分比）。 */
#define WS63_BEEP_MIN_VOLUME_PERCENT    0U
#define WS63_BEEP_MAX_VOLUME_PERCENT    95U
#define WS63_BEEP_DEFAULT_VOLUME_PERCENT 50U

/* PWM v151 计数位宽上限（16bit）。 */
#define WS63_BEEP_PWM_PERIOD_TICKS_MAX  0xFFFFU

/* ----------------------------- TTP229 触摸键盘配置 ----------------------------- */
/* TTP229 功能开关：0=关闭，1=开启。 */
#define WS63_TTP229_ENABLE                      1U

/* TTP229 接线：SCL=GPIO16，SDO(板上标注 SDA)=GPIO15。 */
#define WS63_TTP229_SCL_PIN                     GPIO_16
#define WS63_TTP229_SDO_PIN                     GPIO_15

/* TTP229 引脚复用模式：默认按 GPIO 输入输出模式。 */
#define WS63_TTP229_SCL_PIN_MODE                0
#define WS63_TTP229_SDO_PIN_MODE                0

/* TTP229 独立任务参数。 */
#define WS63_TTP229_TASK_STACK_SIZE             2048U
#define WS63_TTP229_TASK_PRIORITY               27U
#define WS63_TTP229_TASK_POLL_MS                10U
#define WS63_TTP229_INIT_RETRY_MS               500U

/* 状态机默认开关：支持运行时通过调试命令启停。 */
#define WS63_TTP229_ENABLE_DEFAULT              1U

/* 多键报警默认开关：1=开启，2键及以上同时按下时告警。 */
#define WS63_TTP229_MULTI_KEY_ALARM_DEFAULT     1U

/*
 * TTP229 扫描时序参数（参考现有移植代码）：
 * 1) 起始脉冲：高电平 93us，低电平 10us；
 * 2) 位时钟脉冲：高/低电平宽度；
 * 3) 一次读取结束后的间隔延时。
 */
#define WS63_TTP229_START_PULSE_HIGH_US         93U
#define WS63_TTP229_START_PULSE_LOW_US          10U
#define WS63_TTP229_SCL_PULSE_HIGH_US           2U
#define WS63_TTP229_SCL_PULSE_LOW_US            2U
#define WS63_TTP229_READ_GAP_MS                 4U

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
