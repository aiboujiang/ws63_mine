/**
 * @file zw101.h
 * @brief ZW101 指纹模组驱动。
 */
#ifndef ZW101_H
#define ZW101_H

#include "errcode.h"
#include <stdint.h>

/**
 * @brief 初始化ZW101指纹模块并进行通信(握手)测试
 *
 * @param sub_port 对应的串口号
 * @return errcode_t 成功返回ERRCODE_SUCC，否则返回失败代码
 */
errcode_t zw101_init(uint8_t sub_port);

/**
 * @brief 处理ZW101指纹模块接收到的数据
 *
 * @param sub_port 对应串口号
 * @param data 接收到的数据缓冲区
 * @param len 接收到的数据长度
 */
void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

#endif
