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

#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "i2c.h"
#include "securec.h"
#include "cmsis_os2.h"
#include "hal_bsp_oled/hal_bsp_ssd1306.h"
#include "app_init.h"

osThreadId_t task1_ID; // 任务1

#define DELAY_TIME_MS 100
#define SEC_MAX 60
#define MIN_MAX 60
#define HOUR_MAX 24

/**
 * @brief 任务1：模拟时钟显示任务
 * @note 该任务负责在OLED屏幕上显示一个运行的模拟时钟
 */
void task1(void)
{
    // 定义显示缓冲区，用于存储格式化的时间字符串
    char display_buffer[20] = {0};
    
    // 初始化时间变量（时:分:秒）
    uint8_t hour = 10;   // 起始小时
    uint8_t min = 30;    // 起始分钟
    uint8_t sec = 0;     // 起始秒数

    // OLED显示屏初始化
    printf("ssd1306_Init!\n");
    ssd1306_init();      // 初始化OLED硬件和驱动程序
    ssd1306_cls();       // 清空屏幕所有内容

    // 显示静态标题文本
    ssd1306_show_str(OLED_TEXT16_COLUMN_0,  // 起始列位置（通常是第0列）
                     OLED_TEXT16_LINE_0,    // 起始行位置（第0行）
                     "  Analog Clock ",     // 显示的文本内容
                     TEXT_SIZE_16);         // 字体大小：16像素

    // 显示静态日期文本
    ssd1306_show_str(OLED_TEXT16_COLUMN_0,  // 起始列位置
                     OLED_TEXT16_LINE_3,    // 第3行
                     "   2025-01-01  ",     // 日期信息（可考虑改为动态）
                     TEXT_SIZE_16);         // 字体大小：16像素

    // 任务主循环
    while (1) {
        /* 时间计算逻辑 */
        sec++;  // 秒数增加
        
        // 秒数进位检查（60秒进1分钟）
        if (sec > (SEC_MAX - 1)) {
            sec = 0;
            min++;
        }
        
        // 分钟进位检查（60分钟进1小时）
        if (min > (MIN_MAX - 1)) {
            min = 0;
            hour++;
        }
        
        // 小时进位检查（24小时归零）
        if (hour > (HOUR_MAX - 1)) {
            hour = 0;
        }

        /* 显示更新逻辑 */
        // 清空显示缓冲区，准备新数据
        memset_s(display_buffer, sizeof(display_buffer), 0, sizeof(display_buffer));
        
        // 格式化时间字符串到缓冲区
        if (sprintf_s(display_buffer,           // 目标缓冲区
                     sizeof(display_buffer),    // 缓冲区大小
                     "    %02d:%02d:%02d   ",   // 格式字符串（时:分:秒，两位显示）
                     hour, min, sec) > 0) {     // 时间变量参数
            // 在OLED屏幕上显示时间字符串
            ssd1306_show_str(OLED_TEXT16_COLUMN_0,  // 第0列
                             OLED_TEXT16_LINE_2,    // 第2行（位于标题和日期之间）
                             display_buffer,        // 格式化后的时间字符串
                             TEXT_SIZE_16);         // 16像素字体
        }

        // 任务延时，控制时钟更新频率（1000ms = 1秒更新一次）
        osDelay(DELAY_TIME_MS);  // DELAY_TIME_MS应定义为1000
    }
}
static void base_ssd1306_demo(void)
{
    printf("Enter base_sdd1306_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)task1, NULL, &attr);
    if (task1_ID != NULL) {
        printf("ID = %d, Create task1_ID is OK!\r\n", task1_ID);
    }
}
app_run(base_ssd1306_demo);