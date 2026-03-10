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
#include "tcxo.h"                    // 高精度时钟相关
#include "systick.h"                 // 系统滴答功能
#include "watchdog.h"                // 看门狗功能

static int g_recv_pkt_num = 0;                   // 全局变量：接收到的数据包计数
static uint64_t g_count_before_get_us;           // 全局变量：测试开始时间戳（微秒）
static uint64_t g_count_after_get_us;            // 全局变量：测试结束时间戳（微秒）

#define WIFI_TASK_STACK_SIZE 0x2000              // WiFi任务栈大小（8KB）

#define WIFI_NAME "HQYJ_H3863"                   // SoftAP的SSID名称
#define WIFI_KEY "123456789"                     // SoftAP的密码
#define STACK_IP "192.168.5.1"                   // SoftAP的IP地址
#define CONFIG_SERVER_PORT 8888                  // TCP服务器监听端口

#define RECV_PKT_CNT 300                         // 统计速度的数据包数量阈值

int sock_fd;                                     // 全局变量：服务器监听套接字
int accept_fd;                                   // 全局变量：客户端连接套接字

#define SLE_SPEED_HUNDRED   100                  /* 100，用于浮点数转换的精度因子 */

/* 
 * 获取浮点数的整数部分
 * @param in: 输入的浮点数
 * @return uint32_t: 整数部分
 */
static uint32_t get_float_int(float in)
{
    return (uint32_t)(((uint64_t)(in * SLE_SPEED_HUNDRED)) / SLE_SPEED_HUNDRED);
}

/* 
 * 获取浮点数的小数部分（两位小数）
 * @param in: 输入的浮点数
 * @return uint32_t: 小数部分（放大100倍）
 */
static uint32_t get_float_dec(float in)
{
    return (uint32_t)(((uint64_t)(in * SLE_SPEED_HUNDRED)) % SLE_SPEED_HUNDRED);
}

/* 
 * SoftAP功能配置和启动函数
 * @return errcode_t: 成功返回ERRCODE_SUCC，失败返回ERRCODE_FAIL
 */
errcode_t example_softap_function(td_void)
{
    /* SoftAp接口的信息 */
    td_char ssid[WIFI_MAX_SSID_LEN] = WIFI_NAME;                 // SSID名称
    td_char pre_shared_key[WIFI_MAX_KEY_LEN] = WIFI_KEY;         // 预共享密钥（密码）
    softap_config_stru hapd_conf = {0};                          // 基本配置结构体
    softap_config_advance_stru config = {0};                     // 高级配置结构体
    td_char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "ap0";            /* 创建的SoftAp接口名：ap0 */
    struct netif *netif_p = TD_NULL;                             // 网络接口指针
    ip4_addr_t st_gw;                                            // 网关地址
    ip4_addr_t st_ipaddr;                                        // IP地址
    ip4_addr_t st_netmask;                                       // 子网掩码
    
    // 设置IP地址：192.168.5.1
    IP4_ADDR(&st_ipaddr, 192, 168, 5, 1);    /* IP地址设置：192.168.5.1 */
    IP4_ADDR(&st_netmask, 255, 255, 255, 0); /* 子网掩码设置：255.255.255.0 */
    IP4_ADDR(&st_gw, 192, 168, 5, 2);        /* 网关地址设置：192.168.5.2 */

    /* 配置SoftAp基本参数 */
    (td_void) memcpy_s(hapd_conf.ssid, sizeof(hapd_conf.ssid), ssid, sizeof(ssid)); // 复制SSID
    (td_void) memcpy_s(hapd_conf.pre_shared_key, WIFI_MAX_KEY_LEN, pre_shared_key, WIFI_MAX_KEY_LEN); // 复制密码
    hapd_conf.security_type = 3; /* 3: 加密方式设置为WPA_WPA2_PSK */
    hapd_conf.channel_num = 6;   /* 6：工作信道设置为6信道（2.4GHz频段） */
    hapd_conf.wifi_psk_type = 0; // PSK类型

    /* 配置SoftAp高级参数 */
    config.beacon_interval = 100; /* 100：Beacon周期设置为100ms */
    config.dtim_period = 2;       /* 2：DTIM周期设置为2 */
    config.gi = 0;                /* 0：short GI默认关闭 */
    config.group_rekey = 86400;   /* 86400：组播秘钥更新时间设置为1天（秒） */
    config.protocol_mode = 4;     /* 4：协议类型设置为802.11b + 802.11g + 802.11n + 802.11ax */
    config.hidden_ssid_flag = 1;  /* 1：不隐藏SSID */
    
    // 设置高级配置
    if (wifi_set_softap_config_advance(&config) != 0) {
        return ERRCODE_FAIL;
    }
    
    /* 启动SoftAp接口 */
    if (wifi_softap_enable(&hapd_conf) != 0) {
        return ERRCODE_FAIL;
    }
    
    /* 配置DHCP服务器 */
    netif_p = netif_find(ifname); // 查找网络接口
    if (netif_p == TD_NULL) {
        (td_void) wifi_softap_disable(); // 关闭SoftAP
        return ERRCODE_FAIL;
    }
    
    // 设置网络接口地址
    if (netifapi_netif_set_addr(netif_p, &st_ipaddr, &st_netmask, &st_gw) != 0) {
        (td_void) wifi_softap_disable();
        return ERRCODE_FAIL;
    }
    
    // 启动DHCP服务器
    if (netifapi_dhcps_start(netif_p, NULL, 0) != 0) {
        (td_void) wifi_softap_disable();
        return ERRCODE_FAIL;
    }
    
    printf("SoftAp start success.\r\n"); // SoftAP启动成功

    /* 创建TCP服务器套接字 */
    printf("create socket start! \r\n");
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) != ERRCODE_SUCC) {
        printf("create socket failed!\r\n");
        return ERRCODE_FAIL;
    }
    printf("create socket succ!\r\n");
    
    /* 初始化服务器地址信息 */
    struct sockaddr_in send_addr; // 服务器地址结构体
    send_addr.sin_family = AF_INET;                           // IPv4地址族
    send_addr.sin_port = htons(CONFIG_SERVER_PORT);           // 服务器端口（转换为网络字节序）
    send_addr.sin_addr.s_addr = inet_addr(STACK_IP);          // 服务器IP地址（转换为网络格式）
 
    // 绑定套接字到指定地址和端口
    if(bind(sock_fd, (struct sockaddr *)&send_addr, sizeof(send_addr)) != 0)
    {
         printf("bind is error");
         return ERRCODE_FAIL;
    }
    printf("bind succeeded\n");
    
    // 开始监听连接请求， backlog=0 表示使用系统默认值
    if(listen(sock_fd, 0) != 0) 
    {
         printf("listen is error");
         return ERRCODE_FAIL;
    }
    printf("listen succeeded\n");
    
    // 等待客户端连接
    struct sockaddr_in cln_addr = {0};    // 客户端地址结构体
    socklen_t cln_addr_len = sizeof(cln_addr); // 地址长度
    
    // 接受客户端连接（阻塞调用）
    accept_fd = accept(sock_fd, (struct sockaddr *)&cln_addr, &cln_addr_len);
    if(accept_fd < 0)
    {
        printf("accept is error");
        return ERRCODE_FAIL;
    }
    printf("accept succeeded\n");
    
    return ERRCODE_SUCC; // 返回成功
}

/* 
 * AP模式示例初始化函数
 * @param argument: 线程参数（未使用）
 * @return int: 成功返回0，失败返回非0
 */
int ap_sample_init(const char *argument)
{
    argument = argument; // 避免未使用参数警告
    
    uapi_watchdog_disable(); // 禁用看门狗（避免在测试期间触发复位）
    
    uint32_t recv_len = 0;     // 单次接收的数据长度
    uint32_t total_size = 0;   // 总共接收的数据量（字节）
    uint8_t tcp_recv_buff1[1024] = {0}; // TCP接收缓冲区（1KB）
    
    /* 等待wifi子系统初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        osDelay(100); // 延时100ms
    }
    
    // 启动SoftAP功能
    if (example_softap_function() != 0) {
        printf("softap_function fail.\r\n");
        return -1;
    }   
    
    // 主循环：接收数据并统计速度
    while (1)
    {
        // 当接收到指定数量的数据包后，计算并显示传输速度
        if (g_recv_pkt_num == RECV_PKT_CNT) {
            g_count_after_get_us = uapi_tcxo_get_us(); // 获取当前时间戳（微秒）
           
            // 计算总耗时（秒），1秒 = 1000000微秒
            float time = (float)(g_count_after_get_us - g_count_before_get_us) / 1000000.0;          
            
            // 打印总时间和总数据量
            printf("time = %d.%d s, total_size = %d\r\n",
                get_float_int(time), get_float_dec(time), total_size);
            
            // 计算传输速度（字节/秒）
            float speed = (float)total_size / time;
            printf("speed = %d.%d B/s\r\n", get_float_int(speed), get_float_dec(speed));
            
            // 重置统计计数器
            g_recv_pkt_num = 0;
            total_size = 0;
            g_count_before_get_us = g_count_after_get_us; // 更新开始时间
        }
        // 第一次接收数据时记录开始时间
        else if (g_recv_pkt_num == 0) {
            g_count_before_get_us = uapi_tcxo_get_us(); // 记录开始时间戳
        } 
        
        // 从TCP连接接收数据
        recv_len = recv(accept_fd, tcp_recv_buff1, sizeof(tcp_recv_buff1), 0);
        
        if (recv_len > 0) {
            // 成功接收到数据
            total_size += recv_len;      // 累加总数据量
            recv_len = 0;                // 重置单次接收长度
            g_recv_pkt_num++;            // 增加数据包计数
        } else if (recv_len == 0) {
            // 连接已关闭
            printf("连接已关闭\n");
        } else {
            // 接收错误
            printf("recv 错误");
        }
    }
    
    return 0;
}

/* 
 * AP示例任务创建函数
 * 创建并启动ap_sample_init任务
 */
static void ap_sample(void)
{
    osThreadAttr_t attr; // 线程属性结构体
    attr.name = "sta_sample_task";        // 线程名称（注意这里名称是sta_sample_task，可能是笔误）
    attr.attr_bits = 0U;                  // 线程属性位
    attr.cb_mem = NULL;                   // 线程控制块内存地址
    attr.cb_size = 0U;                    // 线程控制块大小
    attr.stack_mem = NULL;                // 线程栈内存地址
    attr.stack_size = WIFI_TASK_STACK_SIZE; // 线程栈大小
    attr.priority = osPriorityNormal;     // 线程优先级
    
    // 创建新线程执行ap_sample_init函数
    if (osThreadNew((osThreadFunc_t)ap_sample_init, NULL, &attr) == NULL) {
        printf("Create sta_sample_task fail.\r\n"); // 创建线程失败
    }
    printf("Create sta_sample_task succ.\r\n"); // 创建线程成功
}

/* 应用运行宏：启动AP示例 */
app_run(ap_sample);