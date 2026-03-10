/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 包含必要的头文件 */
#include "lwip/netifapi.h"           // LwIP 网络接口API
#include "wifi_hotspot.h"            // WiFi热点功能
#include "wifi_hotspot_config.h"     // WiFi热点配置
#include "stdlib.h"                  // 标准库
#include "uart.h"                    // 串口通信
#include "lwip/nettool/misc.h"       // LwIP网络工具
#include "soc_osal.h"                // 操作系统抽象层
#include "app_init.h"                // 应用初始化
#include "cmsis_os2.h"               // CMSIS-RTOS2 API
#include "wifi_device.h"             // WiFi设备功能
#include "wifi_event.h"              // WiFi事件处理
#include "lwip/sockets.h"            // LwIP套接字API
#include "lwip/ip4_addr.h"           // IPv4地址处理
#include "wifi/wifi_connect.h"       // WiFi连接功能

#define WIFI_TASK_STACK_SIZE 0x2000  // WiFi任务栈大小（8KB）
#define DELAY_TIME_MS 100            // 延迟时间100毫秒（虽然定义但未使用）

#define PKT_DATA_LEN 1000            // 数据包长度：1000字节

/* 
 * WiFi扫描状态改变回调函数
 * @param state: 扫描状态
 * @param size: 扫描到的AP数量
 */
static void wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    UNUSED(state);  // 避免未使用参数警告
    UNUSED(size);   // 避免未使用参数警告
    printf("Scan done!\r\n");  // 打印扫描完成信息
    return;
}

/* 
 * WiFi连接状态改变回调函数
 * @param state: 连接状态
 * @param info: 连接信息结构体指针
 * @param reason_code: 连接原因代码
 */
static void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    UNUSED(reason_code);  // 避免未使用参数警告

    // 当WiFi状态为可用时，打印连接信息
    if (state == WIFI_STATE_AVALIABLE)
        printf("[WiFi]:%s, [RSSI]:%d\r\n", info->ssid, info->rssi); // 打印SSID和信号强度
}

/* 
 * STA模式示例初始化函数
 * @param argument: 线程参数（未使用）
 * @return int: 成功返回0，失败返回非0
 */
int sta_sample_init(const char *argument)
{
    argument = argument;  // 避免未使用参数警告
    int sock_fd;          // 套接字文件描述符
    // 服务器的地址信息
    struct sockaddr_in send_addr;  // 服务器地址结构体
    char send_buf[PKT_DATA_LEN];   // 发送数据缓冲区（1000字节）
    wifi_event_stru wifi_event_cb = {0};  // WiFi事件回调结构体，初始化为0

    // 设置WiFi事件回调函数
    wifi_event_cb.wifi_event_scan_state_changed = wifi_scan_state_changed;       // 扫描状态改变回调
    wifi_event_cb.wifi_event_connection_changed = wifi_connection_changed;       // 连接状态改变回调
    
    /* 注册WiFi事件回调 */
    if (wifi_register_event_cb(&wifi_event_cb) != 0) {
        printf("wifi_event_cb register fail.\r\n");  // 注册失败提示
        return -1;
    }
    printf("wifi_event_cb register succ.\r\n");  // 注册成功提示

    // 连接WiFi网络（连接到预设的AP）
    wifi_connect();

    printf("create socket start! \r\n");
    // 创建TCP套接字（SOCK_STREAM表示TCP协议）
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) != ERRCODE_SUCC) {
        printf("create socket failed!\r\n");  // 创建套接字失败
        return 0;
    }
    printf("create socket succ!\r\n");  // 创建套接字成功

    /* 初始化预连接的服务端地址 */
    send_addr.sin_family = AF_INET;                           // IPv4地址族
    send_addr.sin_port = htons(CONFIG_SERVER_PORT);           // 服务器端口（转换为网络字节序）
    send_addr.sin_addr.s_addr = inet_addr(CONFIG_SERVER_IP);  // 服务器IP地址（转换为网络格式）
    
    // 连接到TCP服务器
    if (connect(sock_fd, (struct sockaddr *)&send_addr, sizeof(send_addr)) != 0) {
        printf("Failed to connect to the server\n");  // 连接失败提示
        return 0;
    }
    printf("Connection to server successful\n");  // 连接成功提示
    
    // 准备测试数据：填充1000字节的'A'字符
    for(uint16_t i = 0; i < PKT_DATA_LEN; i++)
        send_buf[i] = 'A';  // 每个字节都填充为字符'A'

    // 主循环：持续发送数据到服务器
    while (1) {
        /* 发送数据到服务端 */
        // 发送1000字节的数据包到TCP服务器
        send(sock_fd, send_buf, sizeof(send_buf), 0);
        // 注意：这里没有延时，会以最大速率持续发送数据
        // 也没有检查send()函数的返回值，可能存在发送失败的情况
    }
    
    // 关闭TCP连接（理论上不会执行到这里，因为上面是无限循环）
    lwip_close(sock_fd);
    return 0;
}

/* 
 * STA示例任务创建函数
 * 创建并启动sta_sample_init任务
 */
static void sta_sample(void)
{
    osThreadAttr_t attr;  // 线程属性结构体
    attr.name = "sta_sample_task";        // 线程名称
    attr.attr_bits = 0U;                  // 线程属性位
    attr.cb_mem = NULL;                   // 线程控制块内存地址
    attr.cb_size = 0U;                    // 线程控制块大小
    attr.stack_mem = NULL;                // 线程栈内存地址
    attr.stack_size = WIFI_TASK_STACK_SIZE; // 线程栈大小（8KB）
    attr.priority = osPriorityNormal;     // 线程优先级（普通优先级）
    
    // 创建新线程执行sta_sample_init函数
    if (osThreadNew((osThreadFunc_t)sta_sample_init, NULL, &attr) == NULL) {
        printf("Create sta_sample_task fail.\r\n");  // 创建线程失败
    }
    printf("Create sta_sample_task succ.\r\n");  // 创建线程成功
}

/* 应用运行宏：启动STA示例 */
app_run(sta_sample);