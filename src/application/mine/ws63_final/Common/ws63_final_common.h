/**
 * @file ws63_final_common.h
 * @brief WK2114 最终版公共层接口。
 *
 * 公共层职责：
 * 1) 提供各层都会复用的纯算法与配置查询；
 * 2) 不依赖任何硬件访问接口。
 */

#ifndef WS63_COMMON_H
#define WS63_COMMON_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 计算 WK2114 子串口波特率寄存器值。
 *
 * @param baud      目标波特率。
 * @param baud1_out BAUD1 输出。
 * @param baud0_out BAUD0 输出。
 * @param pres_out  PRES 输出。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_calc_baud_regs(uint32_t baud,
    uint8_t *baud1_out, uint8_t *baud0_out, uint8_t *pres_out);

/**
 * @brief 判断子串口号是否在有效范围（1~4）。
 *
 * @param sub_port 子串口号。
 * @return uint8_t 1=有效，0=无效。
 */
uint8_t ws63_is_subport_valid(uint8_t sub_port);

/**
 * @brief 查询子串口是否在配置中启用。
 *
 * @param sub_port 子串口号。
 * @return uint8_t 1=启用，0=禁用。
 */
uint8_t ws63_is_subport_enabled(uint8_t sub_port);

/**
 * @brief 查询子串口在配置中的默认启用状态。
 *
 * @param sub_port 子串口号。
 * @return uint8_t 1=配置启用，0=配置禁用。
 */
uint8_t ws63_is_subport_config_enabled(uint8_t sub_port);

/**
 * @brief 重置子串口运行态启用状态为配置默认值。
 */
void ws63_reset_subport_runtime_enable(void);

/**
 * @brief 设置子串口运行态启用状态。
 *
 * 说明：
 * 1) 仅改变运行态门控，不修改编译配置；
 * 2) 对配置禁用的子口，不允许运行态强制启用。
 *
 * @param sub_port 子串口号。
 * @param enable   1=启用，0=禁用。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_set_subport_runtime_enable(uint8_t sub_port, uint8_t enable);

/**
 * @brief 读取子串口配置波特率。
 *
 * @param sub_port 子串口号。
 * @return uint32_t 波特率，0 表示未配置。
 */
uint32_t ws63_get_subport_baud(uint8_t sub_port);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
