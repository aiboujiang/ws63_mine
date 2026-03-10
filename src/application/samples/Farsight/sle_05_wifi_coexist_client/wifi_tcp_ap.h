#ifndef WIFI_TCP_AP_H
#define WIFI_TCP_AP_H
#include "cmsis_os2.h"
// Wi-Fi任务优先级和栈大小定义
#define WIFI_STA_TASK_PRIO 24
#define WIFI_STA_TASK_STACK_SIZE 0x500

// Wi-Fi热点配置参数
#define WIFI_NAME "HQYJ_Hi3863"          // 热点名称
#define WIFI_KEY "123456789"            // 热点密码
#define STACK_IP "192.168.5.1"          // 热点IP地址
#define CONFIG_SERVER_PORT 8888         // TCP服务器端口

//消息队列
#define QUEUE_DATA_MAX_LEN  50
#define MSG_QUEUE_NUMBER    6
extern unsigned long g_msg_queue;      // 消息队列句柄
extern uint8_t msg_data[]; // 消息队列数据缓存
extern unsigned int g_msg_rev_size; // 消息大小
#endif