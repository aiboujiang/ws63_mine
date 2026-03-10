#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "sle_device_discovery.h"
#include "securec.h"
#include "sle_errcode.h"

#include "terminal.h"

const char *local_name="Terminal";  // 本地设备名称
uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;  // 广播句柄，使用默认值
unsigned char local_addr[SLE_ADDR_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; // 本地设备MAC地址

static sle_announce_seek_callbacks_t g_sle_announce_seek_cbk = {0}; // 扫描和广播回调函数结构体

/* 星闪协议栈使能回调函数 */
static void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);  // 标记未使用参数，避免编译器警告
    printf("SLE enable.\r\n");  // 打印协议栈使能成功信息
}

/* ============= 广播回调函数 ============= */

/**
 * 广播使能回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
static void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[%s]  id:%02x, state:%x\r\n", __FUNCTION__, announce_id, status);  // 打印广播使能状态
}

/**
 * 广播禁用回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
static void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[%s] id:%02x, state:%x\r\n", __FUNCTION__, announce_id, status);  // 打印广播禁用状态
}

/**
 * 广播终止回调函数
 * @param announce_id 广播ID
 */
static void sle_announce_terminal_cbk(uint32_t announce_id)
{
    printf("[%s] id:%02x\r\n", __FUNCTION__, announce_id);  // 打印广播终止信息
}

/**
 * 注册广播和扫描回调函数
 * @return 错误码，成功返回ERRCODE_SLE_SUCCESS
 */
static errcode_t announce_seek_register_callbacks(void)
{
    errcode_t ret = 0;
    g_sle_announce_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;  // 协议栈使能回调
    g_sle_announce_seek_cbk.announce_enable_cb = sle_announce_enable_cbk;  // 广播使能回调
    g_sle_announce_seek_cbk.announce_disable_cb = sle_announce_disable_cbk;  // 广播禁用回调
    g_sle_announce_seek_cbk.announce_terminal_cb = sle_announce_terminal_cbk;  // 广播终止回调
    
    // 调用API注册回调函数
    ret = sle_announce_seek_register_callbacks(&g_sle_announce_seek_cbk);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);  // 注册失败打印错误信息
        return ret;
    }
    return ret;
}

/**
 * 设置广播参数
 * @return 错误码，成功返回ERRCODE_SLE_SUCCESS
 */
static errcode_t set_announce_param(void)
{
    errcode_t ret = 0;
    // 设置设备公开参数
    sle_announce_param_t param = {0};
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE; // 可连接可扫描模式
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT; // 广播句柄
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO; // 角色：发起方可协商
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL; // 普通广播级别
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT; // 默认广播信道映射
    param.announce_interval_min = SLE_ADV_INTERVAL_MIN_DEFAULT; // 最小广播间隔
    param.announce_interval_max = SLE_ADV_INTERVAL_MAX_DEFAULT; // 最大广播间隔
    param.conn_interval_min = SLE_CONN_INTV_MIN_DEFAULT; // 最小连接间隔
    param.conn_interval_max = SLE_CONN_INTV_MAX_DEFAULT; // 最大连接间隔
    param.conn_max_latency = SLE_CONN_MAX_LATENCY; // 最大连接延迟
    param.conn_supervision_timeout = SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT; // 连接监控超时
    param.announce_tx_power = SLE_ADV_TX_POWER; // 广播发射功率
    param.own_addr.type = 0; // 地址类型（0表示公共地址）
    
    // 拷贝本地地址到参数结构体
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] data memcpy fail\r\n", __FUNCTION__);  // 内存拷贝失败
        return ret;
    }
    
    // 设置广播参数
    ret = sle_set_announce_param(param.announce_handle, &param);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] data memcpy fail\r\n", __FUNCTION__);  // 参数设置失败
        return ret;
    }
    return ret;
}

/**
 * 设置广播数据
 * @return 错误码，成功返回ERRCODE_SLE_SUCCESS
 */
static errcode_t set_announce_data(void)
{
    uint8_t announce_data_len = 0;  // 广播数据长度
    sle_announce_data_t data = {0};  // 广播数据结构体
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0}; // 广播数据缓冲区
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0}; // 扫描响应数据缓冲区
    size_t len = 0;  // 临时长度变量
    uint16_t idx = 0; // 数据写入索引
    errcode_t ret = 0;

    // ============= 设置广播数据 =============
    
    // 设置发现级别字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1, // AD结构长度（不包括长度字段本身）
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL, // 发现级别类型
        .value = SLE_ANNOUNCE_LEVEL_NORMAL, // 普通发现级别
    };
    ret = memcpy_s(&announce_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] adv_disc_level memcpy fail\r\n", __FUNCTION__);  // 内存拷贝失败
        return ret;
    }
    idx += len;

    // 设置访问模式字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE, // 访问模式类型
        .value = 0, // 访问模式值
    };
    ret = memcpy_s(&announce_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] adv_access_mode memcpy fail\r\n", __FUNCTION__);  // 内存拷贝失败
        return ret;
    }
    idx += len;
    
    announce_data_len = idx;  // 记录广播数据总长度
    
    // 打印广播数据信息
    printf("[%s] data.announce_data_len = %d\r\n", __FUNCTION__, data.announce_data_len);
    printf("[%s] data.announce_data: ", __FUNCTION__);
    for (uint8_t data_index = 0; data_index < data.announce_data_len; data_index++) {
        printf("0x%02x ", data.announce_data[data_index]);  // 打印每个字节的十六进制值
    }
    printf("\r\n");

    // ============= 设置扫描响应数据 =============
    
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);
    idx = 0; // 重置数据写入索引
    
    // 设置发射功率级别字段
    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL, // 发射功率类型
        .value = SLE_ADV_TX_POWER, // 发射功率值
    };
    ret = memcpy_s(seek_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] scan response data memcpy fail\r\n", __FUNCTION__);  // 内存拷贝失败
        return ret;
    }
    idx += scan_rsp_data_len;

    // 设置AD结构：长度字段（名称长度去掉结束符+类型字段的1字节）
    seek_rsp_data[idx++] = sizeof(local_name)-1 + 1;
    
    // 设置AD结构：类型字段（0x09表示完整设备名称）
    seek_rsp_data[idx++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    
    // 拷贝设备名称到广播数据
    ret = memcpy_s(&seek_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx, local_name, sizeof(local_name)-1);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] memcpy fail\r\n", __FUNCTION__);  // 内存拷贝失败
        return ret;
    }
    idx += sizeof(local_name)-1;  // 更新索引
    
    // 填充数据结构体
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = idx;
    
    // 打印扫描响应数据信息
    printf("[%s] data.seek_rsp_data_len = %d\r\n", __FUNCTION__, data.seek_rsp_data_len);
    printf("[%s] data.seek_rsp_data: ", __FUNCTION__);
    for (uint8_t data_index = 0; data_index < data.seek_rsp_data_len; data_index++) {
        printf("0x%02x ", data.seek_rsp_data[data_index]);  // 打印每个字节的十六进制值
    }
    printf("\r\n");

    // 设置广播数据到协议栈
    ret = sle_set_announce_data(adv_handle, &data);
    if (ret == ERRCODE_SLE_SUCCESS) {
        printf("[%s] set announce data success.\r\n", __FUNCTION__);  // 设置成功
    } else {
        printf("[%s] set adv param fail.\r\n", __FUNCTION__);  // 设置失败
    }
    return ret;
}

/**
 * 星闪Terminal Node主任务
 * @param arg 任务参数
 * @return void* NULL
 */
static void *sle_terminal_task(const char *arg)
{
    unused(arg);  // 标记未使用参数，避免编译器警告
    errcode_t ret = 0;

    // 1. 注册回调函数
    announce_seek_register_callbacks();
    
    // 2. 打开星闪协议栈开关
    if (enable_sle() != ERRCODE_SUCC) {
        printf("SLE enbale failed !\r\n");  // 协议栈使能失败
    }
    
    // 3. 设置本地设备地址
    sle_addr_t LocalAddr={0};
    memcpy_s(LocalAddr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    LocalAddr.type=0;  // 地址类型：公共地址
    sle_set_local_addr(&LocalAddr);
    
    // 4. 设置本地设备名称
    sle_set_local_name((const uint8_t *) local_name, strlen(local_name));

    // 5. 设置广播参数
    set_announce_param();
    
    // 6. 设置广播数据
    set_announce_data();

    // 7. 开始广播
    ret = sle_start_announce(adv_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);  // 开始广播失败
        return NULL;
    }
    return NULL;
}

/**
 * 星闪Terminal Node入口函数
 */
static void sle_terminal_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();  // 获取内核线程锁

    // 创建星闪终端任务
    task_handle = osal_kthread_create((osal_kthread_handler)sle_terminal_task, 0, "sle_terminal_task", SLE_TERMINAL_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_TERMINAL_TASK_PRIO);  // 设置任务优先级
        osal_kfree(task_handle);  // 释放任务句柄内存
    }
    osal_kthread_unlock();  // 释放内核线程锁
}

/* 应用运行宏：启动星闪Terminal Node */
app_run(sle_terminal_entry);