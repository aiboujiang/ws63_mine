/*
 * Copyright (c) 2024 Beijing HuaQingYuanJian Education Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hal_bsp_aw2013.h"
#include "stdio.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "securec.h"


#define DELAY_TIME_MS 1
/**
 * @brief AW2013 I2C单字节写入函数
 * @note 该函数用于向AW2013芯片的指定寄存器写入一个字节的数据
 *       是AW2013驱动的最底层通信函数
 * 
 * @param regAddr 要写入的目标寄存器地址
 * @param byte 要写入的数据字节（0x00-0xFF）
 * 
 * @return uint32_t 错误代码
 *         - ERRCODE_SUCC: 写入成功
 *         - 其他: I2C通信失败，具体错误码参考I2C驱动文档
 */
static uint32_t aw2013_WriteByte(uint8_t regAddr, uint8_t byte)
{
    uint32_t result = 0;  // 存储I2C操作结果
    
    // 构造I2C发送数据缓冲区：寄存器地址 + 数据字节
    uint8_t buffer[] = {regAddr, byte};
    
    // 初始化I2C数据传输结构体
    i2c_data_t i2c_data = {0};
    i2c_data.send_buf = buffer;       // 设置发送缓冲区指针
    i2c_data.send_len = sizeof(buffer); // 设置发送数据长度（2字节）
    
    // 调用I2C主设备写操作API
    // 参数：I2C控制器索引、AW2013设备地址、数据传输结构体
    result = uapi_i2c_master_write(AW2013_I2C_IDX,      // I2C控制器编号（如0,1,2...）
                                  AW2013_I2C_ADDR,     // AW2013的I2C设备地址（通常0x45）
                                  &i2c_data);          // I2C数据发送结构体
    
    // 检查I2C传输结果
    if (result != ERRCODE_SUCC) {
        // I2C写入失败，打印错误信息以便调试
        printf("I2C AW2013 Write result is 0x%x!!!\r\n", result);
        return result;  // 返回错误码
    }
    
    return result;  // 返回操作结果（成功时为ERRCODE_SUCC）
}

// 寄存器PWMx控制RGB灯的红色亮度
uint32_t AW2013_Control_Red(uint8_t PWM_Data)
{
    uint32_t result = 0;
    result = aw2013_WriteByte(PWM0_REG_ADDR, PWM_Data); // R
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 Red PWM1_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }
    return result;
}
// 寄存器PWMx控制RGB灯的绿色亮度
uint32_t AW2013_Control_Green(uint8_t PWM_Data)
{
    uint32_t result = 0;
    result = aw2013_WriteByte(PWM1_REG_ADDR, PWM_Data); // G
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 Green PWM0_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }
    return result;
}
// 寄存器PWMx控制RGB灯的蓝色亮度
uint32_t AW2013_Control_Blue(uint8_t PWM_Data)
{
    uint32_t result = 0;
    result = aw2013_WriteByte(PWM2_REG_ADDR, PWM_Data); // B
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 Blue PWM2_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }
    return result;
}
/**
 * @brief 控制AW2013芯片的RGB三色LED亮度
 * @note 通过I2C接口分别设置红、绿、蓝三个通道的PWM寄存器值
 * 
 * @param R_Value 红色通道亮度值 (0-255)
 *                - 0:   完全熄灭
 *                - 255: 最大亮度
 * @param G_Value 绿色通道亮度值 (0-255)
 * @param B_Value 蓝色通道亮度值 (0-255)
 * 
 * @return uint32_t 错误代码
 *         - ERRCODE_SUCC: 操作成功
 *         - 其他: I2C通信失败，具体错误码参考I2C驱动
 */
uint32_t AW2013_Control_RGB(uint8_t R_Value, uint8_t G_Value, uint8_t B_Value)
{
    uint32_t result = 0;  // 存储操作结果

    // 设置红色通道PWM亮度值
    result = aw2013_WriteByte(PWM0_REG_ADDR, R_Value); // PWM0寄存器控制红色LED
    if (result != ERRCODE_SUCC) {
        // I2C写入失败，打印错误信息（注意：日志中颜色标注有误）
        printf("I2C aw2013 Green PWM0_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;  // 立即返回错误码，不再继续后续操作
    }

    // 设置绿色通道PWM亮度值
    result = aw2013_WriteByte(PWM1_REG_ADDR, G_Value); // PWM1寄存器控制绿色LED
    if (result != ERRCODE_SUCC) {
        // I2C写入失败，打印错误信息（注意：日志中颜色标注有误）
        printf("I2C aw2013 Red PWM1_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;  // 立即返回错误码
    }

    // 设置蓝色通道PWM亮度值
    result = aw2013_WriteByte(PWM2_REG_ADDR, B_Value); // PWM2寄存器控制蓝色LED
    if (result != ERRCODE_SUCC) {
        // I2C写入失败，打印错误信息
        printf("I2C aw2013 Blue PWM2_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;  // 立即返回错误码
    }
    
    return result;  // 返回最终操作结果（成功时为ERRCODE_SUCC）
}
uint32_t AW2013_Init(void)
{
    uint32_t result;
    // 复位芯片
    result = aw2013_WriteByte(RSTR_REG_ADDR, 0x55);
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 RSTR_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }

    // 使能全局控制器 设置为RUN模式
    result = aw2013_WriteByte(GCR_REG_ADDR, 0x01);
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 GCR_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }

    // 设置打开RGB三路通道
    result = aw2013_WriteByte(LCTR_REG_ADDR, 0x07); // 4: B, 2: G, 1: R
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 LCTR_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }

    // 设置RGB三路通道的工作模式
    result = aw2013_WriteByte(LCFG0_REG_ADDR, 0x63);
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 LCFG0_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }
    result = aw2013_WriteByte(LCFG1_REG_ADDR, 0x63);
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 LCFG1_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }
    result = aw2013_WriteByte(LCFG2_REG_ADDR, 0x63);
    if (result != ERRCODE_SUCC) {
        printf("I2C aw2013 LCFG2_REG_ADDR status = 0x%x!!!\r\n", result);
        return result;
    }
    printf("I2C aw2013 Init is succeeded!!!\r\n");
    return ERRCODE_SUCC;
}
