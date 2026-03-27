/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块公共接口。
 */

#ifndef MINE_WK2114_UART2_EXT_H
#define MINE_WK2114_UART2_EXT_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化 WK2114 芯片。
 *
 * 初始化包含以下流程：
 * 1) 拉高RST 10ms
 * 2) 拉低RST 10ms复位
 * 3) 再拉高20ms完成复位
 * 4) 复位完成后发送0x55完成波特率匹配
 *
 * @return errcode_t
 * @retval ERRCODE_SUCC 初始化成功
 * @retval 其他         初始化失败
 */
errcode_t mine_wk2114_init_chip(void);

/**
 * @brief 初始化 WK2114 扩展模块。
 *
 * 初始化包含应用笔记流程：
 * 1) 上电复位和硬件复位
 * 2) 发送 0x55 自适应波特率
 * 3) 读取固定寄存器 GENA(默认 0xF0)确认链路
 * 4) 执行 GENA 写回操作，确认读写正常
 *
 * @return errcode_t
 * @retval ERRCODE_SUCC 初始化成功
 * @retval 其他         初始化失败
 */
errcode_t mine_wk2114_uart2_ext_init(void);

/**
 * @brief 设置指定子串口波特率。
 *
 * 包含应用笔记流程：
 * 1) 读取固定寄存器 SPAGE/BAUD/PRES/SCR/FCR
 * 2) 读取关键寄存器的值进行校验。
 *
 * @param channel 子串口号，范围 1~4
 * @param baud_rate 子串口波特率
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_set_subuart_baud(uint8_t channel, uint32_t baud_rate);

/**
 * @brief 通过 WK2114 串口发送数据。
 *
 * @param channel 子串口号，范围 1~4
 * @param data 数据指针。
 * @param len 数据长度
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_send(uint8_t channel, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_WK2114_UART2_EXT_H */
