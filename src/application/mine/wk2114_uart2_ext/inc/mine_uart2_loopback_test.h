/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: UART2 物理短接回环测试接口（TX<->RX）。
 */

#ifndef MINE_UART2_LOOPBACK_TEST_H
#define MINE_UART2_LOOPBACK_TEST_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 执行 UART2 回环测试。
 *
 * 关键流程：
 * 1) 独占初始化 UART2（会先 deinit 再 init）；
 * 2) 发送固定测试帧并等待回读；
 * 3) 校验收发数据一致；
 * 4) 测试结束后释放 UART2。
 *
 * @param baud_rate 测试波特率，传 0 使用默认 115200。
 * @param rounds    测试轮次，传 0 使用默认 10 轮。
 * @param timeout_ms 单轮回读超时（ms），传 0 使用默认 100ms。
 * @return errcode_t
 * @retval ERRCODE_SUCC 测试通过
 * @retval ERRCODE_FAIL 测试失败
 */
errcode_t mine_uart2_loopback_test_run(uint32_t baud_rate, uint16_t rounds, uint32_t timeout_ms);

/**
 * @brief 使用默认参数执行 UART2 回环测试。
 *
 * 默认参数：115200, 10 轮, 100ms。
 *
 * @return errcode_t
 */
errcode_t mine_uart2_loopback_test_default(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_UART2_LOOPBACK_TEST_H */
