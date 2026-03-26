/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块对外接口。
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
 * @brief 初始化 WK2114 扩展模块。
 *
 * 初始化流程基于应用笔记：
 * 1) 上电后先做硬件复位；
 * 2) 发送 0x55 锁定主口波特率；
 * 3) 读取固定寄存器 GENA(默认 0xF0)确认链路；
 * 4) 执行 GENA 写读回，确认读写都正常。
 *
 * @return errcode_t
 * @retval ERRCODE_SUCC 初始化成功。
 * @retval 其他         初始化失败。
 */
errcode_t mine_wk2114_uart2_ext_init(void);

/**
 * @brief 配置并使能指定子串口波特率。
 *
 * 会按应用笔记流程配置 SPAGE/BAUD/PRES/SCR/FCR，
 * 并对关键寄存器进行读回校验。
 *
 * @param channel 子串口号，范围 1~4。
 * @param baud_rate 子串口波特率。
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_set_subuart_baud(uint8_t channel, uint32_t baud_rate);

/**
 * @brief 通过 WK2114 子串口发送数据。
 *
 * @param channel 子串口号，范围 1~4。
 * @param data 数据指针。
 * @param len 数据长度。
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_send(uint8_t channel, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_WK2114_UART2_EXT_H */
