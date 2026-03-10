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
#include "wifi_connect.h"
#include "upg_porting.h"
#include "hal_bsp_nfc_to_wifi.h"
#include "cJSON.h"
osThreadId_t mqtt_init_task_id; // mqtt订阅数据任务
osThreadId_t process;

MQTTClient client;

volatile MQTTClient_deliveryToken deliveredToken;
extern int MQTTClient_init(void);

/* 回调函数，处理消息到达 */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    unused(context);
#if defined(DEBUG)
    printf("Message with token value %d delivery confirmed\n", dt);
#endif
    deliveredToken = dt;
}

/* 回调函数，处理接收到的消息 */
int messageArrived(void *context, char *topicname, int topiclen, MQTTClient_message *message)
{
    unused(context);
    unused(topiclen);
    unused(topicname);
    unused(message);
 #if defined(DEBUG)
    printf("Message arrived on topic: %s\n", topicname);
    printf("Message:%d %s\n", message->payloadlen, (char *)message->payload);
 #endif
  printf("Message:%d %s\n", message->payloadlen, (char *)message->payload);
     // 解析 JSON
    cJSON *json = cJSON_Parse((char *)message->payload);
    if (json == NULL) {
        return 1;
    }
    
    // 提取各个字段
    cJSON *deviceName = cJSON_GetObjectItemCaseSensitive(json, "deviceName");
    cJSON *state = cJSON_GetObjectItemCaseSensitive(json, "state");
    cJSON *track = cJSON_GetObjectItemCaseSensitive(json, "track");
    cJSON *shift = cJSON_GetObjectItemCaseSensitive(json, "shift");

    cJSON *led = cJSON_GetObjectItemCaseSensitive(json, "led");
    cJSON *beep = cJSON_GetObjectItemCaseSensitive(json, "beep");

    // 检查字段是否存在并提取值
    if (cJSON_IsString(deviceName) && (deviceName->valuestring != NULL)) {
        printf("deviceName: %s\n", deviceName->valuestring);
        if(strcmp(deviceName->valuestring,car_name) != 0)
        {
            printf("Not this device\n");
            return 1;
        }
    }
    if(track != NULL)
    {
        if (cJSON_IsNumber(track)) {
            //printf("track: %d\n", track->valueint);
            if ( track->valueint == 0 ) { // 关闭寻迹模式
                systemValue.auto_track_flag = 0;
                tmp_io.bit.p3 = 1;
                PCF8574_Write(tmp_io.all);
            } else if (track->valueint == 1 ) { // 开启寻迹模式
                systemValue.car_speed = 0;
                systemValue.auto_track_flag = 1;
                tmp_io.bit.p3 = 0;
                PCF8574_Write(tmp_io.all);
            }
        }

    }
    if(!systemValue.auto_track_flag)
    {
        if(state != NULL)
        {
            if (cJSON_IsNumber(state)) {
                //printf("state: %d (数字)\n", state->valueint);
                systemValue.car_status =  state->valueint;
            }
        }
        if(shift != NULL)
        {
             if (cJSON_IsNumber(shift)) {
                //printf("shift: %d\n", shift->valueint);
                if(shift->valueint<3 && shift->valueint>=0)
                    systemValue.car_speed = shift->valueint;
            }
        }
    }
    if (led != NULL)
    {
        /* code */
        if (cJSON_IsNumber(led)) 
        {   
            //printf("led: %d\n", led->valueint);
            uapi_gpio_set_val(USER_LED,!(led->valueint));//led接了上拉，所以val值相反
            systemValue.car_led = led->valueint;
        }
           
    }
    if (beep != NULL)
    {
        /* code */
        if (cJSON_IsNumber(beep)) 
        { 
            //printf("beep: %d\n", beep->valueint);
            uapi_gpio_set_val(USER_BEEP,beep->valueint);
            systemValue.car_beep = beep->valueint;
        }
    }
    
    // 清理内存
    cJSON_Delete(json);
    MQTTClient_freeMessage((MQTTClient_message **)&message);
    MQTTClient_free(topicname);
    return 1; // 表示消息已被处理
}

/* 回调函数，处理连接丢失 */
void connlost(void *context, char *cause)
{
    unused(context);
    unused(cause);
#if defined(DEBUG)   
    printf("Connection lost: %s\n", cause);
#endif
    upg_reboot();
}
/* 消息订阅 */
int mqtt_subscribe(const char *topic)
{
#if defined(DEBUG)
    printf("subscribe start\r\n");
#endif
    MQTTClient_subscribe(client, topic, 1);
    return 0;
}

int mqtt_publish(const char *topic, char *g_msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int ret = 0;
    pubmsg.payload = g_msg;
    pubmsg.payloadlen = (int)strlen(g_msg);
    pubmsg.qos = 1;
    pubmsg.retained = 0;
    ret = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("mqtt_publish failed\r\n");
    }
    //MQTTClient_freeMessage(&pubmsg); 
#if defined(DEBUG)
    printf("mqtt_publish(), the payload is %s, the topic is %s\r\n", g_msg, topic);
#endif
    return ret;
}
errcode_t mqtt_connect(void)
{
    int ret;
    char clientid[40]={0};
    char ip_port[40]={0};
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    /* 初始化MQTT客户端 */
    MQTTClient_init();
    memset(ip_port,0,sizeof(ip_port));
    sprintf(ip_port, SERVER_IP_PORT,host_ip );
    //printf("----%s\n",ip_port);
    /* 创建 MQTT 客户端 */
    ret = MQTTClient_create(&client, ip_port, car_name, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (ret != MQTTCLIENT_SUCCESS) {
        printf("Failed to create MQTT client, return code %d\n", ret);
        return ERRCODE_FAIL;
    }
    conn_opts.keepAliveInterval = 120; /* 120: 保活时间  */
    conn_opts.cleansession = 1;

    // 绑定回调函数
    MQTTClient_setCallbacks(client, NULL, connlost, messageArrived, delivered);

    // 尝试连接
    if ((ret = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", ret);
        MQTTClient_destroy(&client); // 连接失败时销毁客户端
        upg_reboot();
        return ERRCODE_FAIL;
    }

#if defined(DEBUG)
    printf("Connected to MQTT broker!\n");
#endif
    osDelay(100);
    // 订阅MQTT主题
    sprintf(clientid, MQTT_TOPIC_SUB, car_name);
    mqtt_subscribe(clientid);
    osDelay(100); /* 100: 延时1s  */

    return ERRCODE_SUCC;
}




