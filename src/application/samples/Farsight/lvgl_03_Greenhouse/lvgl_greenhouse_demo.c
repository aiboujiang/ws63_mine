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

#include "lvgl_greenhouse_demo.h"
extern  lv_obj_t *Hum_arc;
extern  lv_obj_t *Tmp_arc;
extern lv_obj_t *Tem_label;
extern lv_obj_t *Hum_label;
extern lv_obj_t *Msgbox;
/* Local control panel */
extern   lv_obj_t *FanOnOff;
extern  lv_obj_t *FanSpeed_1;
extern lv_obj_t *FanSpeed_2;
extern   lv_obj_t *FanSpeed_3;
extern  lv_obj_t *FanSpeedLabel;
extern lv_obj_t *CurrentSpreedBtn; /* Records the key that is currently pressed */
extern  lv_obj_t *MqttConnectState;
extern lv_obj_t *MqttConnectBtn;
extern lv_obj_t *modeswitch;
osThreadId_t task1_ID; // 任务1 ID
osThreadId_t task2_ID; // 任务2 ID
osThreadId_t task3_ID; // 任务3 ID

#define RELOAD_CNT 1000  // 定时器重载值，单位：微秒
#define TIMER_IRQ_PRIO 3 // 定时器中断优先级，范围：0~7（0最高）

timer_handle_t timer1_handle = 0; // 定时器1句柄

Mode_Select My_Mode;
Sensor_Data My_Sensor;	//存储温湿度值
struct Threshold_t Getthreshold = {30, 50};

lv_obj_t *Fanbtn;

static void sensor_data_update_handler(void); // 定时更新数据函数声明
static void FanSpeedCtl(uint8_t FanState);
void fan_pwm_init(void);
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
 * @brief 传感器数据采集任务
 */
static int sensor_collect(const char *arg)
{
    unused(arg);

    while (1)
    {
        sensor_data_update_handler(); // 更新传感器数据和UI状态
        osal_mdelay(500); 
    }
    return 0;
}
/**
 * @brief 任务1主函数
 * @param arg 任务参数（未使用）
 * @return 任务执行状态
 * @note 主要功能：初始化硬件、LVGL、UI，并处理主循环任务
 */
static int lvgl_refresh(const char *arg)
{
    unused(arg);
    uint16_t msLVGL;   // LVGL刷新计时器
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
    SHT20_Init();
    fan_pwm_init();

    osThreadAttr_t attr;
    attr.name = "lvgl_refresh";    // 任务名称
    attr.attr_bits = 0U;           // 任务属性位
    attr.cb_mem = NULL;            // 任务控制块内存
    attr.cb_size = 0U;             // 任务控制块大小
    attr.stack_mem = NULL;         // 任务堆栈内存
    attr.stack_size = 0x1000;      // 任务堆栈大小（4KB）
    attr.priority = osPriorityNormal; // 任务优先级
    // 创建任务
    task2_ID = osThreadNew((osThreadFunc_t)lvgl_refresh, NULL, &attr);
    if (task1_ID != NULL)
    {
        printf("ID = %d, Create task2_ID is OK!\r\n", task2_ID);
    }
    attr.name = "sensor_collect";    // 任务名称
    // 创建任务
    task3_ID = osThreadNew((osThreadFunc_t)sensor_collect, NULL, &attr);
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


/**
 * @brief 定时更新数据函数
 * @note 每1秒执行一次，更新传感器数据、UI状态并处理智能控制逻辑
 */
static void sensor_data_update_handler(void)
{
    char textbuff[20] = {0};
    
    SHT20_ReadData(&My_Sensor.Tem,&My_Sensor.Hum);
    lv_arc_set_value(Tmp_arc, My_Sensor.Tem);
    sprintf(textbuff, "%.1f", My_Sensor.Tem);
    lv_label_set_text_fmt(Tem_label, "%sRH", textbuff);

    sprintf(textbuff, "%.1f", My_Sensor.Hum);
    lv_arc_set_value(Hum_arc, My_Sensor.Hum);
    lv_label_set_text_fmt(Hum_label, "%sRH", textbuff);

    /* 智能控制模式处理 */
    if (My_Mode.CurrentMode == SMART)
    {
        if((My_Sensor.Hum>Getthreshold.Humid )||( My_Sensor.Tem>Getthreshold.Temp))
        {
            My_Sensor.FanCurrentState = FANSPREED3;
        }
        else if((My_Sensor.Hum<Getthreshold.Humid )&&( My_Sensor.Tem<Getthreshold.Temp))
        {
            My_Sensor.FanCurrentState = FANSPREEDOFF;
        }
        FanSpeedCtl(My_Sensor.FanCurrentState);
    }
    /* MQTT通信处理 */
    if (1 == My_Mode.WeChatOnline)
    {
        FanSpeedCtl(My_Sensor.FanCurrentState);
        osDelay(10);
        /* WeChatOnline */
        send_Hum_or_Tmpdata_protocol("temperature", 0, "false", My_Sensor.Tem);
        osDelay(10);
        send_Hum_or_Tmpdata_protocol("humidity", 0, "false", My_Sensor.Hum);
        osDelay(10);
        
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
    }
       if (My_Mode.CurrentMode == SMART) {
        lv_obj_add_state(modeswitch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(modeswitch, LV_STATE_CHECKED);
    }
 
}

void send_Hum_or_Tmpdata_protocol(const char *Sensor, const int ID, const char *Control, const float val)
{
    char json[300] = {0};
    sprintf(json, "{\"sensor\":\"%s\",\"id\":%d,\"control\":%s,\"data_val\":%.2f}", Sensor, ID, Control, val);
    mqtt_publish(MQTT_DATATOPIC_PUB, json); // 发布到MQTT主题
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

static void FanSpeedCtl(uint8_t FanState)
{
    if (FanState >3)
        return;
    if (My_Sensor.FanSpeed != FanState)
    {
        My_Sensor.FanSpeed = FanState;
        switch (FanState)
        {
            case FANSPREED1:
                Fanbtn = FanSpeed_1;
                break;
            case FANSPREED2:
                Fanbtn = FanSpeed_2;
                break;
            case FANSPREED3:
                Fanbtn = FanSpeed_3;
                break;
            case FANSPREEDOFF:
                Fanbtn = FanOnOff;
                break;
            }
       if (0 ==  FanState )
        {
			lv_label_set_text(FanSpeedLabel,  "FAN:OFF");
            user_pwm_change(FanState);
        }
        else
        {
            user_pwm_change(FanState);
            lv_label_set_text_fmt(FanSpeedLabel, "LEVEL:%d", FanState); 
        }

        if (CurrentSpreedBtn != Fanbtn)
        {
            lv_obj_set_style_bg_color(CurrentSpreedBtn, lv_color_make(10, 38, 116), LV_PART_MAIN);
            lv_obj_set_style_bg_color(Fanbtn, lv_color_make(47, 174, 108), LV_PART_MAIN);
            // if (USR == My_Mode.CurrentMode )
            // {
            lv_obj_clear_state(CurrentSpreedBtn, LV_STATE_DISABLED);
            lv_obj_add_flag(CurrentSpreedBtn, LV_OBJ_FLAG_CLICKABLE);
            // }
        }
        CurrentSpreedBtn = Fanbtn;
          if (1 == My_Mode.WeChatOnline)
            send_Num_data("fan", 0, "true",FanState);
    }
}
#define GPIO_FAN GPIO_03
#define PWM_CHANNEL3 3

pwm_config_t Fan_PWM_level[4] = {
    {7900, 100, 0, 0, true},      //关
    {5500, 2500, 0, 0, true},     //一档
    {4500, 4500, 0, 0, true},     //二档
    {100, 7900, 0, 0, true},      //三档
};
// pwm引脚初始化
void fan_pwm_init(void)
{
    uapi_pin_set_mode(GPIO_FAN, PIN_MODE_1);

    uapi_pwm_init();
    uapi_pwm_open(PWM_CHANNEL3, &Fan_PWM_level[0]);

    uint8_t channel_id = PWM_CHANNEL3;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL3);

}

void user_pwm_change(uint8_t fre)
{
    uapi_pwm_init();
    uapi_pwm_open(PWM_CHANNEL3, &Fan_PWM_level[fre]);

    uint8_t channel_id = PWM_CHANNEL3;
    uapi_pwm_set_group(1, &channel_id, 1);
    uapi_pwm_start(PWM_CHANNEL3);
}