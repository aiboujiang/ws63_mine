#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "securec.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "hybrid.h"
#include <string.h>
#include "callbacks.h"

const char *local_name = "HybridNode";
unsigned char local_addr[SLE_ADDR_LEN] = {0x02, 0x01, 0x01, 0x01, 0x01, 0x01}; // 本地设备地址

uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;

uint8_t g_num_conn = 0; // 已连接的设备数量
sle_conn_and_service_t g_conn_and_service_arr[SLE_MAX_SERVER] = {0};
sle_write_param_t g_sle_send_param_arr[SLE_MAX_SERVER] = {0};
sle_addr_t g_addr_arr[SLE_MAX_SERVER] = {0};

sle_announce_seek_callbacks_t g_sle_announce_seek_cbk = {0}; // 扫描回调结构体
sle_connection_callbacks_t g_connect_cbk = {0};              // 连接管理回调
ssapc_callbacks_t g_sle_uart_ssapc_cbk = {0};                // SSAP客户端回调结构体

sle_addr_t g_remote_addr = {0}; // 远程设备地址
uint16_t g_conn_id = 0;         // 连接ID

/* sle server app uuid for test */
static char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0}; // 服务器应用UUID

/* server notify property uuid for test */
static char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0}; // 特征值初始值

/* sle server handle */
uint8_t g_server_id = 0; // 服务器ID

/* sle service handle */
uint16_t g_service_handle = 0; // 服务句柄

/* sle ntf property handle */
uint16_t g_property_handle = 0; // 特征句柄

// 星闪UUID基础值（128位UUID的基础部分）
uint8_t g_sle_base[] = {0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA, 0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint16_t g_sle_conn_hdl = 0; // 客户端连接句柄

static errcode_t set_announce_param(void)
{
    errcode_t ret = 0;
    // 设置设备公开参数
    sle_announce_param_t param = {0};
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;          // 可连接可扫描模式
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT;                        // 广播句柄
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;                 // 角色：发起方可协商
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;                      // 普通广播级别
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;              // 默认广播信道映射
    param.announce_interval_min = SLE_ADV_INTERVAL_MIN_DEFAULT;            // 最小广播间隔
    param.announce_interval_max = SLE_ADV_INTERVAL_MAX_DEFAULT;            // 最大广播间隔
    param.conn_interval_min = SLE_CONN_INTV_MIN_DEFAULT;                   // 最小连接间隔
    param.conn_interval_max = SLE_CONN_INTV_MAX_DEFAULT;                   // 最大连接间隔
    param.conn_max_latency = SLE_CONN_MAX_LATENCY;                         // 最大连接延迟
    param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT; // 连接监控超时
    param.announce_tx_power = SLE_ADV_TX_POWER;                            // 广播发射功率
    param.own_addr.type = 0;                                               // 地址类型
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] data memcpy fail\r\n", __FUNCTION__);
        return ret;
    }
    ret = sle_set_announce_param(param.announce_handle, &param);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] data memcpy fail\r\n", __FUNCTION__);
        return ret;
    }
    return ret;
}

static errcode_t set_announce_data(void)
{
    uint8_t announce_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0}; // 广播数据缓冲区
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0}; // 扫描响应数据缓冲区
    size_t len = 0;
    uint16_t idx = 0; // 数据写入索引
    errcode_t ret = 0;

    // 设置发现级别字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1,                         // AD结构长度（不包括长度字段本身）
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL, // 发现级别类型
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,        // 普通发现级别
    };
    ret = memcpy_s(&announce_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] adv_disc_level memcpy fail\r\n", __FUNCTION__);
        return ret;
    }
    idx += len;

    // 设置访问模式字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE, // 访问模式类型
        .value = 0,                            // 访问模式值
    };
    ret = memcpy_s(&announce_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] adv_access_mode memcpy fail\r\n", __FUNCTION__);
        return ret;
    }
    idx += len;
    announce_data_len = idx;
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;
    // 打印广播数据信息
    printf("[%s] data.announce_data_len = %d\r\n", __FUNCTION__, data.announce_data_len);
    printf("[%s] data.announce_data: ", __FUNCTION__);
    for (uint8_t data_index = 0; data_index < data.announce_data_len; data_index++) {
        printf("0x%02x ", data.announce_data[data_index]);
    }
    printf("\r\n");

    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);
    idx = 0; // 数据写入索引
    // 设置发射功率级别字段
    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL, // 发射功率类型
        .value = SLE_ADV_TX_POWER,                // 发射功率值
    };
    ret = memcpy_s(seek_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] scan response data memcpy fail\r\n", __FUNCTION__);
        return ret;
    }
    idx += scan_rsp_data_len;

    // 设置AD结构：类型字段（0x0B表示完整设备名称）
    seek_rsp_data[idx++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    // 设置AD结构：长度字段（名称长度去掉结束符+类型字段的1字节）
    seek_rsp_data[idx++] = strlen(local_name) + 1;

    // 拷贝设备名称到广播数据
    ret = memcpy_s(&seek_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx, local_name, strlen(local_name));
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] memcpy fail\r\n", __FUNCTION__);
        return ret;
    }
    idx += strlen(local_name);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = idx;
    // 打印扫描响应数据信息
    printf("[%s] data.seek_rsp_data_len = %d\r\n", __FUNCTION__, data.seek_rsp_data_len);
    printf("[%s] data.seek_rsp_data: ", __FUNCTION__);
    for (uint8_t data_index = 0; data_index < data.seek_rsp_data_len; data_index++) {
        printf("0x%02x ", data.seek_rsp_data[data_index]);
    }
    printf("\r\n");

    // 设置广播数据
    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        printf("[%s] set announce data success.\r\n", __FUNCTION__);
    } else {
        printf("[%s] set adv param fail.\r\n", __FUNCTION__);
    }
    return ret;
}

/**
 * 添加服务器服务
 * @return errcode_t 错误码
 */
static errcode_t sle_uuid_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};

    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid); // 设置服务UUID
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        return ERRCODE_SLE_FAIL;
    }
    printf("[%s] status:%d uuid len:%d \r\n", __FUNCTION__, ret, service_uuid.len);
    return ERRCODE_SLE_SUCCESS;
}

/**
 * 添加服务器特征和描述符
 * @return errcode_t 错误码
 */
static errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x00}; // 通知特征值

    // 配置特征属性
    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    property.operate_indication =
        SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid); // 设置特征UUID

    property.value = (uint8_t *)osal_vmalloc(sizeof(g_sle_property_value));
    if (property.value == NULL) {
        printf("[%s] property mem fail.\r\n", __FUNCTION__);
        return ERRCODE_SLE_FAIL;
    }

    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value, sizeof(g_sle_property_value)) !=
        EOK) {
        printf("[%s] property mem cpy fail.\r\n", __FUNCTION__);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    // 添加特征
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    // 配置描述符
    descriptor.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_CLIENT_CONFIGURATION;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);

    // 添加描述符
    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        osal_vfree(property.value);
        osal_vfree(descriptor.value);
        return ERRCODE_SLE_FAIL;
    }

    osal_vfree(property.value);
    return ERRCODE_SLE_SUCCESS;
}
/**
 * 添加服务器（注册服务、特征、描述符）
 * @return errcode_t 错误码
 */
static errcode_t sle_server_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0};

    printf("[%s] in\r\n", __FUNCTION__);
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    // 注册服务器
    ssaps_register_server(&app_uuid, &g_server_id);
    // 添加服务
    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    // 添加特征和描述符
    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    printf("[%s] server_id:%x, service_handle:%x, property_handle:%x\r\n", __FUNCTION__, g_server_id, g_service_handle,
           g_property_handle);

    // 启动服务
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail, ret:%x\r\n", __FUNCTION__, ret);
        return ERRCODE_SLE_FAIL;
    }
    printf("[%s] out\r\n", __FUNCTION__);
    return ERRCODE_SLE_SUCCESS;
}

errcode_t sle_server_sle_enable(void)
{
    errcode_t ret = 0;
    // 添加服务和特征
    ret = sle_server_add();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_server_add fail :%x\r\n", __FUNCTION__, ret);
    }
    set_announce_param();
    set_announce_data();

    // 开始广播
    ret = sle_start_announce(adv_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    return ret;
}

errcode_t sle_client_sle_enable(void)
{
    errcode_t ret = 0;
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;

    // 设置扫描参数并开始扫描
    ret = sle_set_seek_param(&param);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_set_seek_param fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    ret = sle_start_seek();
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] sle_start_seek fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    return ret;
}
/**
 * 星闪Grant Node主任务
 * @param arg 任务参数
 * @return void* NULL
 */
static void *sle_grant_task(const char *arg)
{
    unused(arg);
    // 注册设备公开和设备发现回调函数
    g_sle_announce_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;
    g_sle_announce_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;
    g_sle_announce_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk;
    g_sle_announce_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_announce_seek_cbk); // 注册回调

    g_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk;
    g_connect_cbk.pair_complete_cb = sle_sample_pair_complete_cbk;
    g_connect_cbk.connect_param_update_req_cb = sle_sample_update_req_cbk;
    g_connect_cbk.connect_param_update_cb = sle_sample_update_cbk;
    g_connect_cbk.read_rssi_cb = sle_sample_read_rssi_cbk;
    sle_connection_register_callbacks(&g_connect_cbk);

    // 设置SSAP客户端所有回调函数
    g_sle_uart_ssapc_cbk.exchange_info_cb = sle_client_exchange_info_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cb = sle_client_find_structure_cbk;
    g_sle_uart_ssapc_cbk.ssapc_find_property_cbk = sle_client_find_property_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cmp_cb = sle_client_find_structure_cmp_cbk;
    g_sle_uart_ssapc_cbk.write_cfm_cb = sle_client_write_cfm_cbk;
    g_sle_uart_ssapc_cbk.read_cfm_cb = sle_client_read_cfm_cbk;
    g_sle_uart_ssapc_cbk.notification_cb = sle_notification_cbk;
    g_sle_uart_ssapc_cbk.indication_cb = sle_indication_cbk;
    ssapc_register_callbacks(&g_sle_uart_ssapc_cbk); // 注册回调

    // 设置SSAP服务端所有回调函数
    ssaps_callbacks_t g_sle_uart_ssaps_cbk = {0};

    // 设置所有回调函数
    g_sle_uart_ssaps_cbk.add_service_cb = ssaps_add_service_cbk;
    g_sle_uart_ssaps_cbk.add_property_cb = ssaps_add_property_cbk;
    g_sle_uart_ssaps_cbk.add_descriptor_cb = ssaps_add_descriptor_cbk;
    g_sle_uart_ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    g_sle_uart_ssaps_cbk.delete_all_service_cb = ssaps_delete_all_service_cbk;
    g_sle_uart_ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    g_sle_uart_ssaps_cbk.read_request_cb = ssaps_read_request_cbk;
    g_sle_uart_ssaps_cbk.write_request_cb = ssaps_write_request_cbk;

    ssaps_register_callbacks(&g_sle_uart_ssaps_cbk);
    // 打开 SLE 开关
    if (enable_sle() != ERRCODE_SUCC) {
        printf("SLE enbale failed !\r\n");
    }

    // 设置本地设备地址
    sle_addr_t LocalAddr = {0};
    memcpy_s(LocalAddr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    LocalAddr.type = 0;
    sle_set_local_addr(&LocalAddr);
    // 设置本地设备名称
    sle_set_local_name((const uint8_t *)local_name, strlen(local_name));
    sle_client_sle_enable();
    sle_server_sle_enable();

    while (1) {
        osal_msleep(1000);
    }
    return NULL;
}

/**
 * 星闪Grant Node入口函数
 */
static void sle_grant_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();

    // 创建星闪服务器任务
    task_handle = osal_kthread_create((osal_kthread_handler)sle_grant_task, 0, "sle_grant_task", SLE_GRANT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_GRANT_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 应用运行宏：启动星闪Grant Node */
app_run(sle_grant_entry);