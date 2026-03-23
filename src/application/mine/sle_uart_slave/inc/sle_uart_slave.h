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
 * @brief UART2 挂载 Camera 模式开关，1=启用，0=禁用。
 */
#define MINE_UART2_MODE_CAMERA_ENABLE 0

/**
 * @brief UART2 挂载 LD2402 模式开关，1=启用，0=禁用。
 */
#define MINE_UART2_MODE_LD2402_ENABLE 1

/**
 * @brief UART2 挂载 ZW101 模式开关，1=启用，0=禁用。
 */
#define MINE_UART2_MODE_ZW101_ENABLE 0

/* UART2 三种用途必须互斥：Camera / LD2402 / ZW101 仅可启用一项。 */
#if ((MINE_UART2_MODE_CAMERA_ENABLE != 0) && (MINE_UART2_MODE_CAMERA_ENABLE != 1))
#error "MINE_UART2_MODE_CAMERA_ENABLE must be 0 or 1"
#endif

#if ((MINE_UART2_MODE_LD2402_ENABLE != 0) && (MINE_UART2_MODE_LD2402_ENABLE != 1))
#error "MINE_UART2_MODE_LD2402_ENABLE must be 0 or 1"
#endif

#if ((MINE_UART2_MODE_ZW101_ENABLE != 0) && (MINE_UART2_MODE_ZW101_ENABLE != 1))
#error "MINE_UART2_MODE_ZW101_ENABLE must be 0 or 1"
#endif

#if ((MINE_UART2_MODE_CAMERA_ENABLE + MINE_UART2_MODE_LD2402_ENABLE + MINE_UART2_MODE_ZW101_ENABLE) != 1)
#error "UART2 mode must be mutually exclusive: enable exactly one mode"
#endif

/* 由 UART2 互斥模式自动推导模块开关。 */
#define MINE_CAMERA_ENABLE MINE_UART2_MODE_CAMERA_ENABLE
#define MINE_LD2402_ENABLE MINE_UART2_MODE_LD2402_ENABLE
#define MINE_ZW101_ENABLE MINE_UART2_MODE_ZW101_ENABLE

/* 兼容旧宏：历史代码中 "NORMAL" 即当前 CAMERA 模式。 */
#define MINE_UART2_MODE_NORMAL_ENABLE MINE_UART2_MODE_CAMERA_ENABLE
#define MINE_UART2_PASSTHROUGH_ENABLE MINE_CAMERA_ENABLE

/* UART2 角色用于日志与 OLED 调试显示。 */
#if (MINE_UART2_MODE_CAMERA_ENABLE == 1)
#define MINE_UART2_MODE_NAME "CAMERA"
#elif (MINE_UART2_MODE_LD2402_ENABLE == 1)
#define MINE_UART2_MODE_NAME "LD2402"
#else
#define MINE_UART2_MODE_NAME "ZW101"
#endif

/* 既然 UART2 必须三选一，就要求掩码中必须使能 UART2。 */
#if ((MINE_UART_ENABLE_MASK & MINE_UART_EN_UART2) == 0U)
#error "UART2 mode selected but UART2 is disabled in MINE_UART_ENABLE_MASK"
#endif

/**
 * @brief LD2402 所在 UART 总线。
 */
#define MINE_LD2402_UART_BUS MINE_UART2_BUS

/**
 * @brief Camera 所在 UART 总线。
 */
#define MINE_CAMERA_UART_BUS MINE_UART2_BUS

/**
 * @brief Camera 串口调试命令开关，1=启用，0=禁用。
 */
#define MINE_CAMERA_DEBUG_CMD_ENABLE 1

/**
 * @brief Camera 串口调试命令输入总线。
 */
#define MINE_CAMERA_DEBUG_UART_BUS MINE_UART0_BUS

/**
 * @brief LD2402 串口调试命令开关，1=启用，0=禁用。
 */
#define MINE_LD2402_DEBUG_CMD_ENABLE 1

/**
 * @brief LD2402 串口调试命令输入总线。
 */
#define MINE_LD2402_DEBUG_UART_BUS MINE_UART0_BUS

/**
 * @brief ZW101 所在 UART 总线。
 */
#define MINE_ZW101_UART_BUS MINE_UART2_BUS

/**
 * @brief ZW101 串口波特率（仅作用于指纹模块所在总线）。
 */
#define MINE_ZW101_UART_BAUD 57600

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
 * @brief ZW101 串口调试命令开关，1=启用，0=禁用。
 */
#define MINE_ZW101_DEBUG_CMD_ENABLE 1

/**
 * @brief ZW101 串口调试命令输入总线。
 */
#define MINE_ZW101_DEBUG_UART_BUS MINE_UART0_BUS

/**
 * @brief ZW101 原始 UART2 数据透传开关，1=透传，0=仅上报状态文本。
 *
 * 默认关闭：ZW101 协议为二进制帧，直接透传到主机会出现乱码；
 * 关闭后仅保留例如 [ZW101]VERIFYING / [ZW101]VERIFY SUCCESS 的可读上报。
 */
#define MINE_ZW101_RAW_UPLINK_ENABLE 0

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
