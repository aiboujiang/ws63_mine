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

#endif
