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

#include "mqtt.h"
#include "cJSON.h"

uint8_t mqtt_conn;
char g_send_buffer[512] = {0}; // 发布数据缓冲区
char g_response_id[100] = {0}; // 保存命令id缓冲区

MQTTClient client;
volatile MQTTClient_deliveryToken deliveredToken;
extern int MQTTClient_init(void);
extern uint16_t lightofflevel;
/* 回调函数，处理连接丢失 */
void connlost(void *context, char *cause)
{
    unused(context);
    printf("Connection lost: %s\n", cause);
}
int mqtt_subscribe(const char *topic)
{
    printf("subscribe start\r\n");
    MQTTClient_subscribe(client, topic, 1);
    return 0;
}

int mqtt_publish(const char *topic, char *msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int ret = 0;
    pubmsg.payload = msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    // printf("[payload]:  %s, [topic]: %s\r\n", msg, topic);
    ret = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("mqtt publish failed\r\n");
        return ret;
    }

    return ret;
}

/* 回调函数，处理消息到达 */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);

    printf("Message with token value %d delivery confirmed\n", dt);

    deliveredToken = dt;
}

/* 回调函数，处理接收到的消息 */
int messageArrived(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    unused(context);
    unused(topic_len);
    unused(topic_name);
    printf("[Message]: %s\n", (char *)message->payload);

    cJSON *root = cJSON_Parse((char *)message->payload);
    if (root == NULL) {
        return 1;
    }
    // 检查是否是设备状态消息
    cJSON *myDevice = cJSON_GetObjectItem(root, "myDevice");
    if (myDevice != NULL) {
        printf("设备状态: %s\n",myDevice->valuestring);
        if(strcmp(myDevice->valuestring,"online") == 0)
        {
  
            My_Mode.WeChatOnline = CONNECTED;
        }
        else if(strcmp(myDevice->valuestring,"offline") == 0)
        {
            My_Mode.WeChatOnline = DISCONNECTED;
        }
        cJSON_Delete(root);
        return 1;
    }
   
    // 提取sensor字段
    cJSON *sensor = cJSON_GetObjectItem(root, "sensor");
    if (sensor != NULL) {
        printf("传感器类型: %s\n", sensor->valuestring);
        if(strcmp(sensor->valuestring,"light") == 0)
        {
            // 提取data_val字段
            cJSON *data_val = cJSON_GetObjectItem(root, "data_val");
            if (data_val != NULL) 
            {
                // 根据数据类型处理不同的值
                printf("数据值(数字): %d\n", data_val->valueint);
                if (data_val->valueint) {
                    My_Sensor.led_state = LIGHTON;
                }else{
                    My_Sensor.led_state = LIGHTOFF;
                }
            }      
        }
        else if(strcmp(sensor->valuestring,"threshold") == 0)
        {
            // 提取data_val字段
            cJSON *data_val = cJSON_GetObjectItem(root, "data_val");
            if (data_val != NULL) 
            {
                printf("数据值(数字): %d\n", data_val->valueint);
                lightofflevel = data_val->valueint;
            }      
        }
        else if(strcmp(sensor->valuestring,"smart") == 0)
        {
            // 提取data_val字段
            cJSON *data_val = cJSON_GetObjectItem(root, "data_val");
            if (data_val != NULL) 
            {
                // 根据数据类型处理不同的值
                printf("数据值(布尔): %s\n", data_val->valueint ? "true" : "false");
                if(data_val->valueint){
                   My_Mode.CurrentMode = SMART;
                }else{
                    My_Mode.CurrentMode = USR;
                }
            }      
        }

    }       

    memset((char *)message->payload, 0, message->payloadlen);
    cJSON_Delete(root);
    MQTTClient_freeMessage((MQTTClient_message **)&message);
    MQTTClient_free(topic_name);
    return 1; // 表示消息已被处理
}

errcode_t mqtt_connect(void)
{
    int ret;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    /* 初始化MQTT客户端 */

    MQTTClient_init();
    /* 创建 MQTT 客户端 */
    ret = MQTTClient_create(&client, SERVER_IP_ADDR, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("Failed to create MQTT client, return code %d\n", ret);
        return ERRCODE_FAIL;
    }
    conn_opts.keepAliveInterval = KEEP_ALIVE_INTERVAL;
    conn_opts.cleansession = 1;
    conn_opts.username = g_username;
    conn_opts.password = g_password;

    // 绑定回调函数
    MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);

    // 尝试连接
    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        MQTTClient_destroy(&client); // 连接失败时销毁客户端
        return ERRCODE_FAIL;
    }
    // 订阅MQTT主题
    osDelay(DELAY_TIME_MS);
    ret = mqtt_subscribe(MQTT_CMDTOPIC_SUB);
    if (ret < 0) {
        printf("subscribe topic error, result %d\n", ret);
    }
    My_Mode.mqttconnetState = 1;
    printf(" My_Mode.mqttconnetState %d\n",  My_Mode.mqttconnetState);
    return ERRCODE_SUCC;
}

