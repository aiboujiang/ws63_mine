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

#ifndef MQTT_H
#define MQTT_H
#include "MQTTClientPersistence.h"
#include "MQTTClient.h"
#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "errcode.h"
#include "common_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lvgl_light_demo.h"

#define SERVER_IP_ADDR   "aggvvcf.iot.gz.baidubce.com"           //服务器地址
#define SERVER_IP_PORT   "1883"                                  //端口号
#define g_username       "thingidp@aggvvcf|fs_embsim|0|MD5"      //用户名
#define g_password       "fc1c8005e5ad7fc373e5510873cef467"      //密码
#define CLIENT_ID        "light"                                 //设备名称


#define MQTT_CMDTOPIC_SUB    "fsembsim/fs_embsim/inteligentlight/sub"
#define MQTT_DATATOPIC_PUB   "fsembsim/fs_embsim/inteligentlight/pub"                 // 属性上报topic

#define KEEP_ALIVE_INTERVAL 120
#define DELAY_TIME_MS 100

extern uint8_t mqtt_conn;
extern uint8_t g_cmdFlag;
extern char g_response_id[100]; // 保存命令id缓冲区
errcode_t mqtt_connect(void);
int mqtt_publish(const char *topic, char *msg);
#endif