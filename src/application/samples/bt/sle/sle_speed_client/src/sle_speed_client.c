/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022. All rights reserved.
 *
 * Description: SLE private service register sample of client.
 */

#include "app_init.h"
#include "systick.h"
#include "tcxo.h"
#include "los_memory.h"
#include "securec.h"
#include "soc_osal.h"
#include "common_def.h"

#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"

#include "sle_speed_client.h"

#undef THIS_FILE_ID
#define THIS_FILE_ID BTH_GLE_SAMPLE_UUID_CLIENT

// 默认定义
#define SLE_MTU_SIZE_DEFAULT        1500        // 默认MTU大小
#define SLE_SEEK_INTERVAL_DEFAULT   100         // 默认扫描间隔
#define SLE_SEEK_WINDOW_DEFAULT     100         // 默认扫描窗口
#define UUID_16BIT_LEN 2                        // 16位UUID长度
#define UUID_128BIT_LEN 16                      // 128位UUID长度
#define SLE_SPEED_HUNDRED   100                 // 用于浮点数计算的常量
#define SPEED_DEFAULT_CONN_INTERVAL 0x14        // 默认连接间隔
#define SPEED_DEFAULT_TIMEOUT_MULTIPLIER 0x1f4  // 默认超时乘数
#define SPEED_DEFAULT_SCAN_INTERVAL 400         // 默认扫描间隔
#define SPEED_DEFAULT_SCAN_WINDOW 20            // 默认扫描窗口

// 全局变量
static int g_recv_pkt_num = 0;                  // 接收数据包计数
static uint64_t g_count_before_get_us;          // 开始时间戳(微秒)
static uint64_t g_count_after_get_us;           // 结束时间戳(微秒)

// 根据配置定义接收包数量
#ifdef CONFIG_LARGE_THROUGHPUT_CLIENT
#define RECV_PKT_CNT 1000
#else
#define RECV_PKT_CNT 1
#endif

static int g_rssi_sum = 0;                      // RSSI总和
static int g_rssi_number = 0;                   // RSSI测量次数

// 回调函数结构体
static sle_announce_seek_callbacks_t g_seek_cbk = {0};      // 设备发现回调
static sle_connection_callbacks_t    g_connect_cbk = {0};   // 连接管理回调
static ssapc_callbacks_t             g_ssapc_cbk = {0};     // SSAP客户端回调
static sle_addr_t                    g_remote_addr = {0};   // 远程设备地址
static uint16_t                      g_conn_id = 0;         // 连接ID
static ssapc_find_service_result_t   g_find_service_result = {0}; // 服务发现结果

/**
 * @brief SLE使能完成回调函数
 * @param status 操作状态
 */
void sle_sample_sle_enable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_start_scan();  // 使能成功后开始扫描
    }
}

/**
 * @brief 扫描使能完成回调函数
 * @param status 操作状态
 */
void sle_sample_seek_enable_cbk(errcode_t status)
{
    if (status == 0) {
        return;
    }
}

/**
 * @brief 扫描禁用完成回调函数
 * @param status 操作状态
 */
void sle_sample_seek_disable_cbk(errcode_t status)
{
    if (status == 0) {
        sle_connect_remote_device(&g_remote_addr);  // 停止扫描后连接设备
    }
}

/**
 * @brief 扫描结果回调函数
 * @param seek_result_data 扫描结果数据
 */
void sle_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data != NULL) {
        // 目标设备的MAC地址
        uint8_t mac[SLE_ADDR_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        // 比较地址，找到目标设备
        if (memcmp(seek_result_data->addr.addr, mac, SLE_ADDR_LEN) == 0) {
            (void)memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
            sle_stop_seek();  // 找到设备后停止扫描
        }
    }
}

/**
 * @brief 获取浮点数的整数部分
 * @param in 输入浮点数
 * @return 整数部分
 */
static uint32_t get_float_int(float in)
{
    return (uint32_t)(((uint64_t)(in * SLE_SPEED_HUNDRED)) / SLE_SPEED_HUNDRED);
}

/**
 * @brief 获取浮点数的小数部分
 * @param in 输入浮点数
 * @return 小数部分(放大100倍)
 */
static uint32_t get_float_dec(float in)
{
    return (uint32_t)(((uint64_t)(in * SLE_SPEED_HUNDRED)) % SLE_SPEED_HUNDRED);
}

/**
 * @brief 速度测试通知回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param data 接收到的数据
 * @param status 操作状态
 */
static void sle_speed_notification_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(client_id);
    unused(status);
    sle_read_remote_device_rssi(conn_id); // 读取RSSI用于统计

    // 记录第一个数据包的时间
    if (g_recv_pkt_num == 0) {
        g_count_before_get_us = uapi_tcxo_get_us();
    } else if (g_recv_pkt_num == RECV_PKT_CNT) {
        // 记录最后一个数据包的时间并计算速度
        g_count_after_get_us = uapi_tcxo_get_us();
        printf("g_count_after_get_us = %llu, g_count_before_get_us = %llu, data_len = %d\r\n",
            g_count_after_get_us, g_count_before_get_us, data->data_len);
        
        // 计算总时间(秒)
        float time = (float)(g_count_after_get_us - g_count_before_get_us) / 1000000.0f;
        printf("time = %d.%d s\r\n", get_float_int(time), get_float_dec(time));
        
        // 计算传输速率(bps)
        uint16_t len = data->data_len;
        float speed = len * RECV_PKT_CNT * 8 / time;  // 1字节=8比特
        printf("speed = %d.%d bps\r\n", get_float_int(speed), get_float_dec(speed));
        
        // 重置计数器
        g_recv_pkt_num = 0;
        g_count_before_get_us = g_count_after_get_us;
    }
    g_recv_pkt_num++;  // 增加接收包计数
}

/**
 * @brief 速度测试指示回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param data 接收到的数据
 * @param status 操作状态
 */
static void sle_speed_indication_cb(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
    errcode_t status)
{
    unused(status);
    unused(conn_id);
    unused(client_id);
    osal_printk("\n sle_speed_indication_cb sle uart recived data : %s\r\n", data->data);
}

/**
 * @brief 注册设备发现回调函数
 */
void sle_sample_seek_cbk_register(void)
{
    g_seek_cbk.sle_enable_cb = sle_sample_sle_enable_cbk;
    g_seek_cbk.seek_enable_cb = sle_sample_seek_enable_cbk;
    g_seek_cbk.seek_disable_cb = sle_sample_seek_disable_cbk;
    g_seek_cbk.seek_result_cb = sle_sample_seek_result_info_cbk;
}

/**
 * @brief 连接状态改变回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 */
void sle_sample_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("[ssap client] conn state changed conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]);
    osal_printk("[ssap client] conn state changed disc_reason:0x%x\n", disc_reason);
    
    // 连接成功后发起配对
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_remote_addr);
        }
        g_conn_id = conn_id;  // 保存连接ID
    }
}

/**
 * @brief 配对完成回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param status 操作状态
 */
void sle_sample_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("[ssap client] pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0],
        addr->addr[4], addr->addr[5]);
    
    // 配对成功后交换MTU信息
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(1, g_conn_id, &info);
    }
}

/**
 * @brief 连接参数更新回调函数
 * @param conn_id 连接ID
 * @param status 操作状态
 * @param param 连接参数
 */
void sle_sample_update_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_evt_t *param)
{
    unused(status);
    osal_printk("[ssap client] updat state changed conn_id:%d, interval = %02x\n", conn_id, param->interval);
}

/**
 * @brief 连接参数更新请求回调函数
 * @param conn_id 连接ID
 * @param status 操作状态
 * @param param 连接参数
 */
void sle_sample_update_req_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_req_t *param)
{
    unused(conn_id);
    unused(status);
    osal_printk("[ssap client] sle_sample_update_req_cbk interval_min = %02x, interval_max = %02x\n",
        param->interval_min, param->interval_max);
}

/**
 * @brief 读取RSSI回调函数
 * @param conn_id 连接ID
 * @param rssi RSSI值
 * @param status 操作状态
 */
void sle_sample_read_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status)
{
    unused(conn_id);
    unused(status);
    // 统计RSSI平均值
    g_rssi_sum = g_rssi_sum + rssi;
    g_rssi_number++;
    if (g_rssi_number == RECV_PKT_CNT) {
        osal_printk("rssi average = %d dbm\r\n", g_rssi_sum / g_rssi_number);
        g_rssi_sum = 0;
        g_rssi_number = 0;
    }
}

/**
 * @brief 注册连接管理回调函数
 */
void sle_sample_connect_cbk_register(void)
{
    g_connect_cbk.connect_state_changed_cb = sle_sample_connect_state_changed_cbk;
    g_connect_cbk.pair_complete_cb = sle_sample_pair_complete_cbk;
    g_connect_cbk.connect_param_update_req_cb = sle_sample_update_req_cbk;
    g_connect_cbk.connect_param_update_cb = sle_sample_update_cbk;
    g_connect_cbk.read_rssi_cb = sle_sample_read_rssi_cbk;
}

/**
 * @brief 信息交换完成回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param param 交换的信息
 * @param status 操作状态
 */
void sle_sample_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
    errcode_t status)
{
    osal_printk("[ssap client] pair complete client id:%d status:%d\n", client_id, status);
    osal_printk("[ssap client] exchange mtu, mtu size: %d, version: %d.\n",
        param->mtu_size, param->version);

    // 交换信息完成后开始发现服务
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    ssapc_find_structure(0, conn_id, &find_param);
}

/**
 * @brief 服务发现结果回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param service 发现的服务信息
 * @param status 操作状态
 */
void sle_sample_find_structure_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_service_result_t *service,
    errcode_t status)
{
    osal_printk("[ssap client] find structure cbk client: %d conn_id:%d status: %d \n",
        client_id, conn_id, status);
    osal_printk("[ssap client] find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n",
        service->start_hdl, service->end_hdl, service->uuid.len);
    
    // 打印UUID信息
    if (service->uuid.len == UUID_16BIT_LEN) {
        osal_printk("[ssap client] structure uuid:[0x%02x][0x%02x]\r\n",
            service->uuid.uuid[14], service->uuid.uuid[15]);
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] structure uuid[%d]:[0x%02x]\r\n", idx, service->uuid.uuid[idx]);
        }
    }
    
    // 保存服务发现结果
    g_find_service_result.start_hdl = service->start_hdl;
    g_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

/**
 * @brief 服务发现完成回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param structure_result 发现的服务结构信息
 * @param status 操作状态
 */
void sle_sample_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    osal_printk("[ssap client] find structure cmp cbk client id:%d status:%d type:%d uuid len:%d \r\n",
        client_id, status, structure_result->type, structure_result->uuid.len);
    
    // 打印UUID信息
    if (structure_result->uuid.len == UUID_16BIT_LEN) {
        osal_printk("[ssap client] find structure cmp cbk structure uuid:[0x%02x][0x%02x]\r\n",
            structure_result->uuid.uuid[14], structure_result->uuid.uuid[15]);
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] find structure cmp cbk structure uuid[%d]:[0x%02x]\r\n", idx,
                structure_result->uuid.uuid[idx]);
        }
    }
    
    // 发现服务后写入测试数据
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t len = sizeof(data);
    ssapc_write_param_t param = {0};
    param.handle = g_find_service_result.start_hdl;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.data_len = len;
    param.data = data;
    ssapc_write_req(0, conn_id, &param);
}

/**
 * @brief 属性发现回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param property 发现的属性信息
 * @param status 操作状态
 */
void sle_sample_find_property_cbk(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("[ssap client] find property cbk, client id: %d, conn id: %d, operate ind: %d, "
        "descriptors count: %d status:%d.\n", client_id, conn_id, property->operate_indication,
        property->descriptors_count, status);
    
    // 打印描述符类型
    for (uint16_t idx = 0; idx < property->descriptors_count; idx++) {
        osal_printk("[ssap client] find property cbk, descriptors type [%d]: 0x%02x.\n",
            idx, property->descriptors_type[idx]);
    }
    
    // 打印UUID信息
    if (property->uuid.len == UUID_16BIT_LEN) {
        osal_printk("[ssap client] find property cbk, uuid: %02x %02x.\n",
            property->uuid.uuid[14], property->uuid.uuid[15]);
    } else if (property->uuid.len == UUID_128BIT_LEN) {
        for (uint16_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] find property cbk, uuid [%d]: %02x.\n",
                idx, property->uuid.uuid[idx]);
        }
    }
}

/**
 * @brief 写入操作确认回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param write_result 写入结果
 * @param status 操作状态
 */
void sle_sample_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result,
    errcode_t status)
{
    osal_printk("[ssap client] write cfm cbk, client id: %d status:%d.\n", client_id, status);
    // 写入成功后读取数据
    ssapc_read_req(0, conn_id, write_result->handle, write_result->type);
}

/**
 * @brief 读取操作确认回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param read_data 读取的数据
 * @param status 操作状态
 */
void sle_sample_read_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *read_data,
    errcode_t status)
{
    osal_printk("[ssap client] read cfm cbk client id: %d conn id: %d status: %d\n",
        client_id, conn_id, status);
    osal_printk("[ssap client] read cfm cbk handle: %d, type: %d , len: %d\n",
        read_data->handle, read_data->type, read_data->data_len);
    
    // 打印读取的数据
    for (uint16_t idx = 0; idx < read_data->data_len; idx++) {
        osal_printk("[ssap client] read cfm cbk[%d] 0x%02x\r\n", idx, read_data->data[idx]);
    }
}

/**
 * @brief 注册SSAP客户端回调函数
 * @param notification_cb 通知回调函数
 * @param indication_cb 指示回调函数
 */
void sle_sample_ssapc_cbk_register(ssapc_notification_callback notification_cb,
    ssapc_indication_callback indication_cb)
{
    g_ssapc_cbk.exchange_info_cb = sle_sample_exchange_info_cbk;
    g_ssapc_cbk.find_structure_cb = sle_sample_find_structure_cbk;
    g_ssapc_cbk.find_structure_cmp_cb = sle_sample_find_structure_cmp_cbk;
    g_ssapc_cbk.ssapc_find_property_cbk = sle_sample_find_property_cbk;
    g_ssapc_cbk.write_cfm_cb = sle_sample_write_cfm_cbk;
    g_ssapc_cbk.read_cfm_cb = sle_sample_read_cfm_cbk;
    g_ssapc_cbk.notification_cb = notification_cb;
    g_ssapc_cbk.indication_cb = indication_cb;
}

/**
 * @brief 初始化连接参数
 */
void sle_speed_connect_param_init(void)
{
    sle_default_connect_param_t param = {0};
    param.enable_filter_policy = 0;
    param.gt_negotiate = 0;
    param.initiate_phys = 1;
    param.max_interval = SPEED_DEFAULT_CONN_INTERVAL;
    param.min_interval = SPEED_DEFAULT_CONN_INTERVAL;
    param.scan_interval = SPEED_DEFAULT_SCAN_INTERVAL;
    param.scan_window = SPEED_DEFAULT_SCAN_WINDOW;
    param.timeout = SPEED_DEFAULT_TIMEOUT_MULTIPLIER;
    sle_default_connection_param_set(&param);
}

/**
 * @brief 初始化SLE客户端
 * @param notification_cb 通知回调函数
 * @param indication_cb 指示回调函数
 */
void sle_client_init(ssapc_notification_callback notification_cb, ssapc_indication_callback indication_cb)
{
    // 设置本地地址
    uint8_t local_addr[SLE_ADDR_LEN] = {0x13, 0x67, 0x5c, 0x07, 0x00, 0x51};
    sle_addr_t local_address;
    local_address.type = 0;
    (void)memcpy_s(local_address.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    
    // 注册各种回调函数
    sle_sample_seek_cbk_register();
    sle_speed_connect_param_init();
    sle_sample_connect_cbk_register();
    sle_sample_ssapc_cbk_register(notification_cb, indication_cb);
    
    // 向SLE栈注册回调
    sle_announce_seek_register_callbacks(&g_seek_cbk);
    sle_connection_register_callbacks(&g_connect_cbk);
    ssapc_register_callbacks(&g_ssapc_cbk);
    
    // 启用SLE功能并设置本地地址
    enable_sle();
    sle_set_local_addr(&local_address);
}

/**
 * @brief 开始扫描设备
 */
void sle_start_scan()
{
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 0;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    
    // 设置扫描参数并开始扫描
    sle_set_seek_param(&param);
    sle_start_seek();
}

/**
 * @brief 速度测试初始化函数
 * @return 初始化状态
 */
int sle_speed_init(void)
{
    osal_msleep(1000);  // 延时1秒等待系统稳定
    // 初始化SLE客户端，注册速度测试回调
    sle_client_init(sle_speed_notification_cb, sle_speed_indication_cb);
    return 0;
}

// 任务定义
#define SLE_SPEED_TASK_PRIO 26         // 任务优先级
#define SLE_SPEED_STACK_SIZE 0x2000    // 任务栈大小

/**
 * @brief 速度测试任务入口函数
 */
static void sle_speed_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    
    // 创建速度测试任务
    task_handle = osal_kthread_create((osal_kthread_handler)sle_speed_init, 0, "RadarTask", SLE_SPEED_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SPEED_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

// 应用运行入口
app_run(sle_speed_entry);