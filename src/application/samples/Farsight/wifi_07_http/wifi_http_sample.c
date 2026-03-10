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

#include "systick_porting.h"         // 系统滴答移植
#include "hal_systick.h"             // 硬件系统滴答
#include "systick.h"                 // 系统滴答功能

#include "lwip/netdb.h"              // 网络数据库操作（DNS等）

#define WIFI_TASK_STACK_SIZE 0x2000  // WiFi任务栈大小（8KB）

#define DELAY_TIME_MS 100            // 延迟时间100毫秒
#define BUFFER_SIZE 1024             // 接收缓冲区大小1KB

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
    wifi_event_stru wifi_event_cb = {0};  // WiFi事件回调结构体，初始化为0

    // 设置WiFi事件回调函数
    wifi_event_cb.wifi_event_scan_state_changed = wifi_scan_state_changed;       // 扫描状态改变回调
    wifi_event_cb.wifi_event_connection_changed = wifi_connection_changed;       // 连接状态改变回调
    
    /* 注册事件回调 */
    if (wifi_register_event_cb(&wifi_event_cb) != 0) {
        printf("wifi_event_cb register fail.\r\n");  // 注册失败提示
        return -1;
    }
    printf("wifi_event_cb register succ.\r\n");  // 注册成功提示

    // 连接WiFi网络
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
    
    // 连接到服务器
    if (connect(sock_fd, (struct sockaddr *)&send_addr, sizeof(send_addr)) != 0) {
        printf("Failed to connect to theerrno=%d", errno);  // 连接失败，打印错误码
        return 0;
    }
    printf("Connection to server successful\n");  // 连接成功提示
    
    // 准备HTTP请求（修改Host字段为百度域名）
    // 这是一个标准的HTTP GET请求，请求百度首页
    const char *request = "GET / HTTP/1.1\r\n"         // HTTP GET请求行
                          "Host: www.baidu.com\r\n"    // 请求主机头（百度域名）
                          "Connection: close\r\n"      // 连接关闭选项
                          "Accept: */*\r\n\r\n";       // 接受任何类型的内容，以空行结束头部
    

    // 发送HTTP请求到服务器
    if (send(sock_fd, request, strlen(request), 0) != (int)strlen(request)) {
        printf("send failed");  // 发送失败提示
        lwip_close(sock_fd);    // 关闭套接字
        return -1;
    }

    /* 设置套接字接收超时为1秒 */ 
    struct timeval receiving_timeout;        // 超时时间结构体
    receiving_timeout.tv_sec = 1;            // 秒数
    receiving_timeout.tv_usec = 0;           // 微秒数
    // 设置套接字选项：接收超时
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) 
    { 
        printf("failed to set socket receiving timeout");  // 设置超时失败
        lwip_close(sock_fd);                               // 关闭套接字
        return 1; 
    } 
    printf("set socket receiving timeout success\n");  // 设置超时成功
    
    char buffer[BUFFER_SIZE];  // 接收数据缓冲区
    int bytes_received;        // 接收到的字节数
    
    // 循环接收服务器响应数据
    do {
        memset(buffer, 0, sizeof(buffer));  // 清空缓冲区
        bytes_received = recv(sock_fd, buffer, BUFFER_SIZE, 0);  // 接收数据
        
        // 逐字符打印接收到的数据（避免字符串中没有结束符的问题）
        for (int i = 0; i < bytes_received; i++) 
            printf("%c", buffer[i]);  // 打印每个字符
            
    } while(bytes_received);  // 当接收到数据时继续循环，接收完毕（返回0）时退出
   
    lwip_close(sock_fd);  // 关闭TCP连接
    return 0;             // 返回成功
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
    attr.stack_size = WIFI_TASK_STACK_SIZE; // 线程栈大小
    attr.priority = osPriorityNormal;     // 线程优先级
    
    // 创建新线程执行sta_sample_init函数
    if (osThreadNew((osThreadFunc_t)sta_sample_init, NULL, &attr) == NULL) {
        printf("Create sta_sample_task fail.\r\n");  // 创建线程失败
    }
    printf("Create sta_sample_task succ.\r\n");  // 创建线程成功
}

/* 应用运行宏：启动STA示例 */
app_run(sta_sample);