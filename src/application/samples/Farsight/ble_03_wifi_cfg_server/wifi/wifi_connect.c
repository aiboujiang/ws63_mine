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

#include "lwip/netifapi.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "stdlib.h"
#include "uart.h"
#include "lwip/nettool/misc.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "wifi_connect.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"

#include "nv.h"
#include "nv_common_cfg.h"

#define WIFI_SCAN_AP_LIMIT 64
#define WIFI_CONN_STATUS_MAX_GET_TIMES 5 /* 启动连接之后，判断是否连接成功的最大尝试次数 */
#define DHCP_BOUND_STATUS_MAX_GET_TIMES 20 /* 启动DHCP Client端功能之后，判断是否绑定成功的最大尝试次数 */
#define WIFI_STA_IP_MAX_GET_TIMES 5 /* 判断是否获取到IP的最大尝试次数 */

WifiInfo my_wifi_info = { 0 };

/*****************************************************************************
  STA 扫描-关联 sample用例
*****************************************************************************/
static errcode_t example_get_match_network(const char *expected_ssid,
                                           const char *key,
                                           wifi_sta_config_stru *expected_bss)
{
    uint32_t num = WIFI_SCAN_AP_LIMIT; /* 64:扫描到的Wi-Fi网络数量 */
    uint32_t bss_index = 0;

    /* 获取扫描结果 */
    uint32_t scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == NULL) {
        return ERRCODE_MALLOC;
    }

    memset_s(result, scan_len, 0, scan_len);
    if (wifi_sta_get_scan_info(result, &num) != ERRCODE_SUCC) {
        osal_kfree(result);
        return ERRCODE_FAIL;
    }

    /* 筛选扫描到的Wi-Fi网络，选择待连接的网络 */
    for (bss_index = 0; bss_index < num; bss_index++) {
        if (strlen(expected_ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(expected_ssid, result[bss_index].ssid, strlen(expected_ssid)) == 0) {
                break;
            }
        }
    }

    /* 未找到待连接AP,可以继续尝试扫描或者退出 */
    if (bss_index >= num) {
        osal_kfree(result);
        return ERRCODE_FAIL;
    }
    /* 找到网络后复制网络信息和接入密码 */
    if (memcpy_s(expected_bss->ssid, WIFI_MAX_SSID_LEN, result[bss_index].ssid, WIFI_MAX_SSID_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, WIFI_MAX_KEY_LEN, key, strlen(key)) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    expected_bss->ip_type = DHCP; /* IP类型为动态DHCP获取 */
    osal_kfree(result);
    return ERRCODE_SUCC;
}

errcode_t wifi_connect(void)
{
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0"; /* WiFi STA 网络设备名 */
    wifi_sta_config_stru expected_bss = {0};         /* 连接请求信息 */
    struct netif *netif_p = NULL;
    wifi_linked_info_stru wifi_status;
    uint8_t index = 0;

    /* 等待wifi初始化完成 */
    while (wifi_is_wifi_inited() == 0) {
        (void)osDelay(10); /* 10: 延时100ms  */
           PRINT("wifi_is_wifi_inited!\r\n");
    }
    /* 创建STA */
    if (wifi_sta_enable() != ERRCODE_SUCC) {
        PRINT("STA enbale fail !\r\n");
        return ERRCODE_FAIL;
    }
       PRINT("wifi_sta_enable!\r\n");
    do {
        PRINT("Start Scan !\r\n");
        (void)osDelay(100); /* 100: 延时1s  */
        /* 启动STA扫描 */
        if (wifi_sta_scan() != ERRCODE_SUCC) {
            PRINT("STA scan fail, try again !\r\n");
            continue;
        }

        (void)osDelay(300); /* 300: 延时100ms  */

        /* 获取待连接的网络 */
        if (example_get_match_network(my_wifi_info.ssid, my_wifi_info.pwd, &expected_bss) != ERRCODE_SUCC) {
            PRINT("Can not find AP, try again !\r\n");
            continue;
        }

        PRINT("STA start connect.\r\n");
        /* 启动连接 */
        if (wifi_sta_connect(&expected_bss) != ERRCODE_SUCC) {
            continue;
        }

        /* 检查网络是否连接成功 */
        for (index = 0; index < WIFI_CONN_STATUS_MAX_GET_TIMES; index++) {
            (void)osDelay(50); /* 50: 延时5s  */
            memset_s(&wifi_status, sizeof(wifi_linked_info_stru), 0, sizeof(wifi_linked_info_stru));
            if (wifi_sta_get_ap_info(&wifi_status) != ERRCODE_SUCC) {
                continue;
            }
            if (wifi_status.conn_state == WIFI_CONNECTED) {
                break;
            }
        }
        if (wifi_status.conn_state == WIFI_CONNECTED) {
            break; /* 连接成功退出循环 */
        }
    } while (1);

    /* DHCP获取IP地址 */
    netif_p = netifapi_netif_find(ifname);
    if (netif_p == NULL) {
        return ERRCODE_FAIL;
    }

    if (netifapi_dhcp_start(netif_p) != ERR_OK) {
        PRINT("STA DHCP Fail.\r\n");
        return ERRCODE_FAIL;
    }

    for (uint8_t i = 0; i < DHCP_BOUND_STATUS_MAX_GET_TIMES; i++) {
        (void)osDelay(50); /* 50: 延时500ms  */
        if (netifapi_dhcp_is_bound(netif_p) == ERR_OK) {
            PRINT("STA DHCP bound success.\r\n");
            break;
        }
    }

    for (uint8_t i = 0; i < WIFI_STA_IP_MAX_GET_TIMES; i++) {
        osDelay(1); /* 1: 延时10ms  */
        if (netif_p->ip_addr.u_addr.ip4.addr != 0) {
            PRINT("STA IP %u.%u.%u.%u\r\n", (netif_p->ip_addr.u_addr.ip4.addr & 0x000000ff),
                  (netif_p->ip_addr.u_addr.ip4.addr & 0x0000ff00) >> 8,   /* 8: 移位  */
                  (netif_p->ip_addr.u_addr.ip4.addr & 0x00ff0000) >> 16,  /* 16: 移位  */
                  (netif_p->ip_addr.u_addr.ip4.addr & 0xff000000) >> 24); /* 24: 移位  */

            /* 连接成功 */
            PRINT("STA connect success.\r\n");
            return ERRCODE_SUCC;
        }
    }
    PRINT("STA connect fail.\r\n");
    return ERRCODE_FAIL;
}

/**
 * 解析WiFi信息字符串，提取SSID和密码
 * 
 * @param input  待解析的字符串，格式应为[SSID:"...", PWD:"..."]
 * @param info   指向WifiInfo结构体的指针，用于存储解析结果
 * @return       0表示解析成功，非0值表示解析失败（具体错误见注释）
 * 
 * 错误码说明：
 *   -1: 输入参数无效（input或info为NULL）
 *   -2: 字符串格式错误（未以[开头或未以]结尾）
 *   -3: 未找到SSID字段
 *   -4: SSID格式错误（缺少闭合引号）
 *   -5: SSID长度超过缓冲区大小
 *   -6: 未找到PWD字段
 *   -7: PWD格式错误（缺少闭合引号或位置不正确）
 *   -8: PWD长度超过缓冲区大小
 */
int parseWifiInfo(const char *input, WifiInfo *info) {
    // 检查输入参数有效性
    if (input == NULL || info == NULL) {
        return -1;  // 输入参数为空指针
    }
    
    // 清空输出结构体
    memset(info, 0, sizeof(WifiInfo));
    
    // 获取输入字符串长度并检查基本格式
    size_t len = strlen(input);  // 使用size_t类型
    if (input[0] != '[' ) {
        return -2;  // 字符串格式不符合要求
    }
    
    // 查找SSID字段的起始位置
    const char *ssidStart = strstr(input, "SSID:\"");
    if (ssidStart == NULL) {
        return -3;  // 未找到SSID标识
    }
    ssidStart += 6;  // 跳过"SSID:\""
    
    // 查找SSID值的结束引号
    const char *ssidEnd = strchr(ssidStart, '"');
    if (ssidEnd == NULL) {
        return -4;  // SSID值缺少闭合引号
    }
    
    // 计算SSID长度（使用size_t无符号类型）
    size_t ssidLen = ssidEnd - ssidStart;
    if (ssidLen >= sizeof(info->ssid)) {  // 现在两边都是无符号类型
        return -5;  // SSID过长
    }
    strncpy(info->ssid, ssidStart, ssidLen);
    info->ssid[ssidLen] = '\0';
    
    // 查找PWD字段
    const char *pwdStart = strstr(ssidEnd, "PWD:\"");
    if (pwdStart == NULL) {
        return -6;  // 未找到PWD标识
    }
    pwdStart += 5;  // 跳过"PWD:\""
    
    // 查找密码值的结束引号
    const char *pwdEnd = strchr(pwdStart, '"');
    if (pwdEnd == NULL || pwdEnd >= input + len - 1) {
        return -7;  // 密码格式错误
    }
    
    // 计算密码长度（使用size_t无符号类型）
    size_t pwdLen = pwdEnd - pwdStart;
    if (pwdLen >= sizeof(info->pwd)) {  // 现在两边都是无符号类型
        return -8;  // 密码过长
    }
    strncpy(info->pwd, pwdStart, pwdLen);
    info->pwd[pwdLen] = '\0';
    
    return 0;  // 解析成功
}