/**
 * @file wk2114.h
 * @brief WK2114 最终版驱动层接口。
 *
 * 驱动层职责：
 * 1) 基于 BSP 封装 WK2114 寄存器与 FIFO 访问；
 * 2) 对上层暴露统一子串口读写接口；
 * 3) 不直接承载具体业务策略。
 */

#ifndef WK2114_H
#define WK2114_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 主口链路状态。
 */
typedef struct {
    uint8_t matched;
    uint8_t last_gena;
} wk2114_link_status_t;

/**
 * @brief 初始化 WK2114 驱动主口与基础链路。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t wk2114_init(void);

/**
 * @brief 初始化指定子串口并设置波特率。
 *
 * @param sub_port 子串口号（1~4）。
 * @param baud     波特率。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t wk2114_subport_init(uint8_t sub_port, uint32_t baud);

/**
 * @brief 向指定子串口写入数据。
 *
 * @param sub_port 子串口号（1~4）。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t wk2114_subport_write(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 从指定子串口读取数据。
 *
 * @param sub_port 子串口号（1~4）。
 * @param data     输出缓冲区。
 * @param max_len  最大读取长度。
 * @return uint8_t 实际读取字节数。
 */
uint8_t wk2114_subport_read(uint8_t sub_port, uint8_t *data, uint8_t max_len);

/**
 * @brief 设置子串口休眠状态。
 *
 * @param sub_port 子串口号（1~4）。
 * @param sleep_en 1=进入休眠，0=退出休眠。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t wk2114_subport_set_sleep(uint8_t sub_port, uint8_t sleep_en);

/**
 * @brief 查询子串口是否处于休眠状态。
 *
 * @param sub_port 子串口号（1~4）。
 * @param sleep_en_out 输出休眠状态（1=休眠，0=非休眠）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t wk2114_subport_get_sleep(uint8_t sub_port, uint8_t *sleep_en_out);

/**
 * @brief 获取当前主口链路状态。
 *
 * @param status 输出结构体。
 */
void wk2114_get_link_status(wk2114_link_status_t *status);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
