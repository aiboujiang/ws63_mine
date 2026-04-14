/**
 * @file zw101.h
 * @brief ZW101 指纹模组驱动接口（ws63_final 重构版）。
 *
 * 设计说明：
 * 1) 仅保留门锁业务必需能力：ENROLL/VERIFY/ECHO/LIST/DEL/CLEAR/CANCEL；
 * 2) 协议帧细节与 ACK 解析全部封装在 Driver 层，Task 层只使用语义接口；
 * 3) VERIFY 默认参数由上层传入，便于与 sle_uart_slave 行为保持一致。
 */

#ifndef ZW101_H
#define ZW101_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief ZW101 ACK 结果。
 *
 * payload[0] 固定为 ACK 码，后续字节为命令私有参数。
 */
typedef struct {
    uint8_t ack_code;
    uint8_t payload[64];
    uint16_t payload_len;
} zw101_ack_result_t;

/**
 * @brief 初始化 ZW101 驱动并完成握手探测。
 *
 * @param sub_port WK2114 子串口号。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_init(uint8_t sub_port);

/**
 * @brief 查询驱动是否已就绪。
 *
 * @return uint8_t 1=就绪，0=未就绪。
 */
uint8_t zw101_is_ready(void);

/**
 * @brief 喂入子串口接收数据（由 WK2114 轮询回调调用）。
 *
 * @param sub_port 子串口号。
 * @param data 接收缓冲区。
 * @param len 接收长度。
 */
void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 执行 ECHO 命令（GetEcho 0x53）。
 *
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 表示链路可达，其他失败。
 */
errcode_t zw101_echo(uint8_t *ack_out);

/**
 * @brief 查询当前是否有手指按压在 ZW101 传感器上（PS_GetImageInfo 0x3D）。
 *
 * ACK 语义：0x00=有手指，0x02=无手指。
 *
 * @param finger_present_out 输出按压状态：1=有手指，0=无手指。
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 表示成功获取状态，其他失败。
 */
errcode_t zw101_check_finger_present(uint8_t *finger_present_out, uint8_t *ack_out);

/**
 * @brief 执行自动注册（AutoEnroll 0x31）。
 *
 * @param page_id 目标模板 ID。
 * @param enroll_times 采样次数（2~6）。
 * @param param_flags 参数位。
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 表示注册成功，其他失败。
 */
errcode_t zw101_enroll(uint16_t page_id, uint8_t enroll_times, uint16_t param_flags, uint8_t *ack_out);

/**
 * @brief 执行自动验证（AutoIdentify 0x32）。
 *
 * @param score_level 安全等级（1~5）。
 * @param target_id 目标 ID；0xFFFF 表示 1:N。
 * @param param_flags 参数位。
 * @param match_id_out 输出匹配 ID，可为 NULL。
 * @param score_out 输出匹配分数，可为 NULL。
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 表示验证通过，其他失败。
 */
errcode_t zw101_verify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out,
    uint8_t *ack_out);

/**
 * @brief 查询有效模板数量（0x1D）。
 *
 * @param valid_num_out 输出模板数量。
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_list(uint16_t *valid_num_out, uint8_t *ack_out);

/**
 * @brief 删除模板（0x0C）。
 *
 * @param page_id 起始模板 ID。
 * @param count 删除数量。
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_delete(uint16_t page_id, uint16_t count, uint8_t *ack_out);

/**
 * @brief 清空模板库（0x0D）。
 *
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_clear(uint8_t *ack_out);

/**
 * @brief 取消当前流程（0x30）。
 *
 * @param ack_out 输出 ACK，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_cancel(uint8_t *ack_out);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
