#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "securec.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_device_discovery.h"
#include "client.h"
#include <string.h>
#include "callbacks.h"
/* 星闪协议栈使能回调 */
void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);
    printf("SLE enable.\r\n");
}

/* 扫描使能回调函数 */
void sle_client_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error\r\n", __FUNCTION__);
    } else {
        printf("Seek enable.\n");
    }
}

/* 返回扫描结果回调 */
void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data != NULL) {
        printf("[seek_result_info_cbk] scan data : "); // 打印扫描到的设备名称 hex
        for (uint8_t i = 0; i < seek_result_data->data_length; i++) {
            printf("0x%X ", seek_result_data->data[i]);
        }
        printf("\r\n");
        if (strstr((const char *)seek_result_data->data, SLE_SERVER_NAME) != NULL) { // 名称对比成功
            memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr,
                     sizeof(sle_addr_t)); // 扫描到目标设备，将目标设备的名字拷贝到g_sle_remote_addr
            sle_stop_seek();              // 停止扫描
        }
    }
}

/* 扫描关闭回调函数 */
void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error = %x\r\n", __FUNCTION__, status);
    } else {
        sle_remove_paired_remote_device(&g_remote_addr); // 关闭扫描后，先删除之前的配对
        sle_connect_remote_device(&g_remote_addr);       // 发送连接请求
    }
}

/**
 * @brief 连接状态改变回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 */
void sle_sample_connect_state_changed_cbk(uint16_t conn_id,
                                          const sle_addr_t *addr,
                                          sle_acb_state_t conn_state,
                                          sle_pair_state_t pair_state,
                                          sle_disc_reason_t disc_reason)
{
    osal_printk("[grant] conn state changed conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0], addr->addr[4],
                addr->addr[5]);
    osal_printk("[grant] conn state changed disc_reason:0x%x\n", disc_reason);

    // 连接成功后发起配对
    if (conn_state == SLE_ACB_STATE_CONNECTED) {

        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_remote_addr);
        }
        g_conn_id = conn_id; // 保存连接ID
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
    osal_printk("[grant] pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0], addr->addr[4],
                addr->addr[5]);

    // 配对成功后交换MTU信息
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(1, g_conn_id, &info);
    }
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
    osal_printk("[grant] sle_sample_update_req_cbk interval_min = %02x, interval_max = %02x\n", param->interval_min,
                param->interval_max);
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
    osal_printk("[grant] updat state changed conn_id:%d, interval = %02x\n", conn_id, param->interval);
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
    osal_printk("rssi average = %d dbm\r\n", rssi);
}


/* 更新MTU回调 */
void sle_client_exchange_info_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssap_exchange_info_t *param,
                                         errcode_t status)
{
    printf("[%s] pair complete client id:%d status:%d\r\n", __FUNCTION__, client_id, status);
    printf("[%s] mtu size: %d, version: %d.\r\n", __FUNCTION__, param->mtu_size, param->version);
    
    // MTU交换完成后开始服务发现
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE; // 发现特征
    find_param.start_hdl = 1;               // 起始句柄
    find_param.end_hdl = 0xFFFF;            // 结束句柄
    ssapc_find_structure(0, conn_id, &find_param);
}

/* 发现服务回调 */
void sle_client_find_structure_cbk(uint8_t client_id,
                                          uint16_t conn_id,
                                          ssapc_find_service_result_t *service,
                                          errcode_t status)
{
    printf("[%s] client: %d conn_id:%d status: %d \r\n", __FUNCTION__, client_id, conn_id, status);
    printf("[%s] find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n", __FUNCTION__,
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
    g_sle_find_service_result.start_hdl = service->start_hdl;
    g_sle_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

/* 发现服务及特征完成回调 */
void sle_client_find_structure_cmp_cbk(uint8_t client_id,
                                              uint16_t conn_id,
                                              ssapc_find_structure_result_t *structure_result,
                                              errcode_t status)
{
    unused(conn_id);
    printf("[%s] client id:%d status:%d type:%d uuid len:%d \r\n", __FUNCTION__, client_id, status,
           structure_result->type, structure_result->uuid.len);
    if(structure_result->type==0x01)
    {
        ssapc_find_structure_param_t find_param = {0};
        find_param.type = SSAP_FIND_TYPE_PROPERTY; // 发现特征
        find_param.start_hdl = g_sle_find_service_result.start_hdl;               // 起始句柄
        find_param.end_hdl = g_sle_find_service_result.end_hdl;            // 结束句柄
        ssapc_find_structure(0, conn_id, &find_param);
    }
    else if(structure_result->type==0x03)
    {
        printf("Find property complete.\n");
    }
}

/* 发现特征回调 */
void sle_client_find_property_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssapc_find_property_result_t *property,
                                         errcode_t status)
{
    printf(
        "[%s] client id: %d, conn id: %d, operate ind: %d, "
        "descriptors count: %d status:%d property->handle %d\r\n",
        __FUNCTION__, client_id, conn_id, property->operate_indication, property->descriptors_count, status,
        property->handle);
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
    // 保存特征句柄用于后续数据发送
    g_sle_send_param.handle = property->handle;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
}



/* 收到写响应回调 */
void sle_client_write_cfm_cbk(uint8_t client_id,
                                     uint16_t conn_id,
                                     ssapc_write_result_t *write_result,
                                     errcode_t status)
{
    printf("[%s] conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n", __FUNCTION__, conn_id, client_id, status,
           write_result->handle, write_result->type);
    // 写入成功后读取数据
    ssapc_read_req(0, conn_id, write_result->handle, write_result->type);
}



/* 收到通知回调 */
void sle_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    // 打印接收到的通知数据
    printf("recv len:%d data: ", data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        printf("%c", data->data[i]);
    }
    printf("\r\n");
}

/* 收到指示回调 */
void sle_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);
    // 打印接收到的指示数据
    printf("recv len:%d data: ", data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        printf("%c", data->data[i]);
    }
    printf("\r\n");
}