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

#include "common.h"
#include "nv.h"
#include "soc_osal.h"
#include "app_init.h"
#include "nv_common_cfg.h"
#include "common_def.h"
#include "cmsis_os2.h"
#include <stdio.h>
/* 定义常量 */
#define ADDR_LEN6 6          // 数据缓冲区长度
#define KEYID 0xa            // NV（Non-Volatile，非易失存储）操作的关键字ID
#define DELAY_TIME_MS 300    // 延迟时间（毫秒）
osThreadId_t Task1_ID;       // 任务1的线程ID

/* 全局数据缓冲区，初始化为 {0x01, 0x02, 0x03, 0x04, 0x05, 0x06} */
uint8_t buf[ADDR_LEN6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

/**
 * @brief  NV存储写入函数
 * @note   将全局缓冲区buf的数据写入到非易失存储中
 * @retval 成功返回1，失败返回0
 */
static int nv_write_func(void)
{
    // 打印准备写入的数据内容，用于调试
    printf("[write data]: ");
    for (size_t i = 0; i < sizeof(buf); i++) {
        printf("%x ", buf[i]);
    }
    printf("\r\n");
    
    // 准备NV操作参数
    uint16_t key = KEYID;                      // 设置NV存储的关键字
    uint16_t key_len = sizeof(mytest_config_t); // 计算需要写入的数据长度（根据结构体大小）
    
    // 在堆上动态分配内存，用于存储要写入的数据
    uint8_t *product_config = osal_vmalloc(key_len);
    // 将全局缓冲区buf的数据拷贝到临时分配的内存中
    memcpy(product_config, buf, sizeof(buf));
    
    // 调用NV写API，将数据写入非易失存储
    int ret = uapi_nv_write(key, product_config, key_len);
    
    // 释放动态分配的内存
    osal_vfree(product_config);
    product_config = NULL; // 将指针置为NULL，防止野指针
    
    // 根据写入操作返回值判断成功与否
    if (ret == 0) {
        return 1; // 写入成功
    } else {
        return 0; // 写入失败
    }
}

/**
 * @brief  NV存储读取函数
 * @note   从非易失存储中读取数据，并与全局缓冲区buf中的数据相加
 * @retval 成功返回1，失败返回0
 */
static int nv_read_func(void)
{
    // 准备NV操作参数
    uint16_t key = KEYID;                      // 设置NV存储的关键字
    uint16_t key_len = sizeof(mytest_config_t); // 计算需要读取的数据长度
    uint16_t real_len = 0;                     // 实际读取到的数据长度
    
    // 在堆上动态分配内存，用于存储从NV读取的数据
    uint8_t *read_value = osal_vmalloc(key_len);
    
    // 调用NV读API，从非易失存储中读取数据
    int ret = uapi_nv_read(key, key_len, &real_len, read_value);
    
    // 判断读取操作是否成功
    if (ret == 0) {
        // 打印读取到的数据内容，用于调试
        printf("[get data]: ");
        for (size_t i = 0; i < sizeof(buf); i++) {
            printf("%x ", read_value[i]);
        }
        printf("\r\n");
        
        // 将读取到的数据与全局缓冲区buf中的对应数据相加
        for (size_t i = 0; i < sizeof(buf); i++) {
            buf[i] += read_value[i];
        }
        
        // 释放动态分配的内存
        osal_vfree(read_value);
        read_value = NULL; // 将指针置为NULL，防止野指针
        return 1; // 读取成功
    } else {
        // 读取失败时也要释放内存
        osal_vfree(read_value);
        read_value = NULL; // 将指针置为NULL，防止野指针
        return 0; // 读取失败
    }
}

/**
 * @description: 任务1的主函数
 * @param {const char} *argument 线程参数（未使用）
 * @return {*}
 */
void task1(const char *argument)
{
    unused(argument); // 显式标记参数未使用，避免编译器警告
    
    osDelay(DELAY_TIME_MS); // 延迟一段时间，等待串口初始化完成等准备工作
    printf("----------------------------------!\n");
    
    // 第一步：尝试从NV读取数据
    uint8_t ret = nv_read_func();
    if (ret) {
        printf("nv read ok!\n");
    } else {
        printf("nv read fail!\n");
    }
    
    osDelay(DELAY_TIME_MS); // 再次延迟
    
    // 第二步：将处理后的数据写入NV
    ret = nv_write_func();
    if (ret) {
        printf("nv write ok!\n");
    } else {
        printf("nv write fail!\n");
    }
}

/**
 * @brief  内核任务示例初始化函数
 * @note   创建并启动任务1
 * @retval None
 */
static void kernel_task_example(void)
{
    // 定义线程属性结构体
    osThreadAttr_t attr;
    attr.name = "Task1";         // 线程名称
    attr.attr_bits = 0U;         // 线程属性位
    attr.cb_mem = NULL;          // 线程控制块内存地址（NULL表示自动分配）
    attr.cb_size = 0U;           // 线程控制块大小
    attr.stack_mem = NULL;       // 线程堆栈内存地址（NULL表示自动分配）
    attr.stack_size = 0x1000;    // 线程堆栈大小（4KB）
    attr.priority = osPriorityNormal; // 线程优先级（普通优先级）
    
    // 创建任务1线程
    Task1_ID = osThreadNew((osThreadFunc_t)task1, NULL, &attr);
    
    // 检查线程是否创建成功
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\n", Task1_ID);
    }
}

/* 使用app_run宏启动示例程序，传入内核任务初始化函数 */
app_run(kernel_task_example);