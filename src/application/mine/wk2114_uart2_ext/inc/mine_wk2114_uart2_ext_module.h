/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块内部共享声明。
 */

#ifndef MINE_WK2114_UART2_EXT_MODULE_H
#define MINE_WK2114_UART2_EXT_MODULE_H

#include <stdint.h>

#include "mine_wk2114_uart2_ext.h"
#include "uart.h"

/* 任务参数。 */
#define MINE_WK2114_TASK_PRIO 24
#define MINE_WK2114_TASK_STACK_SIZE 0x1800
#define MINE_WK2114_INIT_DELAY_MS 200
#define MINE_WK2114_TASK_LOOP_WAIT_MS 80

/* 启动失败重试参数。 */
#define MINE_WK2114_INIT_RETRY_WAIT_MS 1200

/* UART2 作为 WK2114 主接口。 */
#define MINE_WK2114_HOST_UART_BUS UART_BUS_2
#define MINE_WK2114_HOST_UART_TX_PIN 8
#define MINE_WK2114_HOST_UART_RX_PIN 7
#define MINE_WK2114_HOST_UART_PIN_MODE 2
#define MINE_WK2114_HOST_UART_BAUD 115200
#define MINE_WK2114_UART_RX_BUFFER_SIZE 512

/* WK2114 设备参数。 */
#define MINE_WK2114_XTAL_HZ 11059200U
#define MINE_WK2114_SUBUART_COUNT 4
#define MINE_WK2114_SUBUART_MIN 1
#define MINE_WK2114_SUBUART_MAX 4
#define MINE_WK2114_FIFO_CHUNK_MAX 16
#define MINE_WK2114_UART_FRAME_MAX (MINE_WK2114_FIFO_CHUNK_MAX + 1)

/* WK2114 主口 UART 协议命令字前缀（手册 9.3）。 */
#define MINE_WK2114_HOST_CMD_WRITE_REG 0x00U
#define MINE_WK2114_HOST_CMD_READ_REG 0x40U
#define MINE_WK2114_HOST_CMD_WRITE_FIFO 0x80U

/* 手册 9.1：上电后需先发 0x55 完成主口波特率自适应锁定。 */
#define MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE 0x55U

/* 主口读寄存器超时与自适应锁定等待。 */
#define MINE_WK2114_HOST_READ_TIMEOUT_MS 80U
#define MINE_WK2114_HOST_AUTOBAUD_LOCK_WAIT_MS 3U

/* 回读稳定等待与链路检查重读次数。 */
#define MINE_WK2114_HOST_RESP_STABLE_WAIT_MS 2U
#define MINE_WK2114_LINK_CHECK_READ_RETRY 3U

/* 主口回读缓存深度（用于读寄存器等待）。 */
#define MINE_WK2114_HOST_RESP_FIFO_SIZE 64U

/* WK2114 地址定义（6bit 地址）。 */
#define MINE_WK2114_ADDR_GENA 0x00
#define MINE_WK2114_ADDR_GRST 0x01
#define MINE_WK2114_SUBREG_SPAGE 0x03
#define MINE_WK2114_SUBREG_SCR 0x04
#define MINE_WK2114_SUBREG_FCR 0x06
#define MINE_WK2114_SUBREG_BAUD1 0x04
#define MINE_WK2114_SUBREG_BAUD0 0x05
#define MINE_WK2114_SUBREG_PRES 0x06

/* GENA 位定义：高 4 位为保留位（推荐保持为 1），低 4 位为 UT1~UT4 使能位。 */
#define MINE_WK2114_GENA_RESERVED_MASK 0xF0U
#define MINE_WK2114_GENA_CHANNEL_MASK 0x0FU

/* OLED 面板布局。 */
#define MINE_WK2114_OLED_LINE_COUNT 8
#define MINE_WK2114_OLED_LINE_CHARS 21
#define MINE_WK2114_OLED_STATE_LINE_COUNT 2
#define MINE_WK2114_OLED_STATE_TOTAL_CHARS (MINE_WK2114_OLED_LINE_CHARS * MINE_WK2114_OLED_STATE_LINE_COUNT)
#define MINE_WK2114_OLED_DATA_LINE_COUNT 2
#define MINE_WK2114_OLED_DATA_TOTAL_CHARS (MINE_WK2114_OLED_LINE_CHARS * MINE_WK2114_OLED_DATA_LINE_COUNT)
#define MINE_WK2114_OLED_EVENT_BUFFER_LEN 64
#define MINE_WK2114_OLED_LINE_TITLE 0
#define MINE_WK2114_OLED_LINE_STATE0 1
#define MINE_WK2114_OLED_LINE_STATE1 2
#define MINE_WK2114_OLED_LINE_STATE MINE_WK2114_OLED_LINE_STATE0
#define MINE_WK2114_OLED_LINE_DATA0 3
#define MINE_WK2114_OLED_LINE_DATA1 4
#define MINE_WK2114_OLED_LINE_CFG 5
#define MINE_WK2114_OLED_LINE_RX 6
#define MINE_WK2114_OLED_LINE_TX 7

/* 日志缓存。 */
#define MINE_WK2114_LOG_BUFFER_LEN 192

/**
 * @brief WK2114 模块统一日志接口。
 *
 * @param fmt printf 风格格式化字符串。
 */
void mine_wk2114_log(const char *fmt, ...);

/**
 * @brief 初始化 WK2114 OLED 页面。
 */
void mine_wk2114_oled_init(void);

/**
 * @brief 刷新 OLED 脏行到屏幕。
 */
void mine_wk2114_oled_flush_pending(void);

/**
 * @brief 更新 OLED 状态行。
 *
 * @param text 状态文本。
 */
void mine_wk2114_oled_push_state(const char *text);

/**
 * @brief 上报一条数据事件到 OLED。
 *
 * @param prefix 方向前缀（含 RX/TX 字样）。
 * @param data   数据指针。
 * @param len    数据长度。
 */
void mine_wk2114_oled_push_data_event(const char *prefix, const uint8_t *data, uint16_t len);

/**
 * @brief 更新 OLED 当前通道/波特率显示。
 *
 * @param channel   子串口号，1~4；传 0 表示未选择。
 * @param baud_rate 当前波特率。
 */
void mine_wk2114_oled_set_channel(uint8_t channel, uint32_t baud_rate);

#endif /* MINE_WK2114_UART2_EXT_MODULE_H */
