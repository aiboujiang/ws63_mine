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
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "hal_bsp_nfc/hal_bsp_nfc.h"
#include "app_init.h"
osThreadId_t task1_ID; // 任务1设置为低优先级任务

#define TASK_DELAY_TIME 100
void task1(void)
{
    uint8_t ndefLen = 0;     // NDEF数据包的长度
    uint8_t ndef_Header = 0; // NDEF消息开始标志位（当前未使用）
    size_t i = 0;
    
    // 初始化NFC（近场通信）模块
    nfc_Init();
    osDelay(TASK_DELAY_TIME);  // 延迟一段时间，等待NFC模块稳定
    printf("I2C Test Start\r\n");  // 打印调试信息，表示I2C测试开始

    // 读取NFC数据包头部分，获取数据长度和头部信息
    if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) != true) {
        printf("NT3HReadHeaderNfc is failed.\r\n");  // 读取包头失败
        return;  // 任务结束
    }
    
    // 调整数据长度：将实际数据长度加上包头大小（因为之前获取的长度不包含头部字节）
    ndefLen += NDEF_HEADER_SIZE;
    
    // 数据长度有效性检查：如果长度小于等于包头大小，说明没有有效数据
    if (ndefLen <= NDEF_HEADER_SIZE) {
        printf("ndefLen <= 2\r\n");  // 数据长度异常
        return;  // 任务结束
    }
    
    // 动态分配内存用于存储NDEF数据（多分配1字节用于安全终止）
    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("ndefBuff malloc is Falied!\r\n");  // 内存分配失败
        return;  // 任务结束
    }
    
    // 读取完整的NDEF数据包到缓冲区
    if (get_NDEFDataPackage(ndefBuff, ndefLen) != ERRCODE_SUCC) {
        printf("get_NDEFDataPackage is failed. \r\n");  // 数据读取失败
        free(ndefBuff);  // 释放已分配的内存
        return;  // 任务结束
    }

    // 打印NDEF数据包的原始字节内容（用于调试）
    printf("start print ndefBuff.\r\n");
    for (i = 0; i < ndefLen; i++) {
        printf("0x%x ", ndefBuff[i]);  // 以十六进制格式输出每个字节
    }
    printf("\r\n");  // 换行

    // 任务主循环（保持任务持续运行）
    while (1) {
        osDelay(TASK_DELAY_TIME);  // 延时等待
        // 注：实际应用中这里可以添加周期性的NFC检测逻辑
    }
    
    // 理论上不会执行到这里，但为完整性添加内存释放
    free(ndefBuff);  // 释放动态分配的内存
}
static void base_nfc_demo(void)
{
    printf("Enter base_nfc_demo()!\r\n");

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
app_run(base_nfc_demo);