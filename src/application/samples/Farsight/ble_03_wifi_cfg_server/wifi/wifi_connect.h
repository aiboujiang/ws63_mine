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

#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#define CONFIG_WIFI_SSID "HQYJ_H3863" // 要连接的WiFi 热点账号
#define CONFIG_WIFI_PWD "123456789" // 要连接的WiFi 热点密码


// 定义结构体用于存储解析后的WiFi信息
typedef struct {
    char ssid[30];  // 用于保存WiFi的名称(SSID)，最大长度127字符
    char pwd[30];   // 用于保存WiFi的密码(PWD)，最大长度127字符
    uint8_t wifi_flag[1];
} WifiInfo;
extern WifiInfo my_wifi_info;

errcode_t wifi_connect(void);

int parseWifiInfo(const char *input, WifiInfo *info) ;

#endif