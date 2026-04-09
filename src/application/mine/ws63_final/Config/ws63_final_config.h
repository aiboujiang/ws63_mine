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

#define WS63_SUBPORT1_BAUD      115200U
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

/* 调试输出节流周期。 */
#define WS63_LOG_GAP_MS         1000U

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

/* RGB WS2812 演示开关：0=关闭，1=开启。 */
#define WS63_RGB_ENABLE                 0U

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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
