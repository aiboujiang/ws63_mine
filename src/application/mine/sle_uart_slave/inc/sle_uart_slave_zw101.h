/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine 示例 - 从机侧 ZW101 指纹业务模块对外接口。
 *
 * 说明:
 * 1) 本模块对外保持稳定 API，供 sle_uart_slave 主循环调用；
 * 2) 内部实现采用手册定义的自动注册/自动验证/删除模板流程；
 * 3) 所有接口均为线程安全调用语义：
 *    - UART 回调路径调用 feed/try_handle_debug_cmd；
 *    - 任务线程周期调用 process/get_status。
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
 * @brief 初始化 ZW101 模块并执行设备探测。
 *
 * 输入参数:
 * - bus: ZW101 模块连接的 UART 总线号。
 *
 * 返回参数:
 * - true:  初始化成功，设备可用。
 * - false: 初始化失败或设备暂不可用。
 *
 * 作用:
 * - 绑定 HAL 回调；
 * - 初始化协议解析上下文；
 * - 执行握手与传感器检查。
 */
bool mine_zw101_init(uart_bus_t bus);

/**
 * @brief 向 ZW101 协议解析器投喂串口字节流。
 *
 * 输入参数:
 * - bus:  数据来源 UART 总线。
 * - data: 输入数据缓冲区指针。
 * - len:  输入数据长度（字节）。
 *
 * 返回参数:
 * - 无。
 *
 * 作用:
 * - 将 UART 原始数据交给协议状态机解析；
 * - 触发 ACK 回调，驱动自动流程状态更新。
 */
void mine_zw101_feed(uart_bus_t bus, const uint8_t *data, uint16_t len);

/**
 * @brief 尝试解析并消费调试串口文本命令。
 *
 * 输入参数:
 * - bus:  数据来源 UART 总线。
 * - data: 输入字节流。
 * - len:  输入长度（字节）。
 *
 * 返回参数:
 * - true:  本次数据已被识别为 ZW101 调试命令并消费。
 * - false: 非命令数据，调用方应继续原有透传逻辑。
 *
 * 作用:
 * - 支持命令：HELP/STATUS/LIST/ENROLL/VERIFY/DEL/CLEAR/CANCEL。
 */
bool mine_zw101_try_handle_debug_cmd(uart_bus_t bus, const uint8_t *data, uint16_t len);

/**
 * @brief 请求执行一次自动注册模板。
 *
 * 输入参数:
 * - template_id: 目标模板 ID。
 *
 * 返回参数:
 * - true:  请求已受理，后续在 process 中执行。
 * - false: 当前未就绪或命令槽位忙。
 *
 * 作用:
 * - 将自动注册命令写入执行槽位。
 */
bool mine_zw101_request_enroll(uint16_t template_id);

/**
 * @brief 请求执行一次自动验证（默认 1:N）。
 *
 * 输入参数:
 * - 无。
 *
 * 返回参数:
 * - true:  请求已受理，后续在 process 中执行。
 * - false: 当前未就绪或命令槽位忙。
 *
 * 作用:
 * - 将自动验证命令写入执行槽位。
 */
bool mine_zw101_request_verify(void);

/**
 * @brief ZW101 周期处理函数。
 *
 * 输入参数:
 * - 无。
 *
 * 返回参数:
 * - 无。
 *
 * 作用:
 * - 执行后台重探测；
 * - 执行自动验证周期调度（若启用）；
 * - 消费并执行一条待处理命令。
 */
void mine_zw101_process(void);

/**
 * @brief 获取一条最新状态文本（读后清脏）。
 *
 * 输入参数:
 * - buf:     输出缓冲区。
 * - buf_len: 输出缓冲区长度。
 *
 * 返回参数:
 * - true:  成功读取到一条新状态。
 * - false: 当前无新状态或输入参数无效。
 *
 * 作用:
 * - 给 OLED/上层日志模块提供简洁状态。
 */
bool mine_zw101_get_status(char *buf, uint16_t buf_len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_SLE_UART_SLAVE_ZW101_H */
