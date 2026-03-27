/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Internal configuration for Mine WK2114 UART2 extension module.
 */

#ifndef MINE_WK2114_UART2_EXT_MODULE_H
#define MINE_WK2114_UART2_EXT_MODULE_H

#include <stdint.h>

#include "mine_wk2114_uart2_ext.h"
#include "uart.h"

/* Task parameters. */
#define MINE_WK2114_TASK_PRIO 24
#define MINE_WK2114_TASK_STACK_SIZE 0x1800
#define MINE_WK2114_TASK_INIT_DELAY_MS 200
#define MINE_WK2114_TASK_LOOP_WAIT_MS 80
#define MINE_WK2114_INIT_RETRY_WAIT_MS 1200

/* Board wiring: UART2->MRX/MTX, GPIO10->RST, GPIO13->IRQ. */
#define MINE_WK2114_HOST_UART_BUS UART_BUS_2
#define MINE_WK2114_HOST_UART_TX_PIN 8
#define MINE_WK2114_HOST_UART_RX_PIN 7
/* Official documentation emphasizes: Master TX->WK MRX, Master RX<-WK MTX; if board netlist differs from comments, swapping is allowed as fallback. */
#define MINE_WK2114_HOST_UART_SWAP_TX_PIN 7
#define MINE_WK2114_HOST_UART_SWAP_RX_PIN 8
#define MINE_WK2114_HOST_UART_SWAP_RX_GPIO_PIN GPIO_08
/* Main port RX corresponding GPIO, used for reading logic level during timeout for oscilloscope-free diagnosis. */
#define MINE_WK2114_HOST_UART_RX_GPIO_PIN GPIO_07
#define MINE_WK2114_HOST_UART_PIN_MODE 2
#define MINE_WK2114_HOST_UART_ALT_PIN_MODE 1    
#define MINE_WK2114_HOST_UART_PROFILE_COUNT 4
/* 关键流程注释：由于硬件晶振为12MHz，在115200波特率下分频系数约为6.51 */
#define MINE_WK2114_HOST_UART_BAUD 115200
#define MINE_WK2114_HOST_UART_RX_BUFFER_SIZE 512

#define MINE_WK2114_RESET_GPIO_PIN GPIO_10
/*
 * 应用笔记 5.2：示例时序是先将 RST 初始化为高电平，随后在自适应流程中执行
 * "拉低 10ms -> 拉高 100ms -> 发送 0x55 -> 延时 100ms"。
 */
#define MINE_WK2114_RESET_HOLD_MS 1U
#define MINE_WK2114_RESET_LOW_HOLD_MS 10U
#define MINE_WK2114_RESET_RELEASE_WAIT_MS 100U

#define MINE_WK2114_IRQ_GPIO_PIN GPIO_13
#define MINE_WK2114_IRQ_PIN_MODE HAL_PIO_FUNC_GPIO
#define MINE_WK2114_IRQ_TRIGGER_MODE GPIO_INTERRUPT_LOW

/* Main port protocol parameters. */
#define MINE_WK2114_HOST_CMD_WRITE_REG 0x00U
#define MINE_WK2114_HOST_CMD_READ_REG 0x40U
#define MINE_WK2114_HOST_CMD_WRITE_FIFO 0x80U
/* Compatible read command format: Some scenarios require carrying 1 dummy byte when reading registers. */
#define MINE_WK2114_HOST_READ_DUMMY_BYTE 0xFFU
/* Manual default: Main port read register sends only 1-byte CMD; this switch is for field troubleshooting only. */
#define MINE_WK2114_HOST_READ_DUMMY_FALLBACK_ENABLE 0U
#define MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE 0x55U
/* 应用笔记 5.2 示例：复位后发送 1 个 0x55 完成主口波特率锁定。 */
#define MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY 1U
/* Interval when continuously sending 0x55: Must be 0 to ensure clock waveform is continuous and reliably locked. */
#define MINE_WK2114_HOST_AUTOBAUD_SYNC_INTERVAL_MS 0U
#define MINE_WK2114_HOST_AUTOBAUD_LOCK_WAIT_MS 100U
/* Critical process note: When first baud lock fails, execute enhanced 0x55 burst as field compatibility fallback. */
#define MINE_WK2114_HOST_AUTOBAUD_FALLBACK_BURST_COUNT 32U
#define MINE_WK2114_HOST_AUTOBAUD_FALLBACK_INTERVAL_MS 0U
#define MINE_WK2114_HOST_AUTOBAUD_FALLBACK_WAIT_MS 40U

/* Main port register read timeout. */
#define MINE_WK2114_HOST_READ_TIMEOUT_MS 200U
#define MINE_WK2114_HOST_RESP_STABLE_WAIT_MS 2U
#define MINE_WK2114_HOST_RESP_FIFO_SIZE 64U
#define MINE_WK2114_LINK_CHECK_READ_RETRY 6U
#define MINE_WK2114_HOST_RX_DRAIN_MAX 64U

/* UART2 conduction self-test parameters: Used to verify if chip-side UART2 TX/RX loopback is effective. */
#define MINE_WK2114_UART2_LOOPBACK_SELFTEST_ENABLE 1U
#define MINE_WK2114_UART2_LOOPBACK_TIMEOUT_MS 80U
#define MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN 4U

/* Device and sub-serial port parameters. */
#define MINE_WK2114_XTAL_HZ 12000000U
#define MINE_WK2114_SUBUART_COUNT 4
#define MINE_WK2114_SUBUART_MIN 1
#define MINE_WK2114_SUBUART_MAX 4
#define MINE_WK2114_FIFO_CHUNK_MAX 16
#define MINE_WK2114_UART_FRAME_MAX (MINE_WK2114_FIFO_CHUNK_MAX + 1)

/* Register address definitions. */
#define MINE_WK2114_ADDR_GENA 0x00
#define MINE_WK2114_ADDR_GRST 0x01
#define MINE_WK2114_ADDR_GIER 0x10
#define MINE_WK2114_SUBREG_SPAGE 0x03
#define MINE_WK2114_SUBREG_SCR 0x04
#define MINE_WK2114_SUBREG_FCR 0x06
#define MINE_WK2114_SUBREG_SIER 0x07
#define MINE_WK2114_SUBREG_TFCNT 0x09
#define MINE_WK2114_SUBREG_FSR 0x0B
#define MINE_WK2114_SUBREG_BAUD1 0x04
#define MINE_WK2114_SUBREG_BAUD0 0x05
#define MINE_WK2114_SUBREG_PRES 0x06
#define MINE_WK2114_SUBREG_RFTL 0x07
#define MINE_WK2114_SUBREG_TFTL 0x08

/* Sub-serial port initialization parameters: Aligned with official wk2xxx_uart.c. */
#define MINE_WK2114_SIER_RFTRIG_IEN 0x01U
#define MINE_WK2114_SIER_RXOUT_IEN 0x02U
#define MINE_WK2114_SIER_INIT_MASK (MINE_WK2114_SIER_RFTRIG_IEN | MINE_WK2114_SIER_RXOUT_IEN)
#define MINE_WK2114_FCR_INIT_ASSERT 0xFFU
#define MINE_WK2114_FCR_INIT_RELEASE 0xFCU
#define MINE_WK2114_RFTL_INIT_LEVEL 0x40U
#define MINE_WK2114_TFTL_INIT_LEVEL 0x10U
#define MINE_WK2114_FSR_TFULL_BIT 0x02U
#define MINE_WK2114_SUBUART_FIFO_DEPTH 256U

#define MINE_WK2114_GENA_RESERVED_MASK 0xF0U

/* Log buffer length. */
#define MINE_WK2114_LOG_BUFFER_LEN 192

/**
 * @brief WK2114 模块统一日志接口。
 *
 * @param fmt 格式化字符串。
 */
void mine_wk2114_log(const char *fmt, ...);

#endif /* MINE_WK2114_UART2_EXT_MODULE_H */
