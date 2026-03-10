#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "securec.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_device_discovery.h"
#include "grant.h"
#include <string.h>

#define SLE_SERVER_NAME "Terminal"  // 要连接的远程设备名称

const char *local_name = "Grant";  // 本地设备名称
unsigned char local_addr[SLE_ADDR_LEN] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01};  // 本地设备地址

static sle_announce_seek_callbacks_t g_sle_announce_seek_cbk = {0};  // 设备扫描相关的回调函数结构体
static sle_connection_callbacks_t g_connect_cbk = {0};               // 连接管理相关的回调函数结构体

static sle_addr_t g_remote_addr = {0};  // 保存远程设备的地址
static uint16_t g_conn_id = 0;          // 保存连接ID，用于后续通信

/* 星闪协议栈使能回调函数 */
static void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);
    printf("SLE enable.\r\n");  // 打印协议栈使能成功信息
}

/* 扫描使能回调函数 */
static void sle_client_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error\r\n", __FUNCTION__);  // 扫描使能失败，打印错误信息
    } else {
        printf("Seek enable.\n");  // 扫描使能成功
    }
}

/* 返回扫描结果回调函数 */
static void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data != NULL) {
        printf("[seek_result_info_cbk] scan data : ");  // 打印扫描到的设备数据（十六进制格式）
        for (uint8_t i = 0; i < seek_result_data->data_length; i++) {
            printf("0x%X ", seek_result_data->data[i]);
        }
        printf("\r\n");
        // 检查扫描到的设备名称是否包含目标设备名
        if (strstr((const char *)seek_result_data->data, SLE_SERVER_NAME) != NULL) {
            // 名称匹配成功，保存远程设备地址
            memcpy_s(&g_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr,
                     sizeof(sle_addr_t));
            sle_stop_seek();  // 停止扫描，因为已经找到目标设备
        }
    }
}

/* 扫描关闭回调函数 */
static void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error = %x\r\n", __FUNCTION__, status);  // 扫描关闭失败，打印错误码
    } else {
        // 扫描关闭成功，先删除之前的配对记录（如果有），然后尝试连接远程设备
        sle_remove_paired_remote_device(&g_remote_addr);
        sle_connect_remote_device(&g_remote_addr);
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
    // 打印连接状态变化信息，包括连接ID和设备地址（部分）
    osal_printk("[grant] conn state changed conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0], addr->addr[4],
                addr->addr[5]);
    osal_printk("[grant] conn state changed disc_reason:0x%x\n", disc_reason);

    // 如果连接成功建立
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        // 如果当前未配对，则发起配对请求
        if (pair_state == SLE_PAIR_NONE) {
            sle_pair_remote_device(&g_remote_addr);
        }
        g_conn_id = conn_id;  // 保存连接ID供后续使用
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
    // 打印配对完成信息，包括连接ID和设备地址（部分）
    osal_printk("[grant] pair complete conn_id:%d, addr:%02x***%02x%02x\n", conn_id, addr->addr[0], addr->addr[4],
                addr->addr[5]);

    // 如果配对成功，执行MTU信息交换
    if (status == 0) {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;  // 设置默认MTU大小
        info.version = 1;  // 设置版本号
        ssapc_exchange_info_req(1, g_conn_id, &info);  // 发起MTU交换请求
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
    // 打印连接参数更新请求中的最小和最大连接间隔
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
    // 打印连接参数更新后的实际连接间隔
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
    osal_printk("rssi average = %d dbm\r\n", rssi);  // 打印平均RSSI值（信号强度）
}

/**
 * 星闪Grant Node主任务
 * @param arg 任务参数
 * @return void* NULL
 */
static void *sle_grant_task(const char *arg)
{
    unused(arg);
    uint8_t state = 0;
    // 注册设备扫描相关的回调函数
    g_sle_announce_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;
    g_sle_announce_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;
    g_sle_announce_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk;
    g_sle_announce_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_announce_seek_cbk);  // 注册回调函数

    // 注册连接管理相关的回调函数
    g_connect_cbk.connect_state_changed_cb = sle_sample_connect_state_changed_cbk;
    g_connect_cbk.pair_complete_cb = sle_sample_pair_complete_cbk;
    g_connect_cbk.connect_param_update_req_cb = sle_sample_update_req_cbk;
    g_connect_cbk.connect_param_update_cb = sle_sample_update_cbk;
    g_connect_cbk.read_rssi_cb = sle_sample_read_rssi_cbk;
    sle_connection_register_callbacks(&g_connect_cbk);
    
    // 打开SLE协议栈开关
    if (enable_sle() != ERRCODE_SUCC) {
        printf("SLE enbale failed !\r\n");  // 协议栈使能失败
    }

    // 设置本地设备地址
    sle_addr_t LocalAddr = {0};
    memcpy_s(LocalAddr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    LocalAddr.type = 0;
    sle_set_local_addr(&LocalAddr);
    
    // 设置本地设备名称
    sle_set_local_name((const uint8_t *)local_name, strlen(local_name));

    // 配置扫描参数
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;  // 使用默认扫描间隔
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;      // 使用默认扫描窗口

    // 设置扫描参数并开始扫描
    sle_set_seek_param(&param);
    sle_start_seek();
    
    // 主循环：定期读取RSSI和配对状态
    while (1) {
        sle_read_remote_device_rssi(g_conn_id);  // 读取远程设备RSSI
        sle_get_pair_state(&g_remote_addr, &state);  // 获取配对状态
        printf("Paired state=%d\n", state);  // 打印配对状态
        osal_msleep(2000);  // 休眠2秒
    }
    return NULL;
}

/**
 * 星闪Grant Node入口函数
 */
static void sle_grant_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();  // 内核线程上锁，确保任务创建原子性

    // 创建星闪Grant Node任务
    task_handle = osal_kthread_create((osal_kthread_handler)sle_grant_task, 0, "sle_grant_task", SLE_GRANT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_GRANT_TASK_PRIO);  // 设置任务优先级
        osal_kfree(task_handle);  // 释放任务句柄内存
    }
    osal_kthread_unlock();  // 内核线程解锁
}

/* 应用运行宏：启动星闪Grant Node */
app_run(sle_grant_entry);