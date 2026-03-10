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

/**
 * @file main.c
 * @brief 星闪(SparKLink)基础实验 - GPIO控制LED闪烁（基于RTOS）
 * @description 本程序演示如何在星闪平台上使用CMSIS-RTOS2 API创建任务，
 *              并通过GPIO控制LED闪烁。
 */

#include "pinctrl.h"           // 引脚控制相关头文件
#include "gpio.h"              // GPIO操作相关头文件
#include "app_init.h"          // 应用程序初始化头文件
#include "cmsis_os2.h"         // CMSIS-RTOS2 API头文件
#include "platform_core_rom.h" // 平台核心ROM相关头文件

osThreadId_t task1_ID;         // 声明任务1的ID变量，用于存储任务句柄

#define DELAY_TIME_MS 50       // 定义延时时间，单位为毫秒（实际周期为100ms）

/**
 * @brief LED闪烁任务函数
 * @param arg 任务参数（本例中未使用）
 * @return int 任务返回值（由于是无限循环，实际不会返回）
 */
static int blink_task(const char *arg)
{
    unused(arg);  // 标记参数未使用，避免编译器警告
    
    // 配置GPIO_13引脚为普通GPIO功能模式（非复用功能）
    uapi_pin_set_mode(GPIO_13, HAL_PIO_FUNC_GPIO);
    // 设置GPIO_13为输出模式
    uapi_gpio_set_dir(GPIO_13, GPIO_DIRECTION_OUTPUT);
    // 设置GPIO_13初始输出高电平（LED初始状态为亮）
    uapi_gpio_set_val(GPIO_13, GPIO_LEVEL_HIGH);
    
    // 任务主循环
    while (1) {
        osDelay(DELAY_TIME_MS);    // 延时50毫秒（RTOS延时，会释放CPU资源）
        uapi_gpio_toggle(GPIO_13); // GPIO电平翻转（高变低，低变高）
        printf("gpio toggle.\n");  // 串口输出调试信息，指示GPIO状态已改变
    }

    return 0; // 理论上不会执行到这里
}

/**
 * @brief 应用程序入口函数
 * @note 此函数由app_run()调用，用于创建RTOS任务
 */
static void blink_entry(void)
{
    printf("Enter blink_entry()!\r\n"); // 打印入口函数启动信息

    // 定义任务属性结构体并初始化
    osThreadAttr_t attr;
    attr.name = "Task1";        // 任务名称（用于调试识别）
    attr.attr_bits = 0U;        // 特殊属性位，通常为0
    attr.cb_mem = NULL;         // 任务控制块内存指针（NULL表示由系统自动分配）
    attr.cb_size = 0U;          // 任务控制块大小（0表示由系统决定）
    attr.stack_mem = NULL;      // 任务栈内存指针（NULL表示由系统自动分配）
    attr.stack_size = 0x1000;   // 任务栈大小（4KB，根据任务需求调整）
    attr.priority = osPriorityNormal; // 任务优先级（设置为普通优先级）

    // 创建新任务
    task1_ID = osThreadNew((osThreadFunc_t)blink_task, // 任务函数指针
                           NULL,                       // 传递给任务的参数
                           &attr);                     // 任务属性
                           
    // 检查任务是否创建成功
    if (task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", task1_ID); // 打印任务创建成功信息
    }
}

/* 运行示例应用程序 */
app_run(blink_entry); // 启动应用程序，执行blink_entry函数