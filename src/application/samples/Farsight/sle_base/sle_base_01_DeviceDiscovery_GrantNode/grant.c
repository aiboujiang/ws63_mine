#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "sle_device_discovery.h"
#include "securec.h"

#include "grant.h"

const char *local_name = "Grant";  // 本地设备名称
unsigned char local_addr[SLE_ADDR_LEN] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01}; // 本地设备地址，从高到低排列

static sle_announce_seek_callbacks_t g_sle_announce_seek_cbk = {0}; // 设备扫描相关回调函数结构体

/* 星闪协议栈使能回调函数 */
static void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);  // 标记未使用的参数，避免编译警告
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
        printf("Seek result:");  // 打印扫描结果标签
        // 遍历并打印扫描到的设备数据（十六进制格式）
        for (int i = 0; i < seek_result_data->data_length; i++) {
            printf("%02x", seek_result_data->data[i]);
        }
        printf("\n");  // 换行
        
        printf("Addr:");  // 打印设备地址标签
        // 遍历并打印设备地址（六字节，十六进制格式）
        for (int i = 0; i < SLE_ADDR_LEN; i++) {
            printf("%02x", seek_result_data->addr.addr[i]);
        }
        printf("\n");  // 换行
        
        printf("RSSI:%d\n", seek_result_data->rssi);  // 打印接收信号强度指示值
    }
}

/* 扫描关闭回调函数 */
static void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error = %x\r\n", __FUNCTION__, status);  // 扫描关闭失败，打印错误码
    } else {
        printf("Seek disable.\n");  // 扫描关闭成功
    }
}

/**
 * 星闪Grant Node主任务函数
 * @param arg 任务参数（未使用）
 * @return void* 总是返回NULL
 */
static void *sle_grant_task(const char *arg)
{
    unused(arg);  // 标记未使用的参数，避免编译警告

    // 注册设备公开和设备发现相关的回调函数
    g_sle_announce_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;
    g_sle_announce_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;
    g_sle_announce_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk;
    g_sle_announce_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_announce_seek_cbk); // 注册所有回调函数到系统
    
    // 打开SLE协议栈开关
    if (enable_sle() != ERRCODE_SUCC) {
        printf("SLE enbale failed !\r\n");  // 协议栈使能失败
    }

    // 设置本地设备地址
    sle_addr_t LocalAddr = {0};  // 声明并初始化本地地址结构体
    memcpy_s(LocalAddr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);  // 安全拷贝地址数据
    LocalAddr.type = 0;  // 设置地址类型
    sle_set_local_addr(&LocalAddr);  // 应用本地地址设置
    
    // 设置本地设备名称
    sle_set_local_name((const uint8_t *)local_name, strlen(local_name));  // 设置设备名称及长度

    // 配置扫描参数
    sle_seek_param_t param = {0};  // 声明并初始化扫描参数结构体
    param.own_addr_type = 0;  // 自身地址类型
    param.filter_duplicates = 0;  // 是否过滤重复设备
    param.seek_filter_policy = 0;  // 扫描过滤策略
    param.seek_phys = 1;  // 物理层类型
    param.seek_type[0] = 1;  // 扫描类型
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;  // 使用默认扫描间隔
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;  // 使用默认扫描窗口

    // 设置扫描参数并开始扫描
    sle_set_seek_param(&param);  // 应用扫描参数设置
    sle_start_seek();  // 启动设备扫描
    
    return NULL;  // 任务函数返回NULL
}

/**
 * 星闪Grant Node入口函数
 * 负责创建并启动主任务
 */
static void sle_grant_entry(void)
{
    osal_task *task_handle = NULL;  // 任务句柄指针
    osal_kthread_lock();  // 内核线程上锁，确保任务创建原子性

    // 创建星闪服务器任务
    task_handle = osal_kthread_create((osal_kthread_handler)sle_grant_task, 0, "sle_grant_task", SLE_GRANT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_GRANT_TASK_PRIO);  // 设置任务优先级
        osal_kfree(task_handle);  // 释放任务句柄内存（任务已由内核管理）
    }
    osal_kthread_unlock();  // 内核线程解锁
}

/* 应用运行宏：启动星闪Grant Node */
app_run(sle_grant_entry);  // 应用入口宏，启动整个程序