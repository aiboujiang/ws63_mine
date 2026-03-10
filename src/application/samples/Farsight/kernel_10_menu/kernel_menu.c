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

#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "common_def.h"
#include <stdio.h>
#include "gpio.h"
#include "pinctrl.h"
#include "adc.h"
#include "adc_porting.h"
#include "platform_core_rom.h"
#include "uart.h"
#include "watchdog.h"
#define DELAY_TASK1_MS 100
#define TIME_OUT 100

#define CONFIG_UART_TXD_PIN 17
#define CONFIG_UART_RXD_PIN 18
#define CONFIG_UART_PIN_MODE 1
#define CONFIG_UART_ID UART_BUS_0

#define MSG_QUEUE_NUMBER 16 // 定义消息队列对象的个数


// 任务优先级定义（映射到cmsis_os2.h中的优先级枚举）
#define PRIO_CMD_PROCESSOR    osPriorityNormal
#define PRIO_SYSTEM_MONITOR   osPriorityBelowNormal
#define PRIO_PERIODIC_TASK    osPriorityBelowNormal

/* 串口接收缓冲区大小 */
#define UART_RX_MAX 20
uint8_t g_uart_rx_buffer[UART_RX_MAX];
/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;
    uint16_t value_len;
} msg_data_t;
msg_data_t msg_data = {0};
osMessageQueueId_t MsgQueue_ID; // 消息队列的ID
osStatus_t msgStatus;

osThreadId_t Task1_ID; //  任务1 ID
osThreadId_t Task2_ID; //  任务2 ID
osThreadId_t Task3_ID; //  任务3 ID
osTimerId_t Timer_ID; // 定时器ID
osSemaphoreId_t Semaphore_ID; // 信号量ID
void app_uart_init_config(void);
/**
 * @description: 定时器1回调函数
 * @param {*}
 * @return {*}
 */
void timer1_callback(void *argument)
{
    unused(argument);
        // 释放信号量，触发周期性任务
        osStatus_t status = osSemaphoreRelease(Semaphore_ID);
        if (status != osOK) {
            // 信号量释放失败处理
            printf("\r\n定时器回调: 信号量释放失败 (%d)\r\n", status);
        }
}

/**
 * @description: 周期性任务
 * @param {*}
 * @return {*}
 */
void periodic_task(const char *argument)
{
    unused(argument);

    uint32_t count = 0;
    while (1) {
        // 等待信号量（由定时器释放）
        osStatus_t status = osSemaphoreAcquire(Semaphore_ID, osWaitForever);
        if (status == osOK) {
            printf("\r\n周期性任务执行: %lu次\r\n", count++);
        } else {
            // 处理信号量获取失败
            printf("\r\n周期性任务: 信号量获取失败 (%d)\r\n", status);
            osDelay(100);
        }
    }
}


/**
 * @description: 系统监控任务
 * @param {*}
 * @return {*}
 */
void system_monitor_task(const char *argument)
{
    unused(argument);
    osVersion_t kernel_version;
    char kernel_id[100];
    osStatus_t status;

    while (1)
    {
        // 获取RTOS内核信息
        status = osKernelGetInfo(&kernel_version, kernel_id, sizeof(kernel_id));
        if (status == osOK) {
            printf("Kernel ID: %s\n", kernel_id);
        } else {
            // 处理错误
            printf("Failed to get kernel info, status: %d\n", status);
        }
        printf("当前Tick: %lu\r\n", osKernelGetTickCount());
        printf("活跃线程数: %lu\r\n", osThreadGetCount());
        osDelay(500); // 每5秒执行一次
    }
}

/**
 * @description: 命令处理任务
 * @param {*}
 * @return {*}
 */
void command_processor_task(const char *argument)
{
    unused(argument);
    app_uart_init_config();
    osDelay(100);
    printf("\r\n命令处理器就绪,输入help查看命令\r\n");
    printf("  help    - 显示帮助\r\n");
    printf("  info    - 显示系统信息\r\n");
    printf("  timer   - 切换定时器状态\r\n");
    printf("> ");
    
    while (1) {
    // 从消息队列获取串口接收的字符
    osStatus_t status = osMessageQueueGet(MsgQueue_ID, &msg_data, NULL, osWaitForever);
    if (status == osOK) {
        if (msg_data.value != NULL) {  
            // 解析命令
            if (strcmp((char*)msg_data.value, "help") == 0) {
                printf("支持的命令:\r\n");
                printf("  help    - 显示帮助\r\n");
                printf("  info    - 显示系统信息\r\n");
                printf("  timer   - 切换定时器状态\r\n");
            } 
            else if (strcmp((char*)msg_data.value, "info") == 0) {
                printf("系统信息:\r\n");
                printf("  滴答频率: %lu Hz\r\n", osKernelGetTickFreq());
                printf("  当前线程: %p\r\n", osThreadGetId());
            }
            else if (strcmp((char*)msg_data.value, "timer") == 0) {
                static uint8_t timer_running = 1;
                if (timer_running) {
                    osTimerStop(Timer_ID);
                    printf("定时器已停止\r\n");
                } else {
                    osTimerStart(Timer_ID, 100); // 1秒周期
                    printf("定时器已启动(1秒周期)\r\n");
                }
                timer_running = !timer_running;
            }
            else {
                 printf("未知命令\r\n");
            }
            
            printf("> ");
             osal_vfree(msg_data.value);
            memset(msg_data.value, 0, sizeof(msg_data.value)); // 清空命令缓冲区
        }  
    }  
    } 
}




static void kernel_timer_example(void)
{
    uapi_watchdog_disable();
    // 创建消息队列
    MsgQueue_ID = osMessageQueueNew(MSG_QUEUE_NUMBER, sizeof(msg_data_t),
                                    NULL); // 消息队列中的消息个数，消息队列中的消息大小，属性
    if (MsgQueue_ID != NULL)
    {
        printf("ID = %d, Create MsgQueue_ID is OK!\n", MsgQueue_ID);
    }
    // 创建信号量
    Semaphore_ID = osSemaphoreNew(1, 1, NULL); // 参数: 最大计数值，初始计数值，参数配置
    if (Semaphore_ID != NULL) {
        printf("ID = %d, Create Semaphore_ID is OK!\n", Semaphore_ID);
    }
    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0X1000;
    attr.priority = PRIO_SYSTEM_MONITOR;

    Task1_ID = osThreadNew((osThreadFunc_t)system_monitor_task, NULL, &attr); // 创建任务1
    if (Task1_ID != NULL)
    {
        printf("ID = %d, Create system_monitor_task is OK!\n", Task1_ID);
    }
    attr.priority = PRIO_CMD_PROCESSOR;
    Task2_ID = osThreadNew((osThreadFunc_t)command_processor_task, NULL, &attr); // 创建任务2
    if (Task2_ID != NULL)
    {
        printf("ID = %d, Create command_processor_task is OK!\n", Task2_ID);
    }
    attr.priority = PRIO_PERIODIC_TASK;
    Task3_ID = osThreadNew((osThreadFunc_t)periodic_task, NULL, &attr); // 创建任务2
    if (Task3_ID != NULL)
    {
        printf("ID = %d, Create periodic_task is OK!\n", Task3_ID);
    }

    Timer_ID = osTimerNew(timer1_callback, osTimerPeriodic, NULL, NULL); // 创建定时器
    if (Timer_ID != NULL)
    {
        printf("ID = %d, Create timer1_callback is OK!\n", Timer_ID);

        osStatus_t timerStatus =
            osTimerStart(Timer_ID, 300U); // 开始定时器， 并赋予定时器的定时值（在ws63中，1U=10ms，100U=1S）
        if (timerStatus != osOK)
        {
            printf("timer is not startRun !\n");
        }
    }
}

/* Run the sample. */
app_run(kernel_timer_example);


/* 串口接收回调 */
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    void *buffer_cpy = osal_vmalloc(length);
    if (memcpy_s(buffer_cpy, length, buffer, length) != EOK) {
        osal_vfree(buffer_cpy);
        return;
    }
    msg_data.value = (uint8_t *)buffer_cpy;
    msg_data.value_len = length;
  
   // 发送消息到队列，使用非阻塞模式防止回调阻塞
    osStatus_t status = osMessageQueuePut(MsgQueue_ID, &msg_data, 0, 0);
    if (status != osOK) {
        // 消息发送失败，释放已分配的内存
        osal_vfree(buffer_cpy);
    }
}
/* 串口初始化配置 */
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    uart_attr_t attr = {
        .baud_rate = 115200, .data_bits = UART_DATA_BIT_8, .stop_bits = UART_STOP_BIT_1, .parity = UART_PARITY_NONE};
    uart_buffer_config.rx_buffer_size = UART_RX_MAX;
    uart_buffer_config.rx_buffer = g_uart_rx_buffer;
    uart_pin_config_t pin_config = {.tx_pin = S_MGPIO0, .rx_pin = S_MGPIO1, .cts_pin = PIN_NONE, .rts_pin = PIN_NONE};
    uapi_uart_deinit(CONFIG_UART_ID);
    int res = uapi_uart_init(CONFIG_UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    if (uapi_uart_register_rx_callback(CONFIG_UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, uart_read_handler) ==
        ERRCODE_SUCC) {
        printf("uart%d int mode register receive callback succ!\r\n", CONFIG_UART_ID);
    }
}