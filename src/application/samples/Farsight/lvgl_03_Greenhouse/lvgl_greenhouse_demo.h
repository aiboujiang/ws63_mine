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

#ifndef LVGL_DEMO_H
#define LVGL_DEMO_H

#include "soc_osal.h"           // 操作系统抽象层
#include "app_init.h"           // 应用初始化
#include "cmsis_os2.h"          // CMSIS-RTOS2 API
#include "platform_core_rom.h"  // 平台核心ROM
#include "watchdog.h"           // 看门狗
#include "timer.h"              // 定时器
#include "chip_core_irq.h"      // 芯片核心中断
#include <stdio.h>              // 标准输入输出
#include "bsp/hal_bsp_sht20.h" // AP3216C传感器驱动（光敏/接近/红外）

#include "bsp/ui.h"             // 用户界面
#include "bsp/wifi_connect.h"   // WiFi连接
#include "bsp/mqtt.h"           // MQTT客户端
#include "lvgl.h"               // LVGL图形库主头文件
#include "pwm.h"
//温湿度传感器数据结构
typedef struct
{
    float Tem;
    float Hum;
    uint8_t FanCurrentState;
    int FanSpeed;
}Sensor_Data; 
extern Sensor_Data My_Sensor;	//存储温湿度值
/**
 * @brief 模式选择管理结构体
 * @note 管理系统工作模式和网络连接状态
 */
typedef struct {
    uint8_t CurrentMode;     /**< 当前工作模式 */
    uint8_t last_CurrentMode;/**< 上一次工作模式，用于模式变化检测 */
    uint8_t WeChatOnline;    /**< 微信在线状态标志 */
    uint8_t mqttconnetState; /**< MQTT连接状态标志 */
} Mode_Select;

extern Mode_Select My_Mode; /**< 全局模式选择实例 */


/**
 * @brief 发送温湿度数据到MQTT
 * @param Sensor 传感器类型标识
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 字符串数值
 */
void send_Hum_or_Tmpdata_protocol(const char *Sensor, const int ID, const char *Control, const float val);

/**
 * @brief 发送字符串数据到MQTT
 * @param Sensor 传感器类型标识
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 字符串数值
 */
void send_String_data(const char *Sensor, const int ID, const char *Control, const char *val);

/**
 * @brief 发送布尔数据到MQTT
 * @param Sensor 传感器类型标识
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 布尔值
 */
void send_Bool_data(const char *Sensor, const int ID, const char *Control, const char *val);

/**
 * @brief 发送数值数据到MQTT
 * @param Sensor 传感器类型标识
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 数值数据
 */
void send_Num_data(const char *Sensor, const int ID, const char *Control, const short val);

/**
 * @brief 发送重复确认消息到MQTT
 * @param val 需要确认的传感器类型
 * @note 用于响应平台命令，确认接收
 */
void send_repeat(const char *val);

void user_pwm_change(uint8_t fre);
#endif /* LVGL_DEMO_H */