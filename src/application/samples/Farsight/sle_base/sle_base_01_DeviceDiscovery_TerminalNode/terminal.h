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

