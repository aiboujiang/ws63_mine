/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 ZW101 业务模块接口。
 */

#ifndef MINE_SLE_UART_SLAVE_ZW101_H
#define MINE_SLE_UART_SLAVE_ZW101_H

#include <stdbool.h>
#include <stdint.h>

#include "uart.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化 ZW101 模块并完成设备检测。
 *
 * @param bus ZW101 所在 UART 总线。
 * @return true  初始化成功且设备可用。
 * @return false 初始化失败或设备不可用。
 */
bool mine_zw101_init(uart_bus_t bus);

/**
 * @brief 向 ZW101 协议解析器喂入串口数据。
 *
 * @param bus  数据来源 UART 总线。
 * @param data 数据缓冲区。
 * @param len  数据长度。
 */
void mine_zw101_feed(uart_bus_t bus, const uint8_t *data, uint16_t len);

/**
 * @brief 请求执行一次指纹录入（注册）。
 *
 * @param template_id 目标模板 ID。
 * @return true  请求已受理。
 * @return false 模块未就绪或当前忙。
 */
bool mine_zw101_request_enroll(uint16_t template_id);

/**
 * @brief 请求执行一次指纹验证（1:N 识别）。
 *
 * @return true  请求已受理。
 * @return false 模块未就绪或当前忙。
 */
bool mine_zw101_request_verify(void);

/**
 * @brief 轮询处理 ZW101 业务任务。
 */
void mine_zw101_process(void);

/**
 * @brief 获取待刷新的 ZW101 状态文本。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return true  获取成功且有新状态。
 * @return false 无新状态或参数无效。
 */
bool mine_zw101_get_status(char *buf, uint16_t buf_len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_SLE_UART_SLAVE_ZW101_H */
