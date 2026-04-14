/**
 * @file ws63_final_task_debug.h
 * @brief ws63_final 调试命令子模块接口。
 */

#ifndef WS63_TASK_DEBUG_H
#define WS63_TASK_DEBUG_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化调试命令子模块。
 */
void ws63_task_debug_init(void);

/**
 * @brief 查询当前是否处于纯调试模式。
 *
 * 说明：纯调试模式下会跳过门锁编排任务的启动或让其保持挂起，
 * 仅保留调试命令处理链路。
 *
 * @return uint8_t 1=纯调试模式，0=正常门锁模式。
 */
uint8_t ws63_task_debug_is_debug_only_mode(void);

/**
 * @brief 设置纯调试模式开关。
 *
 * @param enable 1=进入纯调试模式，0=退出纯调试模式。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_debug_set_debug_only_mode(uint8_t enable);

/**
 * @brief 尝试消费一段来自 SLE 下行的数据作为调试命令输入。
 *
 * 说明：
 * 1) 若数据被判定为调试命令流，则写入命令组帧状态机并返回成功；
 * 2) 若数据不符合调试命令文本特征，则返回失败，由调用方继续执行原透传路径。
 *
 * @param data SLE 下行数据。
 * @param len  数据长度。
 * @return errcode_t ERRCODE_SUCC=已消费，其他值=未消费。
 */
errcode_t ws63_task_debug_try_consume_sle_downlink(const uint8_t *data, uint16_t len);

/**
 * @brief 周期处理调试命令子模块。
 *
 * @param now_ms 当前系统毫秒计时。
 */
void ws63_task_debug_process(uint32_t now_ms);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
