/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - SLE UART host side.
 */

#ifndef MINE_SLE_UART_HOST_H
#define MINE_SLE_UART_HOST_H

#include <stdint.h>
#include "errcode.h"
#include "sle_ssap_server.h"
#include "uart.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief Host 任务优先级。
 */
#define MINE_SLE_UART_HOST_TASK_PRIO 24

/**
 * @brief Host 任务栈大小。
 */
#define MINE_SLE_UART_HOST_TASK_STACK_SIZE 0x2000

/**
 * @brief UART 使能位定义（可按位组合）。
 */
#define MINE_UART_EN_UART0 (1U << UART_BUS_0)
#define MINE_UART_EN_UART1 (1U << UART_BUS_1)
#define MINE_UART_EN_UART2 (1U << UART_BUS_2)

/**
 * @brief UART 使能掩码。
 *
 * 默认仅启用 UART0。若需同时启用多路，请按位或组合：
 * 例如：MINE_UART_EN_UART0 | MINE_UART_EN_UART1 | MINE_UART_EN_UART2
 */
#define MINE_UART_ENABLE_MASK (MINE_UART_EN_UART0 | MINE_UART_EN_UART2)

/**
 * @brief UART0 引脚配置。
 */
#define MINE_UART0_TXD_PIN 17
#define MINE_UART0_RXD_PIN 18
#define MINE_UART0_PIN_MODE 1
#define MINE_UART0_BUS UART_BUS_0

/**
 * @brief UART1 引脚配置（默认未配置，启用前请按板级资源修改）。
 */
#define MINE_UART1_TXD_PIN PIN_NONE
#define MINE_UART1_RXD_PIN PIN_NONE
#define MINE_UART1_PIN_MODE 1
#define MINE_UART1_BUS UART_BUS_1

/**
 * @brief UART2 引脚配置（默认未配置，启用前请按板级资源修改）。
 */
#define MINE_UART2_TXD_PIN 8
#define MINE_UART2_RXD_PIN 7
#define MINE_UART2_PIN_MODE 2
#define MINE_UART2_BUS UART_BUS_2

/**
 * @brief 本 demo 使用的 SLE 服务 UUID。
 */
#define MINE_SLE_UART_SERVICE_UUID 0xABCD

/**
 * @brief 本 demo 使用的 SLE 特征 UUID。
 */
#define MINE_SLE_UART_PROPERTY_UUID 0xBCDE

/**
 * @brief Host 广播名（从机会按该名称过滤目标设备）。
 */
#define MINE_SLE_UART_HOST_NAME "mine_sle_host"

/**
 * @brief Host 侧回退 SLE MAC。
 */
#define MINE_HOST_FALLBACK_SLE_MAC {0xE2, 0x00, 0x73, 0xC8, 0x11, 0x01}

/**
 * @brief UART 与 SLE 之间转发的数据结构。
 *
 * @param value     数据缓冲区指针。
 * @param value_len 数据长度（字节）。
 */
typedef struct {
    uint8_t uart_bus;
    uint8_t *value;
    uint16_t value_len;
} mine_sle_uart_host_msg_t;

/**
 * @brief 初始化 Host 侧 SLE 业务（服务、特征、回调、广播）。
 *
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 初始化成功。
 * @retval 其他值             初始化失败。
 */
errcode_t mine_sle_uart_host_init(void);

/**
 * @brief 初始化 UART0 并注册接收回调。
 */
void mine_sle_uart_host_uart_init(void);

/**
 * @brief 通过 SLE notify 将数据发送给从机。
 *
 * @param msg 发送消息。
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 发送成功。
 * @retval 其他值             发送失败。
 */
errcode_t mine_sle_uart_host_send_by_handle(const mine_sle_uart_host_msg_t *msg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_SLE_UART_HOST_H */
