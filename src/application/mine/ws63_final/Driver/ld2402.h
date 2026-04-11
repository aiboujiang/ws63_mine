/**
 * @file ld2402.h
 * @brief LD2402 毫米波雷达驱动。
 */
#ifndef LD2402_H
#define LD2402_H

#include "errcode.h"
#include <stdint.h>

/**
 * @brief 初始化LD2402雷达模块并进行通信测试
 *
 * @param sub_port 对应的串口号
 * @return errcode_t 成功返回ERRCODE_SUCC，否则返回失败代码
 */
errcode_t ld2402_init(uint8_t sub_port);

/**
 * @brief 处理LD2402雷达接收到的数据
 *
 * @param sub_port 对应串口号
 * @param data 接收到的数据缓冲区
 * @param len 接收到的数据长度
 */
void ld2402_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 设置 LD2402 运行态日志开关。
 *
 * @param enable 1=开启，0=关闭。
 * @return errcode_t ERRCODE_SUCC 成功。
 */
errcode_t ld2402_set_data_log_enable(uint8_t enable);

/**
 * @brief 获取 LD2402 运行态日志开关状态。
 *
 * @return uint8_t 1=开启，0=关闭。
 */
uint8_t ld2402_get_data_log_enable(void);

/**
 * @brief 设置 LD2402 运行态日志最小输出间隔。
 *
 * @param gap_ms 间隔毫秒，0 表示不做时间节流。
 * @return errcode_t ERRCODE_SUCC 成功。
 */
errcode_t ld2402_set_data_log_gap_ms(uint32_t gap_ms);

/**
 * @brief 获取 LD2402 运行态日志最小输出间隔。
 *
 * @return uint32_t 间隔毫秒。
 */
uint32_t ld2402_get_data_log_gap_ms(void);

/**
 * @brief 获取最近一次解析到的 LD2402 距离值。
 *
 * @return int32_t 最近一次距离值；若尚未解析到有效数据，则返回 -1。
 */
int32_t ld2402_get_last_distance_mm(void);

/**
 * @brief 获取最近一次有效距离值的时间戳（毫秒）。
 *
 * @return uint32_t 最近一次更新时的系统 Tick 毫秒值。
 */
uint32_t ld2402_get_last_distance_tick_ms(void);

#endif
