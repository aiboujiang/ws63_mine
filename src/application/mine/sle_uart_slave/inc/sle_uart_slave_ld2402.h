/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 LD2402 业务模块接口。
 */

#ifndef MINE_SLE_UART_SLAVE_LD2402_H
#define MINE_SLE_UART_SLAVE_LD2402_H

#include <stdbool.h>
#include <stdint.h>

#include "uart.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化 LD2402 模块并执行基础连通性检测。
 *
 * @param bus LD2402 所在 UART 总线。
 * @return true  初始化成功且设备可用。
 * @return false 初始化失败或设备不可用。
 */
bool mine_ld2402_init(uart_bus_t bus);

/**
 * @brief 向 LD2402 协议解析器喂入串口数据。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 数据缓冲区。
 * @param len  数据长度。
 */
void mine_ld2402_feed(uart_bus_t bus, const uint8_t *data, uint16_t len);

/**
 * @brief 尝试处理 LD2402 串口调试命令。
 *
 * 支持通过串口输入文本命令触发版本读取、参数配置、保存参数等动作。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 输入数据缓冲区。
 * @param len  输入数据长度。
 * @return true  本次数据已作为调试命令消费，不再透传。
 * @return false 非调试命令数据，保持原流程透传。
 */
bool mine_ld2402_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len);

/**
 * @brief 轮询处理 LD2402 业务任务。
 */
void mine_ld2402_process(void);

/**
 * @brief 获取待刷新的 LD2402 状态文本。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return true  获取成功且有新状态。
 * @return false 无新状态或参数无效。
 */
bool mine_ld2402_get_status(char *buf, uint16_t buf_len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_SLE_UART_SLAVE_LD2402_H */
