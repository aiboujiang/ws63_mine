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

#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "stdlib.h"
#include "uart.h"
#include "lwip/nettool/misc.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "wifi_device.h"
#include "wifi_event.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"
#include "tcxo.h"
#include "sle_client.h"
#include "wifi_tcp_ap.h"
#include "systick.h"
#include "watchdog.h"
#include "sle_ssap_client.h"

unsigned long g_msg_queue;   // 消息队列句柄
unsigned int g_msg_rev_size; // 消息大小

uint8_t msg_data[QUEUE_DATA_MAX_LEN] = {0}; // 消息队列数据缓存

// 全局socket文件描述符
int sock_fd;   // 服务器socket
int accept_fd; // 客户端连接socket

// SoftAP功能初始化函数
errcode_t example_softap_function(td_void)
{
    /* SoftAp接口的信息 */
    td_char ssid[WIFI_MAX_SSID_LEN] = WIFI_NAME;
    td_char pre_shared_key[WIFI_MAX_KEY_LEN] = WIFI_KEY;
    softap_config_stru hapd_conf = {0};               // 基础配置结构体
    softap_config_advance_stru config = {0};          // 高级配置结构体
    td_char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "ap0"; /* 创建的SoftAp接口名 */
    struct netif *netif_p = TD_NULL;                  // 网络接口指针
    ip4_addr_t st_gw;                                 // 网关地址
    ip4_addr_t st_ipaddr;                             // IP地址
    ip4_addr_t st_netmask;                            // 子网掩码

    // 设置IP地址、子网掩码和网关
    IP4_ADDR(&st_ipaddr, 192, 168, 5, 1);    /* IP地址设置：192.168.5.1 */
    IP4_ADDR(&st_netmask, 255, 255, 255, 0); /* 子网掩码设置：255.255.255.0 */
    IP4_ADDR(&st_gw, 192, 168, 5, 2);        /* 网关地址设置：192.168.5.2 */

    /* 配置SoftAp基本参数 */
    (td_void) memcpy_s(hapd_conf.ssid, sizeof(hapd_conf.ssid), ssid, sizeof(ssid));
    (td_void) memcpy_s(hapd_conf.pre_shared_key, WIFI_MAX_KEY_LEN, pre_shared_key, WIFI_MAX_KEY_LEN);
    hapd_conf.security_type = 3; /* 3: 加密方式设置为WPA_WPA2_PSK */
    hapd_conf.channel_num = 6;   /* 6：工作信道设置为6信道 */
    hapd_conf.wifi_psk_type = 0;

    /* 配置SoftAp网络参数 */
    config.beacon_interval = 100; /* 100：Beacon周期设置为100ms */
    config.dtim_period = 2;       /* 2：DTIM周期设置为2 */
    config.gi = 0;                /* 0：short GI默认关闭 */
    config.group_rekey = 86400;   /* 86400：组播秘钥更新时间设置为1天 */
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
        (td_void) wifi_softap_disable();
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

    printf("SoftAp start success.\r\n");

    // 创建TCP服务器socket
    printf("create socket start! \r\n");
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) != ERRCODE_SUCC) {
        printf("create socket failed!\r\n");
        return ERRCODE_FAIL;
    }
    printf("create socket succ!\r\n");

    /* 初始化预连接的服务端地址 */
    struct sockaddr_in send_addr;
    send_addr.sin_family = AF_INET;                  // IPv4地址族
    send_addr.sin_port = htons(CONFIG_SERVER_PORT);  // 端口号（主机字节序转网络字节序）
    send_addr.sin_addr.s_addr = inet_addr(STACK_IP); // IP地址

    // 绑定socket到指定地址和端口
    if (bind(sock_fd, (struct sockaddr *)&send_addr, sizeof(send_addr)) != 0) {
        printf("bind is error");
        return ERRCODE_FAIL;
    }
    printf("bind succeeded\n");

    // 监听socket，等待客户端连接
    if (listen(sock_fd, 0) != 0) {
        printf("listen is error");
        return ERRCODE_FAIL;
    }
    printf("listen succeeded\n");

    // 接受客户端连接
    struct sockaddr_in cln_addr = {0};         // 客户端地址结构
    socklen_t cln_addr_len = sizeof(cln_addr); // 地址结构长度
    accept_fd = accept(sock_fd, (struct sockaddr *)&cln_addr, &cln_addr_len);
    if (accept_fd < 0) {
        printf("accept is error");
        return ERRCODE_FAIL;
    }
    printf("accept succeeded\n");

    return ERRCODE_SUCC;
}

// TCP数据发送任务
int tcp_send_tsask(const char *argument)
{
    unused(argument); // 未使用参数
    while (1) {
        g_msg_rev_size = sizeof(msg_data);
        // 从消息队列读取数据（阻塞等待）
        int msg_ret = osal_msg_queue_read_copy(g_msg_queue, msg_data, &g_msg_rev_size, OSAL_WAIT_FOREVER);
        if (msg_ret != OSAL_SUCCESS) {
            printf("Msg queue read copy fail.Ret:%d\n", msg_ret);
        }

        // 如果收到有效数据，通过TCP发送
        if (g_msg_rev_size != 0) {
            /* 发送数据到客户端 */
            send(accept_fd, msg_data, g_msg_rev_size, 0);
        }
        osDelay(1);
    }
    return 0;
}

// AP示例初始化主任务
int ap_sample_init(const char *argument)
{
    argument = argument;     // 未使用参数
    uapi_watchdog_disable(); // 禁用看门狗

    uint32_t recv_len = 0;
    uint8_t tcp_recv_buff1[100] = {0}; // TCP接收缓冲区

    /* 等待WiFi初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        osDelay(100); // 延时100ms
    }

    // 启动SoftAP功能
    if (example_softap_function() != 0) {
        printf("softap_function fail.\r\n");
        return ERRCODE_FAIL;
    }

    // 创建TCP发送任务
    osal_task *task_handle = NULL;
    osal_kthread_lock(); // 内核线程锁
    task_handle =
        osal_kthread_create((osal_kthread_handler)tcp_send_tsask, 0, "tcp_send_tsask", WIFI_STA_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WIFI_STA_TASK_PRIO); // 设置任务优先级
    }
    osal_kthread_unlock(); // 解锁

    printf("Create ap_sample_init succ.\r\n");

    // 主循环：接收TCP数据并处理控制命令
    while (1) {
        // 接收TCP数据
        recv_len = recv(accept_fd, tcp_recv_buff1, sizeof(tcp_recv_buff1), 0);

        if (recv_len > 0) {
            printf("%s\n", tcp_recv_buff1); // 打印接收到的数据
            for (int i = 0; i < g_num_conn; i++) {
                ssapc_write_param_t param = {0};
                param.handle = g_sle_send_param_arr[i].g_sle_send_param.handle;
                param.type = SSAP_PROPERTY_TYPE_VALUE;
                param.data_len = recv_len;
                param.data = tcp_recv_buff1;
                ssapc_write_req(0, g_sle_send_param_arr[i].conn_id, &param);
                osal_msleep(1000);
            }
            // 清空缓冲区
            memset(tcp_recv_buff1, 0, 100);
            recv_len = 0;

        } else if (recv_len == 0) {
            printf("连接已关闭\n"); // 连接关闭
            // 接受客户端连接
            struct sockaddr_in cln_addr = {0};         // 客户端地址结构
            socklen_t cln_addr_len = sizeof(cln_addr); // 地址结构长度
            accept_fd = accept(sock_fd, (struct sockaddr *)&cln_addr, &cln_addr_len);
            if (accept_fd < 0) {
                printf("accept is error");
                return ERRCODE_FAIL;
            }
            printf("accept succeeded\n");
        } else {
            printf("recv 错误"); // 接收错误
            return ERRCODE_SUCC;
        }
    }

    return ERRCODE_SUCC;
}

// AP示例入口函数
static void ap_sample(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    // 创建消息队列
    int ret = osal_msg_queue_create("sle_msg", MSG_QUEUE_NUMBER, &g_msg_queue, 0, QUEUE_DATA_MAX_LEN);
    if (ret != OSAL_SUCCESS) {
        osal_printk("create queue failure!,error:%x\n", ret);
    } else {
        osal_printk("create queue success!,id:%d\n", g_msg_queue);
    }
    // 创建AP初始化任务

    task_handle =
        osal_kthread_create((osal_kthread_handler)ap_sample_init, 0, "ap_sample_init", WIFI_STA_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WIFI_STA_TASK_PRIO);
    }
    osal_kthread_unlock();

    printf("Create ap_sample_init succ.\r\n");
}

/* Run the sample. */
app_run(ap_sample); // 启动应用