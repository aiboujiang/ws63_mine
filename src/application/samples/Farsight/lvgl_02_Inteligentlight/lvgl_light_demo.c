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

#include "lvgl_light_demo.h"

osThreadId_t task1_ID; // 任务1 ID
osThreadId_t task2_ID; // 任务2 ID
osThreadId_t task3_ID; // 任务3 ID

Sensor_Data My_Sensor;  // 传感器数据结构体，用于存储传感器数据
Mode_Select My_Mode;    // 模式选择结构体，用于存储当前工作模式

#define RELOAD_CNT 1000  // 定时器重载值，单位：微秒
#define TIMER_IRQ_PRIO 3 // 定时器中断优先级，范围：0~7（0最高）
timer_handle_t timer1_handle = 0; // 定时器1句柄

static void sensor_data_update_handler(void);
static void smart_light_control_handler(void);
static void light_status_update_handler(void);
static void mqtt_communication_handler(void);
/**
 * @brief 定时器1回调函数
 * @param data 用户数据（未使用）
 * @note 该函数每1ms执行一次，用于LVGL心跳和数据更新计时
 */
void timer1_callback(uintptr_t data)
{
    unused(data);
    lv_tick_inc(1);     // LVGL心跳递增
    
    /* 开启下一次timer中断 */
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}

/**
 * @brief LVGL定时器初始化
 * @note 初始化硬件定时器1，设置中断优先级并启动定时器
 */
void lvgl_timer_init(void)
{
    uapi_timer_init(); // 初始化定时器模块
    
    /* 设置 timer1 硬件初始化，设置中断号，配置优先级 */
    uapi_timer_adapter(TIMER_INDEX_1, TIMER_1_IRQN, TIMER_IRQ_PRIO);
    
    /* 创建 timer1 软件定时器控制句柄 */
    uapi_timer_create(TIMER_INDEX_1, &timer1_handle);
    
    /* 启动定时器，设置重载值和回调函数 */
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}

/**
 * @brief lvgl刷新任务
 */
static int lvgl_refresh_task(const char *arg)
{
    unused(arg);

    uint16_t msLVGL;      // LVGL刷新计时器
    lvgl_timer_init(); // 初始化LVGL定时器
    // 主循环
    while (1)
    {
        msLVGL = lv_timer_handler(); // 处理LVGL定时器任务
        osal_mdelay(msLVGL); 
    }
    return 0;
}
/**
 * @brief 传感器数据采集任务
 */
static int sensor_collect_task(const char *arg)
{
    unused(arg);

    while (1)
    {
        sensor_data_update_handler(); // 更新传感器数据和UI状态
        osal_mdelay(100); // 延时1ms
    }
    return 0;
}
/**
 * @brief 硬件初始化任务
 * @param arg 任务参数（未使用）
 * @note 主要功能：初始化硬件、LVGL、UI
 */
static int bsp_init(const char *arg)
{
    unused(arg);

    uapi_watchdog_disable(); // 禁用看门狗

    // LVGL初始化
    lv_init();                    // 初始化LVGL库
    lv_port_disp_init();          // 初始化LVGL显示端口
    lv_port_indev_init();         // 初始化LVGL输入设备端口
    UI_Init();                    // 初始化用户界面
    
    // 硬件初始化
    AP3216C_Init();  // 初始化光线传感器
    AW2013_Init();   // 初始化RGB LED控制器
    AW2013_Control_RGB(0,0,0); // 关闭LED
    
    osThreadAttr_t attr;
    attr.name = "lvgl_refresh_task";    // 任务名称
    attr.attr_bits = 0U;           // 任务属性位
    attr.cb_mem = NULL;            // 任务控制块内存
    attr.cb_size = 0U;             // 任务控制块大小
    attr.stack_mem = NULL;         // 任务堆栈内存
    attr.stack_size = 0x1000;      // 任务堆栈大小（4KB）
    attr.priority = osPriorityNormal; // 任务优先级
    // 创建任务
    task2_ID = osThreadNew((osThreadFunc_t)lvgl_refresh_task, NULL, &attr);
    if (task1_ID != NULL)
    {
        printf("ID = %d, Create task2_ID is OK!\r\n", task2_ID);
    }
    attr.name = "sensor_collect_task";    // 任务名称
    // 创建任务
    task3_ID = osThreadNew((osThreadFunc_t)sensor_collect_task, NULL, &attr);
    if (task1_ID != NULL)
    {
        printf("ID = %d, Create task3_ID is OK!\r\n", task3_ID);
    }

    return 0;
}
/**
 * @brief LVGL任务入口函数
 * @note 创建任务1并启动LVGL应用
 */
static void lvgl_entry(void)
{
    printf("Enter lvgl_entry()!\r\n");
    
    osThreadAttr_t attr;
    attr.name = "bsp_init";           // 任务名称
    attr.attr_bits = 0U;           // 任务属性位
    attr.cb_mem = NULL;            // 任务控制块内存
    attr.cb_size = 0U;             // 任务控制块大小
    attr.stack_mem = NULL;         // 任务堆栈内存
    attr.stack_size = 0x1000;      // 任务堆栈大小（4KB）
    attr.priority = osPriorityNormal; // 任务优先级

    // 创建任务
    task1_ID = osThreadNew((osThreadFunc_t)bsp_init, NULL, &attr);
    if (task1_ID != NULL)
    {
        printf("ID = %d, Create Task1_ID is OK!\r\n", task1_ID);
    }   
   
}

/* 运行示例应用 */
app_run(lvgl_entry);

uint16_t lightofflevel = 500; // 灯光关闭的光照阈值（Lux）

/**
 * @brief 定时更新数据函数
 * @note 每1秒执行一次，更新传感器数据、UI状态并处理智能控制逻辑
 */
static void sensor_data_update_handler(void)
{
    /* 读取传感器数据 */
    AP3216C_ReadData(&My_Sensor.Lux_val.ir, &My_Sensor.Lux_val.als, &My_Sensor.Lux_val.ps);
    /* 更新光敏传感器显示 */
    lv_label_set_text_fmt(photosensitivestateLabel, "Light:%dLux", My_Sensor.Lux_val.als);
   
    /* 智能控制逻辑 */
    smart_light_control_handler();
    
    /* 灯光状态更新 */
    light_status_update_handler();
    
    /* MQTT通信处理 */
    mqtt_communication_handler();
}
/**
 * @brief 智能灯光控制处理
 */
static void smart_light_control_handler(void)
{
    if (My_Mode.CurrentMode != SMART) {
        return;
    }

    if (My_Sensor.Lux_val.als >= lightofflevel) {
        My_Sensor.led_state = LIGHTOFF;
    } else if (My_Sensor.Lux_val.als <= lightofflevel) {
        My_Sensor.led_state = LIGHTON;
    }
}

/**
 * @brief 灯光状态更新处理
 */
static void light_status_update_handler(void)
{
    if (My_Sensor.led_state == LIGHTON) {
        lv_img_set_src(lightimg, &lighton);
        lv_label_set_text(lightstateLabel, "Switch:ON");
        AW2013_Control_RGB(0xff, 0xff, 0xff);
        lv_obj_add_state(lightswitch, LV_STATE_CHECKED);
    } else {
        lv_img_set_src(lightimg, &lightoff);
        lv_label_set_text(lightstateLabel, "Switch:OFF");
        AW2013_Control_RGB(0, 0, 0);
        lv_obj_clear_state(lightswitch, LV_STATE_CHECKED);
    }
    if (My_Mode.CurrentMode == SMART) {
        lv_obj_add_state(modeswitch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(modeswitch, LV_STATE_CHECKED);
    }
}

/**
 * @brief MQTT通信处理
 */
static void mqtt_communication_handler(void)
{
    if (My_Mode.WeChatOnline != 1) {
        return;
    }

    /* 灯光状态变化通知 */
    if (My_Sensor.led_state != My_Sensor.led_laststate) {
        const char *state_msg = (My_Sensor.led_state == LIGHTON) ? "LEDON" : "LEDOFF";
        send_String_data("light", 0, "true", state_msg);
        My_Sensor.led_laststate = My_Sensor.led_state;
    }
    
    /* 模式状态通知 */
    if (My_Mode.CurrentMode != My_Mode.last_CurrentMode) {
        const char *mode_val = (My_Mode.CurrentMode == USR) ? "false" : "true";
        send_Bool_data("smart", 0, "false", mode_val);
        My_Mode.last_CurrentMode = My_Mode.CurrentMode;
    }
    
    /* 传感器数据上报 */
    send_Num_data("photosensitive", 0, "false", My_Sensor.Lux_val.als);
    osDelay(20);
}
/**
 * @brief 发送字符串数据到MQTT
 * @param Sensor 传感器类型
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 字符串数值
 */
void send_String_data(const char *Sensor, const int ID, const char *Control, const char *val)
{
    char json[300] = {0};
    sprintf(json, "{\"sensor\":\"%s\",\"id\":%d,\"control\":%s,\"data_val\":\"%s\"}", 
            Sensor, ID, Control, val);
    mqtt_publish(MQTT_DATATOPIC_PUB, json); // 发布到MQTT主题
}

/**
 * @brief 发送数值数据到MQTT
 * @param Sensor 传感器类型
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 数值数据
 */
void send_Num_data(const char *Sensor, const int ID, const char *Control, const short val)
{
    char json[300] = {0};
    sprintf(json, "{\"sensor\":\"%s\",\"id\":%d,\"control\":%s,\"data_val\":%d}", 
            Sensor, ID, Control, val);
    mqtt_publish(MQTT_DATATOPIC_PUB, json); // 发布到MQTT主题
}

/**
 * @brief 发送布尔数据到MQTT
 * @param Sensor 传感器类型
 * @param ID 传感器ID
 * @param Control 控制标志
 * @param val 布尔值
 */
void send_Bool_data(const char *Sensor, const int ID, const char *Control, const char *val)
{
    char json[300] = {0};
    sprintf(json, "{\"sensor\":\"%s\",\"id\":%d,\"control\":%s,\"data_val\":%s}", 
            Sensor, ID, Control, val);
    mqtt_publish(MQTT_DATATOPIC_PUB, json); // 发布到MQTT主题
}

/**
 * @brief 发送重复确认消息到MQTT
 * @param val 确认的传感器类型
 */
void send_repeat(const char *val)
{
    char json[100] = {0};
    sprintf(json, "{\"repeat\":\"%s\"}", val);
    mqtt_publish(MQTT_DATATOPIC_PUB, json); // 发布到MQTT主题
}