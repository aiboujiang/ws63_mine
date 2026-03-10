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

const char *local_name = "Client";
unsigned char local_addr[SLE_ADDR_LEN] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01}; // 本地设备地址

uint8_t g_num_conn = 0; // 已连接的设备数量
sle_conn_and_service_t g_conn_and_service_arr[SLE_MAX_SERVER] = {0};
sle_write_param_t g_sle_send_param_arr[SLE_MAX_SERVER] = {0};
sle_addr_t g_addr_arr[SLE_MAX_SERVER] = {0};

sle_announce_seek_callbacks_t g_sle_announce_seek_cbk = {0}; // 扫描回调结构体
sle_connection_callbacks_t g_connect_cbk = {0};              // 连接管理回调
ssapc_callbacks_t g_sle_uart_ssapc_cbk = {0};                // SSAP客户端回调结构体

sle_addr_t g_remote_addr = {0}; // 远程设备地址
uint16_t g_conn_id = 0;         // 连接ID

/**
 * 星闪Grant Node主任务
 * @param arg 任务参数
 * @return void* NULL
 */
static void *sle_grant_task(const char *arg)
{
    unused(arg);
    uint8_t state = 0;
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

    while (1) {
        for (int i = 0; i < g_num_conn; i++) {
            sle_read_remote_device_rssi(g_sle_send_param_arr[i].conn_id);
            sle_get_pair_state(&g_addr_arr[i], &state);
            printf("conn_id/total num: %d/%d\n", g_sle_send_param_arr[i].conn_id,g_num_conn);
            printf("Paired state=%d\n", state);
            // 写入测试数据
            uint8_t data[] = "Hello!";
            uint8_t len = sizeof(data);
            ssapc_write_param_t param = {0};
            param.handle = g_sle_send_param_arr[i].g_sle_send_param.handle;
            param.type = SSAP_PROPERTY_TYPE_VALUE;
            param.data_len = len;
            param.data = data;
            ssapc_write_req(0, g_sle_send_param_arr[i].conn_id, &param);
            osal_msleep(1000);
        }
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