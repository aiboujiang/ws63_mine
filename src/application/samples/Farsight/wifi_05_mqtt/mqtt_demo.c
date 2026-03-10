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

#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_def.h"
#include "MQTTClientPersistence.h"
#include "MQTTClient.h"
#include "errcode.h"
#include "wifi/wifi_connect.h"
osThreadId_t mqtt_init_task_id; // mqtt订阅数据任务

/* MQTT 服务器配置 */
#define SERVER_IP_ADDR "192.168.125.230"  // MQTT 代理服务器（Broker）的 IP 地址
#define SERVER_IP_PORT 1883               // MQTT 协议默认端口（未加密通信）
#define CLIENT_ID "ADMIN"                 // 客户端标识符，用于在Broker上唯一标识此设备

/* MQTT 主题定义 */
#define MQTT_TOPIC_SUB "subTopic"         // 订阅的主题名称，用于接收消息
#define MQTT_TOPIC_PUB "pubTopic"         // 发布的主题名称，用于发送消息

/* 全局变量声明 */
char *g_msg = "hello!";                   // 要发送的MQTT消息内容
MQTTClient client;                        // MQTT客户端对象
volatile MQTTClient_deliveryToken deliveredToken; // 消息交付令牌，用于确认消息发布状态
extern int MQTTClient_init(void);         // 声明外部初始化函数

/* 
 * 回调函数：处理消息发布确认
 * @param context: 用户上下文（未使用）
 * @param dt: 消息交付令牌
 */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context); // 避免未使用参数警告
    printf("Message with token value %d delivery confirmed\n", dt); // 打印消息确认信息
    deliveredToken = dt; // 更新全局交付令牌
}

/* 
 * 回调函数：处理接收到的消息
 * @param context: 用户上下文（未使用）
 * @param topicname: 消息到达的主题名称
 * @param topiclen: 主题名称长度（未使用）
 * @param message: 消息内容结构体指针
 * @return int: 总是返回1，表示消息已被成功处理
 */
int messageArrived(void *context, char *topicname, int topiclen, MQTTClient_message *message)
{
    unused(context);    // 避免未使用参数警告
    unused(topiclen);   // 避免未使用参数警告
    
    // 打印接收到的消息信息
    printf("Message arrived on topic: %s\n", topicname);
    printf("Message: %.*s\n", message->payloadlen, (char *)message->payload);
    MQTTClient_freeMessage((MQTTClient_message **)&message);
    MQTTClient_free(topicname);
    return 1; // 返回1表示消息已被处理，Broker可以释放消息资源
}

/* 
 * 回调函数：处理连接丢失事件
 * @param context: 用户上下文（未使用）
 * @param cause: 连接丢失的原因描述
 */
void connlost(void *context, char *cause)
{
    unused(context); // 避免未使用参数警告
    printf("Connection lost: %s\n", cause); // 打印连接丢失原因
}

/* 
 * 消息订阅函数
 * @param topic: 要订阅的主题名称
 * @return int: 总是返回0（成功）
 */
int mqtt_subscribe(const char *topic)
{
    printf("subscribe start\r\n");
    // 订阅指定主题，QoS等级设置为1（至少一次）
    MQTTClient_subscribe(client, topic, 1);
    return 0;
}

/* 
 * 消息发布函数
 * @param topic: 要发布到的主题名称
 * @param g_msg: 要发布的消息内容
 * @return int: 发布操作的结果代码
 */
int mqtt_publish(const char *topic, char *g_msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer; // 初始化消息结构体
    MQTTClient_deliveryToken token; // 消息交付令牌
    int ret = 0; // 返回值
    
    // 设置消息内容
    pubmsg.payload = g_msg;                // 消息负载数据
    pubmsg.payloadlen = (int)strlen(g_msg); // 消息负载长度
    pubmsg.qos = 1;                        // 服务质量等级：1（至少一次）
    pubmsg.retained = 0;                   // 保留消息标志：0（不保留）
    
    // 发布消息
    ret = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("mqtt_publish failed\r\n"); // 发布失败提示
    }
    
    printf("mqtt_publish(), the payload is %s, the topic is %s\r\n", g_msg, topic);
    return ret;
}

/* 
 * MQTT连接函数
 * @return errcode_t: 连接成功返回ERRCODE_SUCC，失败返回ERRCODE_FAIL
 */
static errcode_t mqtt_connect(void)
{
    int ret; // 操作返回值
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer; // 初始化连接选项
    
    /* 初始化MQTT客户端库 */
    MQTTClient_init();
    
    /* 创建 MQTT 客户端实例 */
    // 参数说明：&client(客户端对象), SERVER_IP_ADDR(服务器地址), CLIENT_ID(客户端ID), 
    // MQTTCLIENT_PERSISTENCE_NONE(不持久化), NULL(持久化上下文)
    ret = MQTTClient_create(&client, SERVER_IP_ADDR, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("Failed to create MQTT client, return code %d\n", ret);
        return ERRCODE_FAIL; // 创建客户端失败
    }
    
    /* 配置连接选项 */
    conn_opts.keepAliveInterval = 120; /* 120: 保活时间（秒），客户端定期发送心跳包保持连接 */
    conn_opts.cleansession = 1;        /* 1: 清理会话标志，设置为1表示不持久化会话状态 */
    
    // 设置MQTT回调函数
    // 参数说明：client(客户端对象), NULL(上下文), connlost(连接丢失回调), 
    // messageArrived(消息到达回调), delivered(消息交付确认回调)
    MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);

    // 尝试连接到MQTT代理服务器
    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", ret);
        MQTTClient_destroy(&client); // 连接失败时销毁客户端，释放资源
        return ERRCODE_FAIL;
    }
    
    printf("Connected to MQTT broker!\n");
    osDelay(200); /* 200: 延时2s，等待连接稳定 */
    
    // 订阅MQTT主题，开始接收消息
    mqtt_subscribe(MQTT_TOPIC_SUB);
    
    // 主循环：定期发布消息
    while (1) {
        osDelay(100); /* 100: 延时1s，控制发布频率 */
        mqtt_publish(MQTT_TOPIC_PUB, g_msg); // 发布消息到指定主题
    }

    return ERRCODE_SUCC; // 理论上不会执行到这里
}

/* 
 * MQTT初始化任务函数
 * @param argument: 线程参数（未使用）
 */
void mqtt_init_task(const char *argument)
{
    unused(argument); // 避免未使用参数警告
    
    // 第一步：连接WiFi网络
    wifi_connect();
    osDelay(200); /* 200: 延时2s，等待网络连接稳定 */
    
    // 第二步：建立MQTT连接并开始通信
    mqtt_connect();
}

/* 
 * 网络WiFi MQTT示例主函数
 * 创建并启动MQTT初始化任务
 */
static void network_wifi_mqtt_example(void)
{
    printf("Enter network_wifi_mqtt_example()!");

    // 配置新任务的属性
    osThreadAttr_t options;
    options.name = "mqtt_init_task";     // 任务名称
    options.attr_bits = 0;               // 属性位
    options.cb_mem = NULL;               // 控制块内存地址
    options.cb_size = 0;                 // 控制块大小
    options.stack_mem = NULL;            // 栈内存地址
    options.stack_size = 0x3000;         // 栈大小（12KB）
    options.priority = osPriorityNormal; // 任务优先级

    // 创建并启动MQTT初始化任务
    mqtt_init_task_id = osThreadNew((osThreadFunc_t)mqtt_init_task, NULL, &options);
    if (mqtt_init_task_id != NULL) {
        printf("ID = %d, Create mqtt_init_task_id is OK!", mqtt_init_task_id);
    }
}

/* 应用运行宏：启动网络WiFi MQTT示例 */
app_run(network_wifi_mqtt_example);