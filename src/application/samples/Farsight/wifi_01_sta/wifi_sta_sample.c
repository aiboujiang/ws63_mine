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
#include <stdio.h>

/* 定义常量 */
#define WIFI_SCAN_AP_LIMIT 64                   // 最大扫描AP数量限制
#define WIFI_CONN_STATUS_MAX_GET_TIMES 5        // 启动连接后，判断连接状态的最大尝试次数
#define DHCP_BOUND_STATUS_MAX_GET_TIMES 20      // 启动DHCP后，判断IP绑定状态的最大尝试次数
#define WIFI_STA_IP_MAX_GET_TIMES 5             // 判断是否获取到IP地址的最大尝试次数

/**
 * @brief  获取匹配的Wi-Fi网络信息
 * @param  expected_ssid: 期望连接的SSID名称
 * @param  expected_pwd: Wi-Fi密码
 * @param  expected_bss: 输出参数，存储匹配的网络配置信息
 * @retval 错误码：成功返回ERRCODE_SUCC，失败返回其他错误码
 */
static errcode_t example_get_match_network(const char *expected_ssid,
                                           const char *expected_key,
                                           wifi_sta_config_stru *expected_bss)
{
    uint32_t num = WIFI_SCAN_AP_LIMIT;          // 扫描到的AP数量
    uint32_t bss_index = 0;                     // AP索引

    /* 动态分配内存用于存储扫描结果 */
    uint32_t scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == NULL) {
        return ERRCODE_MALLOC; // 内存分配失败
    }

    // 清空扫描结果缓冲区
    memset_s(result, scan_len, 0, scan_len);
    
    // 获取Wi-Fi扫描信息
    if (wifi_sta_get_scan_info(result, &num) != ERRCODE_SUCC) {
        osal_kfree(result); // 释放内存
        return ERRCODE_FAIL; // 扫描失败
    }

    /* 在扫描结果中查找匹配的SSID */
    for (bss_index = 0; bss_index < num; bss_index++) {
        // 比较SSID长度和内容
        if (strlen(expected_ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(expected_ssid, result[bss_index].ssid, strlen(expected_ssid)) == 0) {
                break; // 找到匹配的AP，跳出循环
            }
        }
    }

    /* 检查是否找到匹配的AP */
    if (bss_index >= num) {
        osal_kfree(result); // 释放内存
        return ERRCODE_FAIL; // 未找到匹配的AP
    }
    
    /* 复制找到的AP信息到输出参数中 */
    // 复制SSID
    if (memcpy_s(expected_bss->ssid, WIFI_MAX_SSID_LEN, result[bss_index].ssid, WIFI_MAX_SSID_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    // 复制BSSID（MAC地址）
    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    // 设置安全类型
    expected_bss->security_type = result[bss_index].security_type;
    // 复制预共享密钥（密码）
    if (memcpy_s(expected_bss->pre_shared_key, WIFI_MAX_KEY_LEN, expected_key, strlen(expected_key)) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    // 设置IP获取方式为DHCP
    expected_bss->ip_type = DHCP;
    
    osal_kfree(result); // 释放扫描结果内存
    return ERRCODE_SUCC; // 成功找到并配置网络
}

/**
 * @brief  STA模式功能示例
 * @note   实现Wi-Fi STA模式的完整连接流程：扫描->连接->获取IP
 * @retval 错误码：成功返回ERRCODE_SUCC，失败返回ERRCODE_FAIL
 */
static errcode_t example_sta_function(void)
{
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0"; // 网络接口名称
    wifi_sta_config_stru expected_wifi = {0};         // Wi-Fi连接配置信息
    const char expected_ssid[] = "HQYJ_HQYJ_WS63";  // 目标SSID
    const char expected_key[] = "123456789";                  // Wi-Fi密码
    struct netif *netif_p = NULL;                    // 网络接口指针
    wifi_linked_info_stru wifi_status;               // Wi-Fi连接状态信息
    uint8_t index = 0;

    /* 启用STA模式 */
    if (wifi_sta_enable() != ERRCODE_SUCC) {
        printf("STA enable fail !\r\n");
        return ERRCODE_FAIL;
    }
    printf("STA enable succ.\r\n");
    
    /* 循环尝试连接Wi-Fi */
    do {
        printf("Start Scan !\r\n");
        osDelay(100); // 延时1秒（100*10ms）
        
        /* 执行Wi-Fi扫描 */
        if (wifi_sta_scan() != ERRCODE_SUCC) {
            printf("STA scan fail, try again !\r\n");
            continue; // 扫描失败，继续重试
        }

        osDelay(300); // 延时3秒，等待扫描完成

        /* 获取匹配的网络配置 */
        if (example_get_match_network(expected_ssid, expected_key, &expected_wifi) != ERRCODE_SUCC) {
            continue; // 未找到匹配网络，继续扫描
        }

        printf("STA start connect.\r\n");
        /* 尝试连接AP */
        if (wifi_sta_connect(&expected_wifi) != ERRCODE_SUCC) {
            continue; // 连接失败，继续重试
        }

        /* 检查连接状态，最多尝试WIFI_CONN_STATUS_MAX_GET_TIMES次 */
        for (index = 0; index < WIFI_CONN_STATUS_MAX_GET_TIMES; index++) {
            osDelay(50); // 延时500ms
            memset_s(&wifi_status, sizeof(wifi_linked_info_stru), 0, sizeof(wifi_linked_info_stru));
            // 获取连接信息
            if (wifi_sta_get_ap_info(&wifi_status) != ERRCODE_SUCC) {
                continue;
            }
            // 检查是否已连接
            if (wifi_status.conn_state == WIFI_CONNECTED) {
                break; // 连接成功，跳出循环
            }
        }
        // 如果连接成功，跳出外部循环
        if (wifi_status.conn_state == WIFI_CONNECTED) {
            break;
        }
    } while (1);

    printf("STA DHCP start.\r\n");
    /* 查找网络接口 */
    netif_p = netifapi_netif_find(ifname);
    if (netif_p == NULL) {
        return ERRCODE_FAIL; // 找不到网络接口
    }

    /* 启动DHCP客户端 */
    if (netifapi_dhcp_start(netif_p) != ERR_OK) {
        printf("STA DHCP Fail.\r\n");
        return ERRCODE_FAIL;
    }

    /* 等待DHCP绑定完成 */
    for (uint8_t i = 0; i < DHCP_BOUND_STATUS_MAX_GET_TIMES; i++) {
        osDelay(50); // 延时500ms
        if (netifapi_dhcp_is_bound(netif_p) == ERR_OK) {
            printf("STA DHCP bound success.\r\n");
            break;
        }
    }

    /* 检查是否获取到IP地址 */
    for (uint8_t i = 0; i < WIFI_STA_IP_MAX_GET_TIMES; i++) {
        osDelay(1); // 延时10ms
        // 检查IP地址是否为非0（已分配IP）
        if (netif_p->ip_addr.u_addr.ip4.addr != 0) {
            // 打印获取到的IP地址（格式：xxx.xxx.xxx.xxx）
            printf("STA IP %u.%u.%u.%u\r\n", 
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x000000ff),
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x0000ff00) >> 8,
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x00ff0000) >> 16,
                   (netif_p->ip_addr.u_addr.ip4.addr & 0xff000000) >> 24);
            
            // 显示DHCP客户端信息
            netifapi_netif_common(netif_p, dhcp_clients_info_show, NULL);
            
            // 再次启动DHCP（此处可能有误，通常不需要重复启动）
            if (netifapi_dhcp_start(netif_p) != 0) {
                printf("STA DHCP Fail.\r\n");
                return ERRCODE_FAIL;
            }

            /* 连接成功 */
            printf("STA connect success.\r\n");
            return ERRCODE_SUCC;
        }
    }

    printf("STA connect fail.\r\n");
    return ERRCODE_FAIL;
}

/**
 * @brief  STA示例初始化函数
 * @param  argument: 线程参数（未使用）
 * @retval 总是返回0
 */
int sta_sample_init(const char *argument)
{
    argument = argument; // 避免未使用参数警告
    
    /* 等待Wi-Fi模块初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        osDelay(10); // 延时100ms
    }
    
    // 执行STA功能示例
    example_sta_function();
    return 0;
}

/**
 * @brief  创建STA示例任务
 */
static void sta_sample(void)
{
    osThreadAttr_t attr;
    attr.name = "sta_sample_task";      // 任务名称
    attr.attr_bits = 0U;                // 属性位
    attr.cb_mem = NULL;                 // 控制块内存
    attr.cb_size = 0U;                  // 控制块大小
    attr.stack_mem = NULL;              // 堆栈内存
    attr.stack_size = 0x1000;           // 堆栈大小（4KB）
    attr.priority = osPriorityNormal;   // 任务优先级
    
    // 创建STA示例任务
    if (osThreadNew((osThreadFunc_t)sta_sample_init, NULL, &attr) == NULL) {
        printf("Create sta_sample_task fail.\r\n");
    }
    printf("Create sta_sample_task succ.\r\n");
}

/* 使用app_run宏启动示例程序 */
app_run(sta_sample);