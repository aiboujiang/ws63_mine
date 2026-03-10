#ifndef SLE_CLIENT_H
#define SLE_CLIENT_H
#include <stdint.h>
#include "sle_common.h"
#include "sle_ssap_client.h"
/* 任务相关 */
#define SLE_GRANT_TASK_PRIO 24
#define SLE_GRANT_STACK_SIZE 0x500

// 默认定义
#define SLE_MTU_SIZE_DEFAULT 256              // 默认MTU大小
#define SLE_SEEK_INTERVAL_DEFAULT 500          // 默认扫描间隔
#define SLE_SEEK_WINDOW_DEFAULT 100            // 默认扫描窗口
#define UUID_16BIT_LEN 2                       // 16位UUID长度
#define UUID_128BIT_LEN 16                     // 128位UUID长度
#define SLE_SPEED_HUNDRED 100                  // 用于浮点数计算的常量
#define SPEED_DEFAULT_CONN_INTERVAL 0x14       // 默认连接间隔
#define SPEED_DEFAULT_TIMEOUT_MULTIPLIER 0x1f4 // 默认超时乘数
#define SPEED_DEFAULT_SCAN_INTERVAL 400        // 默认扫描间隔
#define SPEED_DEFAULT_SCAN_WINDOW 20           // 默认扫描窗口

#define SLE_SERVER_NAME "Terminal"

#define SLE_MAX_SERVER 8
extern sle_addr_t g_remote_addr; // 远程设备地址
extern uint16_t g_conn_id;         // 连接ID

extern ssapc_find_service_result_t g_sle_find_service_result;
extern ssapc_write_param_t g_sle_send_param; 
extern uint8_t g_num_conn;

typedef struct
{
    uint16_t conn_id;                                
    ssapc_find_service_result_t find_service_result; 
} sle_conn_and_service_t;

typedef struct
{
    uint16_t conn_id;                                
    ssapc_write_param_t g_sle_send_param; 
} sle_write_param_t;

extern sle_conn_and_service_t g_conn_and_service_arr[];
extern sle_write_param_t g_sle_send_param_arr[];
extern sle_addr_t  g_addr_arr[];
#endif /* SLE_CLIENT_H */