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
#include "bsp/hal_bsp_ap3216.h" // AP3216C传感器驱动（光敏/接近/红外）
#include "bsp/hal_bsp_aw2013.h" // AW2013 RGB LED驱动
#include "bsp/ui.h"             // 用户界面
#include "bsp/wifi_connect.h"   // WiFi连接
#include "bsp/mqtt.h"           // MQTT客户端
#include "lvgl.h"               // LVGL图形库主头文件
// 声明外部UI对象
extern lv_obj_t *lightimg;                 // 灯光图像对象
extern lv_obj_t *lightswitch;              // 灯光开关对象
extern lv_obj_t *lightstateLabel;          // 灯光状态标签
extern lv_obj_t *photosensitivestateLabel; // 光敏状态标签
extern lv_obj_t *modeswitch;               // 模式切换开关
extern lv_obj_t *Msgbox;           // 消息框对象
extern lv_obj_t *MqttConnectState; // MQTT连接状态对象
extern lv_obj_t *MqttConnectBtn;   // MQTT连接按钮对象
// 声明外部图像资源
LV_IMG_DECLARE(lighton)   // 灯光开启图标
LV_IMG_DECLARE(lightoff)  // 灯光关闭图标
/**
 * @brief AP3216C传感器数据结构
 * @note 包含红外、环境光、接近传感器数据
 */
typedef struct {
    uint16_t ir;  /**< 红外传感器数据 */
    uint16_t als; /**< 环境光传感器数据（光照强度） */
    uint16_t ps;  /**< 接近传感器数据 */
} AP3216;

/**
 * @brief 传感器数据管理结构体
 * @note 包含所有传感器数据和设备状态
 */
typedef struct {
    AP3216 Lux_val;     /**< AP3216C传感器数据 */
    char led_state;     /**< 当前LED状态 */
    char led_cmd_flag;  /**< LED命令标志位，用于平台控制响应 */
    char led_laststate; /**< 上一次LED状态，用于状态变化检测 */
} Sensor_Data;

extern Sensor_Data My_Sensor; /**< 全局传感器数据实例 */

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

/* MQTT数据发送函数声明 */

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

#endif /* LVGL_DEMO_H */