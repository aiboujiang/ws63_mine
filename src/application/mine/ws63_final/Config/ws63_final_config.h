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

/* 璁惧?囩??鍙ｆ槧灏? */
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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
