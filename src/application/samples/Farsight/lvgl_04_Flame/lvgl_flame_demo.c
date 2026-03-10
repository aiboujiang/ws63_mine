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

#include "lvgl_flame_demo.h"

extern lv_disp_drv_t disp_drv;
extern lv_obj_t *beepswitch;
extern lv_obj_t *buzzerstateLabel;
extern lv_obj_t *FlamestateLabel;
extern lv_obj_t *modeswitch;
osThreadId_t task1_ID; // 任务1 ID

#define RELOAD_CNT 1000  // 定时器重载值，单位：微秒
#define TIMER_IRQ_PRIO 3 // 定时器中断优先级，范围：0~7（0最高）

static uint16_t msLVGL;      // LVGL刷新计时器
static uint16_t UpdateTimer; // 数据更新计时器

timer_handle_t timer1_handle = 0; // 定时器1句柄

Mode_Select My_Mode;
Sensor_Data My_Sensor;	

static void TimerUpdateData(void); // 定时更新数据函数声明

/**
 * @brief 定时器1回调函数
 * @param data 用户数据（未使用）
 * @note 该函数每1ms执行一次，用于LVGL心跳和数据更新计时
 */
void timer1_callback(uintptr_t data)
{
    unused(data);
    lv_tick_inc(1);     // LVGL心跳递增
    msLVGL++;           // LVGL处理标志递增
    UpdateTimer++;      // 传感器数据处理标志递增
    
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

void user_beep_init(void)
{
    uapi_pin_set_mode(USER_BEEP, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(USER_BEEP, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
}

/**
 * @brief 任务1主函数
 * @param arg 任务参数（未使用）
 * @return 任务执行状态
 * @note 主要功能：初始化硬件、LVGL、UI，并处理主循环任务
 */
static int task1(const char *arg)
{
    unused(arg);
    uapi_watchdog_disable(); // 禁用看门狗

    // LVGL初始化
    lv_init();                    // 初始化LVGL库
    lv_port_disp_init();          // 初始化LVGL显示端口
    lv_port_indev_init();         // 初始化LVGL输入设备端口
    UI_Init();                    // 初始化用户界面
    
    // 硬件初始化
    AP3216C_Init();
    user_beep_init();

    lvgl_timer_init(); // 初始化LVGL定时器
    
    // 主循环
    while (1)
    {
        // 每5ms执行一次LVGL任务处理
        if(msLVGL++ >= 5)
        {
            lv_timer_handler(); // 处理LVGL定时器任务
            msLVGL = 0;         // 重置计时器
        }
        
        // 每1000ms执行一次数据更新
        if(UpdateTimer >= 1000)
        {
            TimerUpdateData(); // 更新传感器数据和UI状态
            UpdateTimer = 0;   // 重置计时器
        }
        
        osDelay(1); // 延时1ms
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
    attr.name = "Task1";           // 任务名称
    attr.attr_bits = 0U;           // 任务属性位
    attr.cb_mem = NULL;            // 任务控制块内存
    attr.cb_size = 0U;             // 任务控制块大小
    attr.stack_mem = NULL;         // 任务堆栈内存
    attr.stack_size = 0x1000;      // 任务堆栈大小（4KB）
    attr.priority = osPriorityNormal; // 任务优先级

    // 创建任务1
    task1_ID = osThreadNew((osThreadFunc_t)task1, NULL, &attr);
    if (task1_ID != NULL)
    {
        printf("ID = %d, Create Task1_ID is OK!\r\n", task1_ID);
    }
}

/* 运行示例应用 */
app_run(lvgl_entry);


/**
 * @brief 定时更新数据函数
 * @note 每1秒执行一次，更新传感器数据、UI状态并处理智能控制逻辑
 */
static void TimerUpdateData(void)
{
   
    AP3216C_ReadData(&My_Sensor.flame.ir, &My_Sensor.flame.als, &My_Sensor.flame.ps);
    //printf("ir = %d    als = %d    ps = %d\r\n", My_Sensor.flame.ir, My_Sensor.flame.als, My_Sensor.flame.ps);
    //使用接近传感器的接近属性当作火焰传感器
    if (My_Sensor.flame.ps > 500) My_Sensor.flame_io = 1;//传感器数值大于500视为触发火焰传感器
    else My_Sensor.flame_io = 0;

    if (My_Sensor.flame_io)
    {
          lv_label_set_text(FlamestateLabel, "Flame:ON");
    }
    else
    {
        lv_label_set_text(FlamestateLabel, "Flame:OFF");
    }
    if (My_Mode.CurrentMode == SMART) {
        lv_obj_add_state(modeswitch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(modeswitch, LV_STATE_CHECKED);
    }
    /* 智能控制模式处理 */
    if (My_Mode.CurrentMode == SMART)
    {
       if (My_Sensor.flame_io)
        {
            My_Sensor.beep_state = BEEPON;
            lv_label_set_text(buzzerstateLabel, "BEEP:ON");
            uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH);
            lv_obj_add_state(beepswitch, LV_STATE_CHECKED);
        }
        else
        {
            My_Sensor.beep_state = BEEPOFF;
			lv_label_set_text(buzzerstateLabel, "BEEP:OFF");
            uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
            lv_obj_clear_state(beepswitch, LV_STATE_CHECKED);
        }
    }
  
    /* MQTT通信处理 */
    if (1 == My_Mode.WeChatOnline)
    {
        /* 模式改变时发送模式状态 */
        if (My_Mode.CurrentMode != My_Mode.last_CurrentMode)
        {
            if(My_Mode.CurrentMode == USR)
            {
                send_Bool_data("smart", 0, "false", "false"); // 发送手动模式
            }
            else
            {
                send_Bool_data("smart", 0, "false", "true"); // 发送智能模式
            }
            My_Mode.last_CurrentMode =My_Mode.CurrentMode ;
        }
        osDelay(10);
        if (My_Sensor.beep_state != My_Sensor.beep_laststate)
        {
            if (My_Sensor.beep_state)
            { 
                My_Sensor.beep_state = BEEPON;
                lv_label_set_text(buzzerstateLabel, "BEEP:ON");
                uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH);
                lv_obj_add_state(beepswitch, LV_STATE_CHECKED);
                send_Bool_data("beep", 0, "true", "true");
            }
            else
            {
                My_Sensor.beep_state = BEEPOFF;
                lv_label_set_text(buzzerstateLabel, "BEEP:OFF");
                uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
                lv_obj_clear_state(beepswitch, LV_STATE_CHECKED);
                send_Bool_data("beep", 0, "true", "false");
            }
            My_Sensor.beep_laststate = My_Sensor.beep_state;
        }
        osDelay(10);
        if (My_Sensor.flame_io != My_Sensor.flame_io_laststate)
        {
            if (My_Sensor.flame_io)
            {
                send_Bool_data("flame", 0, "false", "true");
            }
            else
            {
                send_Bool_data("flame", 0, "false", "false");
            }
            My_Sensor.flame_io_laststate = My_Sensor.flame_io;
        }
    }
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
