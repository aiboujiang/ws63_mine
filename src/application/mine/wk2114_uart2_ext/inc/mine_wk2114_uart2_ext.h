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
 * @brief 初始化 WK2114 UART2 扩展模块。
 *
 * @return errcode_t
 * @retval ERRCODE_SUCC 初始化成功。
 * @retval 其他值       初始化失败。
 */
errcode_t mine_wk2114_uart2_ext_init(void);

/**
 * @brief 配置并使能指定子串口。
 *
 * 该接口会按规格书流程设置 SPAGE/BAUD/PRES/SCR/FCR，
 * 同时更新 OLED 通道与波特率显示。
 *
 * @param channel   子串口号，范围 1~4。
 * @param baud_rate 目标波特率。
 * @return errcode_t
 * @retval ERRCODE_SUCC         配置成功。
 * @retval ERRCODE_INVALID_PARAM 参数非法。
 * @retval 其他值               底层发送失败。
 */
errcode_t mine_wk2114_uart2_ext_set_subuart_baud(uint8_t channel, uint32_t baud_rate);

/**
 * @brief 通过 WK2114 向指定子串口发送数据。
 *
 * @param channel 子串口号，范围 1~4。
 * @param data    待发送数据。
 * @param len     数据长度。
 * @return errcode_t
 * @retval ERRCODE_SUCC          发送成功。
 * @retval ERRCODE_UART_NOT_INIT 模块未初始化。
 * @retval ERRCODE_INVALID_PARAM 参数非法。
 * @retval 其他值                发送失败。
 */
errcode_t mine_wk2114_uart2_ext_send(uint8_t channel, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_WK2114_UART2_EXT_H */
