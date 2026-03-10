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
#include "hal_bsp_rgb/hal_bsp_aw2013.h"
#include "app_init.h"
osThreadId_t task1_ID; // 任务1
#define RGB_ON 255
#define RGB_OFF 0
#define TASK_DELAY_TIME 100
/**
 * @brief 任务1：RGB LED循环显示任务
 * @note 该任务用于控制AW2013驱动RGB LED，循环显示红、绿、蓝三种颜色
 */
void task1(void)
{
    // 初始化AW2013三色LED驱动芯片
    // 配置I2C通信、寄存器参数等
    AW2013_Init(); 
    
    // 初始化RGB LED为关闭状态，确保起始状态已知
    AW2013_Control_Red(RGB_OFF);    // 关闭红色通道
    AW2013_Control_Green(RGB_OFF);  // 关闭绿色通道
    AW2013_Control_Blue(RGB_OFF);   // 关闭蓝色通道

    // 任务主循环
    while (1) {
        // 显示红色：开启红色，关闭绿色和蓝色
        AW2013_Control_RGB(RGB_ON, RGB_OFF, RGB_OFF);
        osDelay(TASK_DELAY_TIME);  // 保持红色显示一段时间

        // 显示绿色：关闭红色，开启绿色，关闭蓝色
        AW2013_Control_RGB(RGB_OFF, RGB_ON, RGB_OFF);
        osDelay(TASK_DELAY_TIME);  // 保持绿色显示一段时间

        // 显示蓝色：关闭红色和绿色，开启蓝色
        AW2013_Control_RGB(RGB_OFF, RGB_OFF, RGB_ON);
        osDelay(TASK_DELAY_TIME);  // 保持蓝色显示一段时间
    }
}
static void base_rgb_demo(void)
{
    printf("Enter base_rgb_demo()!\r\n");

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
app_run(base_rgb_demo);