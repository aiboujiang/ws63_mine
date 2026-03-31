/**
 * @file ws63_final_config.h
 * @brief WK2114 最终版分层框架配置层。
 *
 * 说明：
 * 1) 仅放置常量配置，便于后续项目按板级差异快速调整；
 * 2) 所有层都可读取本文件，但不允许在这里放运行逻辑。
 */

#ifndef MINE_WS63_FINAL_CONFIG_H
#define MINE_WS63_FINAL_CONFIG_H

#include "uart.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* 自动运行开关：0=仅编译不自启动，1=系统启动后自动创建任务。 */
#define MINE_WS63_FINAL_AUTO_RUN 0

/* WK2114 主口（WS63 的 UART2）配置。 */
#define MINE_WS63_FINAL_HOST_UART_BUS      UART_BUS_2
#define MINE_WS63_FINAL_HOST_UART_TX_PIN   8
#define MINE_WS63_FINAL_HOST_UART_RX_PIN   7
#define MINE_WS63_FINAL_HOST_UART_PIN_MODE 2

/* WK2114 复位/中断引脚配置。 */
#define MINE_WS63_FINAL_RST_PIN            10
#define MINE_WS63_FINAL_RST_PIN_MODE       0
#define MINE_WS63_FINAL_IRQ_PIN            13
#define MINE_WS63_FINAL_IRQ_PIN_MODE       0

/* WK2114 晶振配置（根据硬件实际焊接修改）。 */
#ifndef MINE_WS63_FINAL_XTAL_FREQ_HZ
#define MINE_WS63_FINAL_XTAL_FREQ_HZ       11059200U
#endif

/* 主口自动匹配时序参数。 */
#define MINE_WS63_FINAL_MATCH_MAX_RETRY    3U
#define MINE_WS63_FINAL_MATCH_SEND55_COUNT 5U
#define MINE_WS63_FINAL_RESET_LOW_MS       10U
#define MINE_WS63_FINAL_RESET_READY_MS     20U
#define MINE_WS63_FINAL_MATCH_GAP_MS       5U
#define MINE_WS63_FINAL_MATCH_LOCK_MS      20U

/* 子串口默认配置。 */
#define MINE_WS63_FINAL_SUBPORT1_ENABLE    1U
#define MINE_WS63_FINAL_SUBPORT2_ENABLE    1U
#define MINE_WS63_FINAL_SUBPORT3_ENABLE    0U
#define MINE_WS63_FINAL_SUBPORT4_ENABLE    0U

#define MINE_WS63_FINAL_SUBPORT1_BAUD      115200U
#define MINE_WS63_FINAL_SUBPORT2_BAUD      115200U
#define MINE_WS63_FINAL_SUBPORT3_BAUD      115200U
#define MINE_WS63_FINAL_SUBPORT4_BAUD      115200U

/* FIFO 与任务轮询节拍。 */
#define MINE_WS63_FINAL_FIFO_CHUNK_MAX     16U
#define MINE_WS63_FINAL_TASK_POLL_MS       5U
#define MINE_WS63_FINAL_BOOT_DELAY_MS      1000U

/* 子串口触发阈值。 */
#define MINE_WS63_FINAL_RX_TRIGGER_LEVEL   0x40U
#define MINE_WS63_FINAL_TX_TRIGGER_LEVEL   0x10U

/* 任务参数。 */
#define MINE_WS63_FINAL_TASK_STACK_SIZE    2048U
#define MINE_WS63_FINAL_TASK_PRIORITY      26U

/* 调试输出节流周期。 */
#define MINE_WS63_FINAL_LOG_GAP_MS         1000U

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
