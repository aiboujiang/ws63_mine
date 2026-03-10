/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "product.h"
#include "sle_common.h"
#include "sle_server.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "string.h"
#include "sle_server_adv.h"

/* 星闪设备名称最大长度 */
#define NAME_MAX_LENGTH 9

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

/* 广播设备名称 */
static uint8_t g_sle_local_name[NAME_MAX_LENGTH] = "sle_test"; // 设备名称"sle_test"

/**
 * 设置广播数据中的本地设备名称
 * @param adv_data 广播数据缓冲区
 * @param max_len 缓冲区最大长度
 * @return uint16_t 写入的数据长度，0表示失败
 */
static uint16_t sle_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    errno_t ret;
    uint8_t index = 0; // 数据写入索引

    uint8_t *local_name = g_sle_local_name;
    uint8_t local_name_len = sizeof(g_sle_local_name) - 1; // 名称长度（去掉结束符）
    printf("[%s] local_name_len = %d\r\n", __FUNCTION__, local_name_len);
    printf("[%s] local_name: ", __FUNCTION__);
    
    // 打印设备名称的十六进制值
    for (uint8_t i = 0; i < local_name_len; i++) {
        printf("0x%02x ", local_name[i]);
    }
    printf("\r\n");
    
    // 设置AD结构：长度字段（名称长度+类型字段的1字节）
    adv_data[index++] = local_name_len + 1;
    
    // 设置AD结构：类型字段（0x09表示完整设备名称）
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;
    
    // 拷贝设备名称到广播数据
    ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (ret != EOK) {
        printf("[%s] memcpy fail\r\n", __FUNCTION__);
        return 0;
    }
    return (uint16_t)index + local_name_len; // 返回总写入长度
}

/**
 * 设置广播数据（主广播数据）
 * @param adv_data 广播数据缓冲区
 * @return uint16_t 写入的数据长度，0表示失败
 */
static uint16_t sle_set_adv_data(uint8_t *adv_data)
{
    size_t len = 0;
    uint16_t idx = 0; // 数据写入索引
    errno_t ret = 0;

    // 设置发现级别字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_disc_level = {
        .length = len - 1, // AD结构长度（不包括长度字段本身）
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL, // 发现级别类型
        .value = SLE_ANNOUNCE_LEVEL_NORMAL, // 普通发现级别
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_disc_level, len);
    if (ret != EOK) {
        printf("[%s] adv_disc_level memcpy fail\r\n", __FUNCTION__);
        return 0;
    }
    idx += len;

    // 设置访问模式字段
    len = sizeof(struct sle_adv_common_value);
    struct sle_adv_common_value adv_access_mode = {
        .length = len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE, // 访问模式类型
        .value = 0, // 访问模式值
    };
    ret = memcpy_s(&adv_data[idx], SLE_ADV_DATA_LEN_MAX - idx, &adv_access_mode, len);
    if (ret != EOK) {
        printf("[%s] adv_access_mode memcpy fail\r\n", __FUNCTION__);
        return 0;
    }
    idx += len;

    return idx; // 返回总写入长度
}

/**
 * 设置扫描响应数据
 * @param scan_rsp_data 扫描响应数据缓冲区
 * @return uint16_t 写入的数据长度，0表示失败
 */
static uint16_t sle_set_scan_response_data(uint8_t *scan_rsp_data)
{
    uint16_t idx = 0; // 数据写入索引
    errno_t ret;
    size_t scan_rsp_data_len = sizeof(struct sle_adv_common_value);

    // 设置发射功率级别字段
    struct sle_adv_common_value tx_power_level = {
        .length = scan_rsp_data_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL, // 发射功率类型
        .value = SLE_ADV_TX_POWER, // 发射功率值
    };
    ret = memcpy_s(scan_rsp_data, SLE_ADV_DATA_LEN_MAX, &tx_power_level, scan_rsp_data_len);
    if (ret != EOK) {
        printf("[%s] scan response data memcpy fail\r\n", __FUNCTION__);
        return 0;
    }
    idx += scan_rsp_data_len;

    /* 设置本地设备名称 */
    idx += sle_set_adv_local_name(&scan_rsp_data[idx], SLE_ADV_DATA_LEN_MAX - idx);
    return idx; // 返回总写入长度
}

/**
 * 设置默认广播参数
 * @return int 错误码，0表示成功
 */
static int sle_set_default_announce_param(void)
{
    errno_t ret;
    sle_announce_param_t param = {0};
    uint8_t index;
    unsigned char local_addr[SLE_ADDR_LEN] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x07}; // 本地设备地址
    
    // 配置广播参数
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
    param.own_addr.type = 0; // 地址类型
    
    // 拷贝本地地址
    ret = memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    if (ret != EOK) {
        printf("[%s] data memcpy fail\r\n", __FUNCTION__);
        return 0;
    }
    
    // 打印本地地址
    printf("[%s] sle_local addr: ", __FUNCTION__);
    for (index = 0; index < SLE_ADDR_LEN; index++) {
        printf("0x%02x ", param.own_addr.addr[index]);
    }
    printf("\r\n");
    
    // 设置广播参数
    return sle_set_announce_param(param.announce_handle, &param);
}

/**
 * 设置默认广播数据
 * @return int 错误码，0表示成功
 */
static int sle_set_default_announce_data(void)
{
    errcode_t ret;
    uint8_t announce_data_len = 0;
    uint8_t seek_data_len = 0;
    sle_announce_data_t data = {0};
    uint8_t adv_handle = SLE_ADV_HANDLE_DEFAULT;
    uint8_t announce_data[SLE_ADV_DATA_LEN_MAX] = {0}; // 广播数据缓冲区
    uint8_t seek_rsp_data[SLE_ADV_DATA_LEN_MAX] = {0}; // 扫描响应数据缓冲区
    uint8_t data_index = 0;

    // 设置广播数据
    announce_data_len = sle_set_adv_data(announce_data);
    data.announce_data = announce_data;
    data.announce_data_len = announce_data_len;

    // 打印广播数据信息
    printf("[%s] data.announce_data_len = %d\r\n", __FUNCTION__, data.announce_data_len);
    printf("[%s] data.announce_data: ", __FUNCTION__);
    for (data_index = 0; data_index < data.announce_data_len; data_index++) {
        printf("0x%02x ", data.announce_data[data_index]);
    }
    printf("\r\n");

    // 设置扫描响应数据
    seek_data_len = sle_set_scan_response_data(seek_rsp_data);
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_data_len;

    // 打印扫描响应数据信息
    printf("[%s] data.seek_rsp_data_len = %d\r\n", __FUNCTION__, data.seek_rsp_data_len);
    printf("[%s] data.seek_rsp_data: ", __FUNCTION__);
    for (data_index = 0; data_index < data.seek_rsp_data_len; data_index++) {
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
    return ERRCODE_SLE_SUCCESS;
}

/* ============= 广播回调函数 ============= */

/**
 * 广播使能回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
static void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[%s]  id:%02x, state:%x\r\n", __FUNCTION__, announce_id, status);
}

/**
 * 广播禁用回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
static void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[%s] id:%02x, state:%x\r\n", __FUNCTION__, announce_id, status);
}

/**
 * 广播终止回调函数
 * @param announce_id 广播ID
 */
static void sle_announce_terminal_cbk(uint32_t announce_id)
{
    printf("[%s] id:%02x\r\n", __FUNCTION__, announce_id);
}

/**
 * 注册广播回调函数
 * @return errcode_t 错误码
 */
errcode_t sle_announce_register_cbks(void)
{
    errcode_t ret = 0;
    sle_announce_seek_callbacks_t seek_cbks = {0};
    
    // 设置广播回调函数
    seek_cbks.announce_enable_cb = sle_announce_enable_cbk;
    seek_cbks.announce_disable_cb = sle_announce_disable_cbk;
    seek_cbks.announce_terminal_cb = sle_announce_terminal_cbk;
    
    // 注册回调函数
    ret = sle_announce_seek_register_callbacks(&seek_cbks);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}

/**
 * 星闪服务器广播初始化
 * @return errcode_t 错误码
 */
errcode_t sle_server_adv_init(void)
{
    errcode_t ret;
    
    // 设置广播参数和数据
    sle_set_default_announce_param();
    sle_set_default_announce_data();
    
    // 开始广播
    ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    if (ret != ERRCODE_SLE_SUCCESS) {
        printf("[%s] fail :%x\r\n", __FUNCTION__, ret);
        return ret;
    }
    return ERRCODE_SLE_SUCCESS;
}