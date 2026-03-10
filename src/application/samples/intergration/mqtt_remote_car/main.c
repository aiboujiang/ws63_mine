/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
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

#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"

#include "app_init.h"
#include "osal_task.h"
#include "watchdog.h"

#include "bsp/bsp_ina219.h"
#include "bsp/bsp_init.h"
#include "bsp/hal_bsp_ssd1306.h"
#include "bsp/hal_bsp_dc.h"
#include "bsp/track.h"
#include "bsp/pid.h"

#include "bsp/hal_bsp_nfc.h"
#include "bsp/hal_bsp_nfc_to_wifi.h"
#include "bsp/wifi_connect.h"
#include "bsp/mqtt.h"
#include "bsp/hal_bsp_sht20.h"

char mqtt_msg[] = "{\"deviceName\":\"%s\",\"battery\":{\"percentage\":%d,\"voltage\":%.1f},\"shift\":%d,\"speed\":%.1f,\"state\":%d,\"track\":%d,\"led\":%d,\"beep\":%d,\"temp\":%.1f,\"humid\":%.1f}";

osThreadId_t Task1_ID;
osThreadId_t Task2_ID;
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_beep_init();
    user_led_init();
    INA219_Init();
    PCF8574_Init();
    uapi_watchdog_disable();
    user_pwm_init();
    car_io_init();
    user_key_init();
    PID_Init();
    pid_timer_init();
    motor_enc_init();
   
    SSD1306_Init(); // OLED 显示屏初始化
    SHT20_Init(); // SHT20初始化
    SSD1306_CLS();  // 清屏
    app_uart_init_config();
}
void car_contral_task(const char* arg)
{
    unused(arg);
    while (1) {
        if ((systemValue.car_speed != systemValue.last_speed)) {
            systemValue.last_speed = systemValue.car_speed;
            target_speed = 0;
            change_flag = 0;
        }
        // 读取温湿度数据
        SHT20_ReadData(&systemValue.temperature, &systemValue.humidity);
        INA219_get_bus_voltage_mv(&systemValue.battery_voltage);
        systemValue.percentage = (((float)(systemValue.battery_voltage)/1000 - 6.5)/1.9)*100;
        oled_show();
        osal_msleep(500); /* 500: 延时500ms */
    }
}

void main_task(void *argument)
{
    unused(argument);
    char TOPIC_PUB[40] = {0};
    char send_buf[256] = {0};

    my_peripheral_init();
    SSD1306_ShowStr(OLED_TEXT16_COLUMN_0, OLED_TEXT16_LINE_0, "Connecting      ", TEXT_SIZE_16);
    nfc_connect_wifi_init();    // 建立 WiFi 连接
    mqtt_connect();    // 建立 MQTT 连接

    osThreadAttr_t attr;
    attr.name = "Task2";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = osPriorityNormal;

    Task2_ID = osThreadNew((osThreadFunc_t)car_contral_task, NULL, &attr);
    if (Task2_ID != NULL) {
        printf("ID = %d, Create Task2_ID is OK!\r\n", Task2_ID);
    }

    sprintf(TOPIC_PUB, MQTT_TOPIC_PUB, car_name);
    while (1) {
        osal_mdelay(1000);
        memset_s(send_buf, sizeof(send_buf), 0, sizeof(send_buf));
        sprintf(send_buf, mqtt_msg, car_name,systemValue.percentage,(float)(systemValue.battery_voltage)/1000,\
                systemValue.car_speed,(float)systemValue.L_enc,systemValue.car_status,systemValue.auto_track_flag,systemValue.car_led,systemValue.car_beep,systemValue.temperature,systemValue.humidity);
        mqtt_publish(TOPIC_PUB, send_buf);
    }
}

static void demo(void)
{
    printf("Enter demo()!\r\n");
    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)main_task, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
   
   
}
app_run(demo);
