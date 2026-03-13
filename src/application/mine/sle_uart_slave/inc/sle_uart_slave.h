/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - SLE UART 从机侧。
 */

#ifndef MINE_SLE_UART_SLAVE_H
#define MINE_SLE_UART_SLAVE_H

#include <stdint.h>

#include "errcode.h"
#include "sle_ssap_client.h"
#include "uart.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief Slave 任务优先级。
 */
#define MINE_SLE_UART_SLAVE_TASK_PRIO 24

/**
 * @brief Slave 任务栈大小。
 */
#define MINE_SLE_UART_SLAVE_TASK_STACK_SIZE 0x2000

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
 * @brief 与 Host 保持一致的服务 UUID。
 */
#define MINE_SLE_UART_SERVICE_UUID 0xABCD

/**
 * @brief 与 Host 保持一致的特征 UUID。
 */
#define MINE_SLE_UART_PROPERTY_UUID 0xBCDE

/**
 * @brief 从机扫描时匹配的目标广播名。
 */
#define MINE_SLE_UART_HOST_NAME "mine_sle_host"

/**
 * @brief 从机本地广播名。
 */
#define MINE_SLE_UART_SLAVE_NAME "mine_sle_slave"

/**
 * @brief Slave 侧回退 SLE MAC。
 */
#define MINE_SLAVE_FALLBACK_SLE_MAC {0xE2, 0x00, 0x73, 0xC8, 0x11, 0x02}

/**
 * @brief LD2402 启用开关，1=启用，0=禁用。
 */
#define MINE_LD2402_ENABLE 0

/**
 * @brief LD2402 所在 UART 总线。
 */
#define MINE_LD2402_UART_BUS MINE_UART2_BUS

/**
 * @brief ZW101 启用开关，1=启用，0=禁用。
 */
#define MINE_ZW101_ENABLE 1

/**
 * @brief ZW101 所在 UART 总线。
 */
#define MINE_ZW101_UART_BUS MINE_UART2_BUS

/**
 * @brief ZW101 自动录入开关，1=启用，0=禁用。
 */
#define MINE_ZW101_AUTO_ENROLL_ENABLE 0

/**
 * @brief 自动录入目标模板 ID。
 */
#define MINE_ZW101_AUTO_ENROLL_ID 1

/**
 * @brief ZW101 自动验证开关，1=启用，0=禁用。
 */
#define MINE_ZW101_AUTO_VERIFY_ENABLE 0

/**
 * @brief 自动验证周期（毫秒）。
 */
#define MINE_ZW101_AUTO_VERIFY_INTERVAL_MS 3000

/**
 * @brief 指纹库检索起始页。
 */
#define MINE_ZW101_SEARCH_START_PAGE 0

/**
 * @brief 指纹库检索页数。
 */
#define MINE_ZW101_SEARCH_PAGE_NUM 300

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
} mine_sle_uart_slave_msg_t;

/**
 * @brief 启动从机扫描。
 */
void mine_sle_uart_slave_start_scan(void);

/**
 * @brief 初始化 UART0 并注册接收回调。
 */
void mine_sle_uart_slave_uart_init(void);

/**
 * @brief 初始化从机侧 SLE 业务。
 *
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 初始化成功。
 * @retval 其他值             初始化失败。
 */
errcode_t mine_sle_uart_slave_init(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_SLE_UART_SLAVE_H */
