#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "securec.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_ssap_server.h"
#include "sle_device_discovery.h"
#include "hybrid.h"
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
    if (seek_result_data == NULL) {
        osal_printk("[sle_client_seek_result_info_cbk] seek_result_data is NULL\r\n");
        return;
    }
    if (strstr((const char *)seek_result_data->data, SLE_SERVER_NAME) != NULL) {
        if (g_num_conn < SLE_MAX_SERVER) {
            osal_printk("[sle_client_seek_result_info_cbk] target found, addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                        seek_result_data->addr.addr[0], seek_result_data->addr.addr[1], seek_result_data->addr.addr[2],
                        seek_result_data->addr.addr[3], seek_result_data->addr.addr[4], seek_result_data->addr.addr[5]);

            memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));

            sle_connect_remote_device(&g_remote_addr);
            sle_stop_seek(); // 找到目标设备后停止扫描
        }
    }
}

/* 扫描关闭回调函数 */
void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != ERRCODE_SUCC) {
        osal_printk("[sle_client_seek_disable_cbk] status error = %x\r\n", status);
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
void sle_client_connect_state_changed_cbk(uint16_t conn_id,
                                          const sle_addr_t *addr,
                                          sle_acb_state_t conn_state,
                                          sle_pair_state_t pair_state,
                                          sle_disc_reason_t disc_reason)
{
    osal_printk(
        "[sle_client_connect_state_changed_cbk] conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n",
        conn_id, conn_state, pair_state, disc_reason);
    osal_printk("[sle_client_connect_state_changed_cbk] addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n", addr->addr[0],
                addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        if (addr->addr[0] == 0x03) // ServerNode
        {
            memcpy_s(&g_addr_arr[g_num_conn], sizeof(sle_addr_t), addr, sizeof(sle_addr_t));
            g_conn_and_service_arr[g_num_conn].conn_id = conn_id;
            g_sle_send_param_arr[g_num_conn].conn_id = conn_id;
            g_num_conn++;
            printf("Connect with server node.");

            ssap_exchange_info_t info = {0};

            info.mtu_size = SLE_MTU_SIZE_DEFAULT;

            info.version = 1;

            ssapc_exchange_info_req(0, conn_id, &info);
        } else if (addr->addr[0] == 0x01) {
            printf("Connect with client node.");
            g_sle_conn_hdl=conn_id;
            sle_stop_announce(adv_handle);
        }
    }

    else if (conn_state == SLE_ACB_STATE_NONE) {
        osal_printk("[sle_client_connect_state_changed_cbk] SLE_ACB_STATE_NONE\r\n");
    }

    else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        osal_printk("[sle_client_connect_state_changed_cbk] SLE_ACB_STATE_DISCONNECTED\r\n");
        if (addr->addr[0] == 0x03) // ServerNode
        {
            for (int i = 0; i < g_num_conn; i++) {
                if (g_conn_and_service_arr[i].conn_id == conn_id) {

                    int j;
                    for (j = i; j < g_num_conn - 1; j++) {
                        g_conn_and_service_arr[j] = g_conn_and_service_arr[j + 1];
                        g_sle_send_param_arr[j] = g_sle_send_param_arr[j + 1];
                        g_addr_arr[j] = g_addr_arr[j + 1];
                    }

                    memset(&g_conn_and_service_arr[j], 0, sizeof(sle_conn_and_service_t));
                    memset(&g_sle_send_param_arr[j], 0, sizeof(sle_write_param_t));
                    memset(&g_addr_arr[j], 0, sizeof(sle_addr_t));
                    g_num_conn--;
                }
            }
        } else if (addr->addr[0] == 0x01) {
            sle_start_announce(adv_handle);
        }
    } else {
        osal_printk("[sle_client_connect_state_changed_cbk] status error\r\n");
    }
    if (g_num_conn < SLE_MAX_SERVER) {
        sle_seek_param_t param = {0};
        param.own_addr_type = 0;
        param.filter_duplicates = 0;
        param.seek_filter_policy = 0;
        param.seek_phys = 1;
        param.seek_type[0] = 1;
        param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
        param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;

        // 设置扫描参数并开始扫描
        sle_set_seek_param(&param);
        sle_start_seek();
    } else {
        sle_stop_seek(); // 已达到最大连接数，停止扫描
        printf("device full\n ");
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
void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status)
{
    printf("[%s] pair complete client id:%d status:%d\r\n", __FUNCTION__, client_id, status);
    printf("[%s] mtu size: %d, version: %d.\r\n", __FUNCTION__, param->mtu_size, param->version);

    // MTU交换完成后开始服务发现
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE; // 发现特征
    find_param.start_hdl = 1;                         // 起始句柄
    find_param.end_hdl = 0xFFFF;                      // 结束句柄
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
        osal_printk("[ssap client] structure uuid:[0x%02x][0x%02x]\r\n", service->uuid.uuid[14],
                    service->uuid.uuid[15]);
    } else {
        for (uint8_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] structure uuid[%d]:[0x%02x]\r\n", idx, service->uuid.uuid[idx]);
        }
    }
    for (int i = 0; i < g_num_conn; i++) {
        if (g_conn_and_service_arr[i].conn_id == conn_id) {

            g_conn_and_service_arr[i].find_service_result.start_hdl = service->start_hdl;

            g_conn_and_service_arr[i].find_service_result.end_hdl = service->end_hdl;

            memcpy_s(&g_conn_and_service_arr[i].find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid,
                     sizeof(sle_uuid_t));
        }
    }
}

/* 发现服务及特征完成回调 */
void sle_client_find_structure_cmp_cbk(uint8_t client_id,
                                       uint16_t conn_id,
                                       ssapc_find_structure_result_t *structure_result,
                                       errcode_t status)
{
    printf("[%s] client id:%d status:%d type:%d uuid len:%d \r\n", __FUNCTION__, client_id, status,
           structure_result->type, structure_result->uuid.len);
    if (structure_result->type == 0x01) {
        ssapc_find_structure_param_t find_param = {0};
        find_param.type = SSAP_FIND_TYPE_PROPERTY; // 发现特征
        for (int i = 0; i < g_num_conn; i++) {
            if (g_conn_and_service_arr[i].conn_id == conn_id) {
                find_param.start_hdl = g_conn_and_service_arr[i].find_service_result.start_hdl;
                find_param.end_hdl = g_conn_and_service_arr[i].find_service_result.end_hdl;
                memcpy_s(&g_conn_and_service_arr[i].find_service_result.uuid, sizeof(sle_uuid_t),
                         structure_result->uuid.uuid, sizeof(sle_uuid_t));
            }
        }
        ssapc_find_structure(0, conn_id, &find_param);
    } else if (structure_result->type == 0x03) {
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
        osal_printk("[ssap client] find property cbk, uuid: %02x %02x.\n", property->uuid.uuid[14],
                    property->uuid.uuid[15]);
    } else if (property->uuid.len == UUID_128BIT_LEN) {
        for (uint16_t idx = 0; idx < UUID_128BIT_LEN; idx++) {
            osal_printk("[ssap client] find property cbk, uuid [%d]: %02x.\n", idx, property->uuid.uuid[idx]);
        }
    }
    // 保存特征句柄用于后续数据发送
    for (int i = 0; i < g_num_conn; i++) {
        if (g_sle_send_param_arr[i].conn_id == conn_id) {
            g_sle_send_param_arr[i].g_sle_send_param.handle = property->handle;
            g_sle_send_param_arr[i].g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
        }
    }
}

/* 收到写响应回调 */
void sle_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result, errcode_t status)
{
    printf("[%s] conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n", __FUNCTION__, conn_id, client_id, status,
           write_result->handle, write_result->type);
}

/**
 * @brief 读取操作确认回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param read_data 读取的数据
 * @param status 操作状态
 */
void sle_client_read_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *read_data, errcode_t status)
{
    osal_printk("[ssap client] read cfm cbk client id: %d conn id: %d status: %d\n", client_id, conn_id, status);
    osal_printk("[ssap client] read cfm cbk handle: %d, type: %d , len: %d\n", read_data->handle, read_data->type,
                read_data->data_len);

    // 打印读取的数据
    printf("[ssap client] read cfm cbk:");
    for (uint16_t idx = 0; idx < read_data->data_len; idx++) {
        osal_printk("%c", read_data->data[idx]);
    }
    printf("\n");
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
    ssaps_ntf_ind_t param = {0};
    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value_len = data->data_len;
    param.value = data->data;
    if (ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param) != ERRCODE_SUCC) {
        printf("[sle_server_send_notify_by_handle] ssaps_notify_indicate fail\r\n");
        osal_vfree(param.value);
    }
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

// 服务端回调函数

/**
 * 将16位数据编码为小端字节序
 * @param ptr 目标指针
 * @param data 要编码的16位数据
 */
void encode2byte_little(uint8_t *ptr, uint16_t data)
{
    *(uint8_t *)((ptr) + 1) = (uint8_t)((data) >> 0x8); // 高字节在后
    *(uint8_t *)(ptr) = (uint8_t)(data);                // 低字节在前
}

/**
 * 设置星闪UUID的基础部分
 * @param out 输出的UUID结构体
 */
void sle_uuid_set_base(sle_uuid_t *out)
{
    errcode_t ret;
    ret = memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_base, SLE_UUID_LEN);
    if (ret != EOK) {
        printf("[%s] memcpy fail\n", __FUNCTION__);
        out->len = 0;
        return;
    }
    out->len = UUID_LEN_2;
}

/**
 * 设置16位UUID值（基于基础UUID）
 * @param u2 16位UUID值
 * @param out 输出的UUID结构体
 */
void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out); // 设置基础部分
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2); /* 14: 在基础UUID的最后2字节位置设置自定义UUID值 */
}

/**
 * 打印UUID内容
 * @param uuid 要打印的UUID结构体
 */
void sle_uuid_print(sle_uuid_t *uuid)
{
    printf("[%s] ", __FUNCTION__);
    if (uuid == NULL) {
        printf("uuid is null\r\n");
        return;
    }
    if (uuid->len == UUID_16BIT_LEN) {
        printf("uuid: %02x %02x.\n", uuid->uuid[14], uuid->uuid[15]); /* 14 15: 下标，显示最后2字节 */
    } else if (uuid->len == UUID_128BIT_LEN) {
        for (size_t i = 0; i < UUID_128BIT_LEN; i++) {
            printf("0x%02x ", uuid->uuid[i]); // 打印完整的128位UUID
        }
    }
}
/* ============= SSAPS (星闪服务器协议栈) 回调函数 ============= */

/**
 * MTU改变回调函数
 */
void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size, errcode_t status)
{
    printf("[%s] server_id:%d, conn_id:%d, mtu_size:%d, status:%d\r\n", __FUNCTION__, server_id, conn_id,
           mtu_size->mtu_size, status);
}

/**
 * 服务启动回调函数
 */
void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    printf("[%s] server_id:%d, handle:%x, status:%x\r\n", __FUNCTION__, server_id, handle, status);
}
/**
 * 服务添加回调函数
 */
void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    printf("[%s] server_id:%x, handle:%x, status:%x\r\n", __FUNCTION__, server_id, handle, status);
    sle_uuid_print(uuid);
}

/**
 * 特征添加回调函数
 */
void ssaps_add_property_cbk(uint8_t server_id,
                            sle_uuid_t *uuid,
                            uint16_t service_handle,
                            uint16_t handle,
                            errcode_t status)
{
    printf("[%s] server_id:%x, service_handle:%x,handle:%x, status:%x\r\n", __FUNCTION__, server_id, service_handle,
           handle, status);
    sle_uuid_print(uuid);
}

/**
 * 描述符添加回调函数
 */
void ssaps_add_descriptor_cbk(uint8_t server_id,
                              sle_uuid_t *uuid,
                              uint16_t service_handle,
                              uint16_t property_handle,
                              errcode_t status)
{
    printf("[%s] server_id:%x, service_handle:%x, property_handle:%x,status:%x\r\n", __FUNCTION__, server_id,
           service_handle, property_handle, status);
    sle_uuid_print(uuid);
}

/**
 * 删除所有服务回调函数
 */
void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status)
{
    printf("[%s] server_id:%x, status:%x\r\n", __FUNCTION__, server_id, status);
}

/**
 * 读请求回调函数
 */
void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para, errcode_t status)
{
    printf("[%s] server_id:%x, conn_id:%x, handle:%x, status:%x\r\n", __FUNCTION__, server_id, conn_id,
           read_cb_para->handle, status);
}

/**
 * 写请求回调函数 - 处理客户端发送的数据
 */
void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    unused(status);
    printf("[%s] server_id:%x, conn_id:%x, handle:%x\r\n", __FUNCTION__, server_id, conn_id, write_cb_para->handle);
    if ((write_cb_para->length > 0) && write_cb_para->value) {
        printf("recv len:%d data: ", write_cb_para->length);
        for (uint16_t i = 0; i < write_cb_para->length; i++) {
            printf("%c", write_cb_para->value[i]); // 打印接收到的数据内容
        }
        printf("\r\n");
    }
    for (int i = 0; i < g_num_conn; i++) {
        sle_read_remote_device_rssi(g_sle_send_param_arr[i].conn_id);
        printf("conn_id/total num: %d/%d\n", g_sle_send_param_arr[i].conn_id, g_num_conn);
        // 写入测试数据
        ssapc_write_param_t param = {0};
        param.handle = g_sle_send_param_arr[i].g_sle_send_param.handle;
        param.type = SSAP_PROPERTY_TYPE_VALUE;
        param.data_len = write_cb_para->length;
        param.data = write_cb_para->value;
        ssapc_write_req(0, g_sle_send_param_arr[i].conn_id, &param);
    }
}