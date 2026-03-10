#ifndef SLE_SERVER_DEFINES_H
#define SLE_SERVER_DEFINES_H
#include <stdint.h>
/* 任务相关 */
#define SLE_TERMINAL_TASK_PRIO 24
#define SLE_TERMINAL_STACK_SIZE 0x2000


/* 连接参数配置 */
#define SLE_CONN_INTV_MIN_DEFAULT 0x32     // 最小连接间隔12.5ms（单位：125us）
#define SLE_CONN_INTV_MAX_DEFAULT 0x32     // 最大连接间隔12.5ms（单位：125us）
#define SLE_ADV_INTERVAL_MIN_DEFAULT 0xC8  // 最小广播间隔25ms（单位：125us）
#define SLE_ADV_INTERVAL_MAX_DEFAULT 0xC8  // 最大广播间隔25ms（单位：125us）
#define SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT 0x1F4  // 连接监控超时时间5000ms（单位：10ms）
#define SLE_CONN_MAX_LATENCY 0x1F3         // 最大连接延迟4990ms（单位：10ms）

/* 广播参数配置 */
#define SLE_ADV_TX_POWER 20                // 广播发送功率20dBm
#define SLE_ADV_HANDLE_DEFAULT 1           // 默认广播句柄
#define SLE_ADV_DATA_LEN_MAX 251           // 最大广播数据长度251字节


#define SLE_MTU_SIZE_DEFAULT 512                 // 默认MTU大小
#define DEFAULT_PAYLOAD_SIZE (SLE_MTU_SIZE_DEFAULT - 12) // 设置有效载荷，MTU减去协议头开销

#define OCTET_BIT_LEN 8
#define UUID_LEN_2 2

#define UUID_16BIT_LEN 2
#define UUID_128BIT_LEN 16

/* Service UUID */
#define SLE_UUID_SERVER_SERVICE 0xABCD           // 自定义服务UUID
/* Property UUID */
#define SLE_UUID_SERVER_NTF_REPORT 0xBCDE        // 自定义特征UUID

typedef struct sle_adv_common_value {
    uint8_t type;
    uint8_t length;
    uint8_t value;
} le_adv_common_t;

typedef enum sle_adv_channel {
    SLE_ADV_CHANNEL_MAP_77 = 0x01,
    SLE_ADV_CHANNEL_MAP_78 = 0x02,
    SLE_ADV_CHANNEL_MAP_79 = 0x04,
    SLE_ADV_CHANNEL_MAP_DEFAULT = 0x07
} sle_adv_channel_map_t;

typedef enum sle_adv_data {
    SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL = 0x01,                         /* 发现等级 */
    SLE_ADV_DATA_TYPE_ACCESS_MODE = 0x02,                             /* 接入层能力 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_16BIT_UUID = 0x03,                 /* 标准服务数据信息 */
    SLE_ADV_DATA_TYPE_SERVICE_DATA_128BIT_UUID = 0x04,                /* 自定义服务数据信息 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x05,    /* 完整标准服务标识列表 */
    SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_128BIT_SERVICE_UUIDS = 0x06,   /* 完整自定义服务标识列表 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x07,  /* 部分标准服务标识列表 */
    SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_128BIT_SERVICE_UUIDS = 0x08, /* 部分自定义服务标识列表 */
    SLE_ADV_DATA_TYPE_SERVICE_STRUCTURE_HASH_VALUE = 0x09,            /* 服务结构散列值 */
    SLE_ADV_DATA_TYPE_SHORTENED_LOCAL_NAME = 0x0A,                    /* 设备缩写本地名称 */
    SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x0B,                     /* 设备完整本地名称 */
    SLE_ADV_DATA_TYPE_TX_POWER_LEVEL = 0x0C,                          /* 广播发送功率 */
    SLE_ADV_DATA_TYPE_SLB_COMMUNICATION_DOMAIN = 0x0D,                /* SLB通信域域名 */
    SLE_ADV_DATA_TYPE_SLB_MEDIA_ACCESS_LAYER_ID = 0x0E,               /* SLB媒体接入层标识 */
    SLE_ADV_DATA_TYPE_EXTENDED = 0xFE,                                /* 数据类型扩展 */
    SLE_ADV_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA = 0xFF               /* 厂商自定义信息 */
} sle_adv_data_type;


/* ============= 全局变量声明 ============= */

/** 本地设备名称 */
extern const char *local_name;

/** 广播句柄 */
extern uint8_t adv_handle;

/** 本地设备地址 */
extern unsigned char local_addr[SLE_ADDR_LEN];

/** 星闪连接句柄 */
extern uint16_t g_sle_conn_hdl;

/** 服务器ID */
extern uint8_t g_server_id;

/** 服务句柄 */
extern uint16_t g_service_handle;

/** 特征句柄 */
extern uint16_t g_property_handle;

/** 星闪UUID基础值（128位UUID的基础部分） */
extern uint8_t g_sle_base[];

/** 扫描回调结构体 */
extern sle_announce_seek_callbacks_t g_sle_announce_seek_cbk;

/** 连接管理回调结构体 */
extern sle_connection_callbacks_t g_connect_cbk;

extern uint8_t connected_flag;
#endif
