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

#ifndef HAL_BSP_AW2013_H
#define HAL_BSP_AW2013_H

#include "cmsis_os2.h"

#define AW2013_I2C_ADDR 0x45    // 器件的I2C从机地址
#define AW2013_I2C_IDX 1        // 模块的I2C总线号
#define AW2013_I2C_SPEED 100000 // 100KHz
#define I2C_MASTER_ADDR 0x0

#define RSTR_REG_ADDR 0x00
#define GCR_REG_ADDR 0x01
#define ISR_REG_ADDR 0x02
#define LCTR_REG_ADDR 0x30
#define LCFG0_REG_ADDR 0x31
#define LCFG1_REG_ADDR 0x32
#define LCFG2_REG_ADDR 0x33
#define PWM0_REG_ADDR 0x34
#define PWM1_REG_ADDR 0x35
#define PWM2_REG_ADDR 0x36
#define LED0T0_REG_ADDR 0x37
#define LED0T1_REG_ADDR 0x38
#define LED0T2_REG_ADDR 0x39
#define LED1T0_REG_ADDR 0x3A
#define LED1T1_REG_ADDR 0x3B
#define LED1T2_REG_ADDR 0x3C
#define LED2T0_REG_ADDR 0x3D
#define LED2T1_REG_ADDR 0x3E
#define LED2T2_REG_ADDR 0x3F
#define IADR_REG_ADDR 0x77
/**
 * @brief AW2013 控制RGB灯的红灯亮度
 * @param PWM_Data PWM的控制输出值 0~255
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t AW2013_Control_Red(uint8_t PWM_Data);
/**
 * @brief AW2013 控制RGB灯的绿灯亮度
 * @param PWM_Data PWM的控制输出值 0~255
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t AW2013_Control_Green(uint8_t PWM_Data);
/**
 * @brief AW2013 控制RGB灯的蓝灯亮度
 * @param PWM_Data PWM的控制输出值 0~255
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t AW2013_Control_Blue(uint8_t PWM_Data);
/**
 * @brief AW2013 控制RGB灯的三色灯参数
 * @param R_Value PWM的控制输出值 0~255
 * @param G_Value PWM的控制输出值 0~255
 * @param B_Value PWM的控制输出值 0~255
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t AW2013_Control_RGB(uint8_t R_Value, uint8_t G_Value, uint8_t B_Value);
/**
 * @brief AW2013 器件的初始化
 * @param 无
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t AW2013_Init(void);

#endif // !HAL_BSP_AW2013_H
