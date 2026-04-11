/**
 * @file ws63_final_sle.h
 * @brief WS63 final 的 SLE 从机桥接中间件接口。
 *
 * 分层约束：
 * 1) 本模块位于 Middleware，仅负责 SLE 协议侧能力；
 * 2) 串口/子口收发由 App/Task 或 Driver 处理；
 * 3) 通过回调把下行数据交给上层，不在本层直接触碰业务模块。
 */

#ifndef WS63_FINAL_SLE_H
#define WS63_FINAL_SLE_H

#include <stdbool.h>
#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief SLE 上行数据来源模块标识。
 */
typedef enum {
    WS63_SLE_MODULE_UNKNOWN = 0,
    WS63_SLE_MODULE_LD2402,
    WS63_SLE_MODULE_ZW101,
    WS63_SLE_MODULE_CAMERA,
} ws63_sle_module_t;

/**
 * @brief SLE 下行数据回调类型。
 *
 * @param data 下行负载。
 * @param len  负载长度。
 * @return errcode_t 0 表示处理成功，非 0 表示处理失败。
 */
typedef errcode_t (*ws63_sle_downlink_cb_t)(const uint8_t *data, uint16_t len);

/**
 * @brief 初始化 SLE 从机桥接模块。
 *
 * @param downlink_cb 下行回调，可为 NULL（表示下行数据忽略）。
 * @return errcode_t ERRCODE_SUCC 成功，其他为失败码。
 */
errcode_t ws63_sle_init(ws63_sle_downlink_cb_t downlink_cb);

/**
 * @brief 周期处理 SLE 桥接状态机。
 */
void ws63_sle_process(void);

/**
 * @brief 查询当前 SLE 链路是否已连接且可发送。
 *
 * @return true  可发送。
 * @return false 未连接或特征句柄未就绪。
 */
bool ws63_sle_ready(void);

/**
 * @brief 将子口数据上行到 SLE Host。
 *
 * @param sub_port 数据来源子口号。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他为失败码。
 */
errcode_t ws63_sle_send_subport_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 设置 SLE 上行 success 日志开关。
 *
 * @param enable 1=开启，0=关闭。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_sle_set_uplink_success_log_enable(uint8_t enable);

/**
 * @brief 获取 SLE 上行 success 日志开关。
 *
 * @return uint8_t 1=开启，0=关闭。
 */
uint8_t ws63_sle_get_uplink_success_log_enable(void);

/**
 * @brief 设置 SLE 上行 success 日志最小输出间隔。
 *
 * @param gap_ms 间隔毫秒，0 表示每次成功都打印。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_sle_set_uplink_success_log_gap_ms(uint32_t gap_ms);

/**
 * @brief 获取 SLE 上行 success 日志最小输出间隔。
 *
 * @return uint32_t 间隔毫秒。
 */
uint32_t ws63_sle_get_uplink_success_log_gap_ms(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
