#ifndef __SXTC_MQTT_DEMO_H__
#define __SXTC_MQTT_DEMO_H__
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
#include "los_typedef.h"

// #define DEBUG

#define SERVER_IP_PORT "%s:4321"
#define MQTT_TOPIC_SUB "$hqyj/user/sub/%s"	// 需要修改
#define MQTT_TOPIC_PUB "$hqyj/user/pub/%s"	// 需要修改

int mqtt_publish(const char *topic, char *g_msg);
errcode_t mqtt_connect(void);
#endif


