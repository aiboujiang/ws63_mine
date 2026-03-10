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

#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "product.h"
#include "bts_le_gap.h"
#include "uart.h"
#include "pinctrl.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_02_client_multi_conn.h"
#include "app_init.h"

// 定义常量
#define SLE_MTU_SIZE_DEFAULT 512           // 默认MTU大小
#define SLE_SEEK_INTERVAL_DEFAULT 100      // 默认扫描间隔
#define SLE_SEEK_WINDOW_DEFAULT 100        // 默认扫描窗口
#define UUID_16BIT_LEN 2                   // 16位UUID长度
#define UUID_128BIT_LEN 16                 // 128位UUID长度
#define SLE_SERVER_NAME "sle_test"         // 目标服务器设备名称

#define QUEUE_MAX_NUM 20
// 全局变量定义
unsigned long g_msg_queue = 0;             // 消息队列句柄
unsigned int g_msg_rev_size = sizeof(msg_data_t); // 消息大小

/* 串口接收缓冲区大小 */
#define UART_RX_MAX 1024
uint8_t uart_rx_buffer[UART_RX_MAX];       // 串口接收缓冲区

// 静态变量定义
static ssapc_find_service_result_t g_sle_find_service_result = {0}; // 服务发现结果
static sle_announce_seek_callbacks_t g_sle_uart_seek_cbk = {0};     // 扫描回调结构体
static sle_connection_callbacks_t g_sle_uart_connect_cbk = {0};     // 连接回调结构体
static ssapc_callbacks_t g_sle_uart_ssapc_cbk = {0};               // SSAP客户端回调结构体
static sle_addr_t g_sle_remote_addr = {0};                         // 远程设备地址
ssapc_write_param_t g_sle_send_param = {0};                        // 发送参数

// 连接的设备管理
#define SLE_CLIENT_MAX_CON 4               // 最大连接设备数量

// 设备连接信息结构体
typedef struct server_conn {
    uint16_t g_sle_conn_id;                // 连接ID
    uint8_t conn_stat;                     // 连接状态：0-断开，1-连接
    unsigned char device_type;             // 设备类型（使用地址的第5字节标识）
} device_info;

// 记录已连接的设备数组
device_info sle_conn_device[SLE_CLIENT_MAX_CON] = {0};
uint16_t g_sle_conn_num = 0;               // 当前连接设备数量

/* 开启扫描函数 */
void sle_start_scan(void)
{
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;               // 自身地址类型
    param.filter_duplicates = 0;           // 是否过滤重复设备
    param.seek_filter_policy = 0;          // 扫描过滤策略
    param.seek_phys = 1;                   // 扫描物理通道
    param.seek_type[0] = 1;                // 扫描类型
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT; // 扫描间隔
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;     // 扫描窗口
    sle_set_seek_param(&param);            // 设置扫描参数
    sle_start_seek();                      // 开始扫描
}

/* 星闪协议栈使能回调 */
static void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);
    printf("sle enable.\r\n");
    sle_start_scan();                      // 协议栈使能后开始扫描
}

/* 扫描使能回调函数 */
static void sle_client_seek_enable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error\r\n", __FUNCTION__);
    }
}

/* 返回扫描结果回调 */
static void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data != NULL) {

        // 检查是否还可以连接新设备
        if (g_sle_conn_num < SLE_CLIENT_MAX_CON) {
            // 检查扫描到的设备名称是否匹配目标服务器名称
            if (strstr((const char *)seek_result_data->data, SLE_SERVER_NAME) != NULL) {
                // 拷贝目标设备地址到全局变量
                memcpy_s(&g_sle_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr,
                         sizeof(sle_addr_t));
                sle_stop_seek();           // 找到目标设备后停止扫描
            }
        }
    }
}

/* 扫描关闭回调函数 */
static void sle_client_seek_disable_cbk(errcode_t status)
{
    if (status != 0) {
        printf("[%s] status error = %x\r\n", __FUNCTION__, status);
    } else {
        sle_connect_remote_device(&g_sle_remote_addr); // 发送连接请求
    }
}

/* 扫描注册初始化函数 */
static void sle_client_seek_cbk_register(void)
{
    // 设置扫描相关的回调函数
    g_sle_uart_seek_cbk.sle_enable_cb = sle_client_sle_enable_cbk;
    g_sle_uart_seek_cbk.seek_enable_cb = sle_client_seek_enable_cbk;
    g_sle_uart_seek_cbk.seek_result_cb = sle_client_seek_result_info_cbk;
    g_sle_uart_seek_cbk.seek_disable_cb = sle_client_seek_disable_cbk;
    sle_announce_seek_register_callbacks(&g_sle_uart_seek_cbk); // 注册回调
}

/* 连接状态改变回调 */
static void sle_client_connect_state_changed_cbk(uint16_t conn_id,
                                                 const sle_addr_t *addr,
                                                 sle_acb_state_t conn_state,
                                                 sle_pair_state_t pair_state,
                                                 sle_disc_reason_t disc_reason)
{
    unused(addr);
    unused(pair_state);
    printf("[connect_state_changed] disc_reason:0x%x\r\n", disc_reason);

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        // 连接成功，将设备添加到连接列表
        for (uint8_t i = 0; i < SLE_CLIENT_MAX_CON; i++) {
            if (sle_conn_device[i].conn_stat == 0) { // 查找空闲位置
                sle_conn_device[i].g_sle_conn_id = conn_id;
                sle_conn_device[i].conn_stat = 1;
                sle_conn_device[i].device_type = addr->addr[5]; // 使用地址第5字节作为设备标识

                // 发起MTU交换请求
                ssap_exchange_info_t info = {0};
                info.mtu_size = SLE_MTU_SIZE_DEFAULT;
                info.version = 1;
                ssapc_exchange_info_req(0, sle_conn_device[i].g_sle_conn_id, &info);
                g_sle_conn_num++;          // 连接数量增加
                break;
            }
        }
        // 根据当前连接数量决定是否继续扫描
        if (g_sle_conn_num < SLE_CLIENT_MAX_CON) {
            sle_start_scan();              // 继续扫描其他设备
            printf("sle_start_scan\n ");
        } else {
            sle_stop_seek();               // 已达到最大连接数，停止扫描
            printf("device full\n ");
        }

    } else if (conn_state == SLE_ACB_STATE_NONE) {
        printf(" SLE_ACB_STATE_NONE\r\n");
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        // 设备断开连接，从连接列表中移除
        for (uint8_t j = 0; j < SLE_CLIENT_MAX_CON; j++) {
            if (sle_conn_device[j].device_type == addr->addr[5] && sle_conn_device[j].conn_stat == 1) {
                sle_conn_device[j].conn_stat = 0;
                sle_conn_device[j].device_type = 0;
                g_sle_conn_num--;          // 连接数量减少

                sle_start_scan();          // 重新开始扫描以补充连接
                break;
            }
        }
    } else {
        printf(" status error\r\n");
    }
}

/* 配对完成回调 */
void sle_client_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    unused(status);
    printf("[%s] conn_id:%d, state:%d,addr:%02x***%02x%02x\n", __FUNCTION__, conn_id, status, addr->addr[0],
           addr->addr[4], addr->addr[5]);
    if (status == 0) {
        // 配对成功，发起MTU交换
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, sle_conn_device[g_sle_conn_num].g_sle_conn_id, &info);
    }
}

/* 连接注册初始化函数 */
static void sle_client_connect_cbk_register(void)
{
    // 设置连接相关的回调函数
    g_sle_uart_connect_cbk.connect_state_changed_cb = sle_client_connect_state_changed_cbk;
    g_sle_uart_connect_cbk.pair_complete_cb = sle_client_pair_complete_cbk;
    sle_connection_register_callbacks(&g_sle_uart_connect_cbk); // 注册回调
}

/* 更新MTU回调 */
static void sle_client_exchange_info_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssap_exchange_info_t *param,
                                         errcode_t status)
{
    printf("[%s] pair complete client id:%d status:%d\r\n", __FUNCTION__, client_id, status);
    printf("[%s] mtu size: %d, version: %d.\r\n", __FUNCTION__, param->mtu_size, param->version);
    
    // MTU交换完成后开始服务发现
    ssapc_find_structure_param_t find_param = {0};
    find_param.type = SSAP_FIND_TYPE_PROPERTY; // 发现特征
    find_param.start_hdl = 1;               // 起始句柄
    find_param.end_hdl = 0xFFFF;            // 结束句柄
    ssapc_find_structure(0, conn_id, &find_param);
}

/* 发现服务回调 */
static void sle_client_find_structure_cbk(uint8_t client_id,
                                          uint16_t conn_id,
                                          ssapc_find_service_result_t *service,
                                          errcode_t status)
{
    printf("[%s] client: %d conn_id:%d status: %d \r\n", __FUNCTION__, client_id, conn_id, status);
    printf("[%s] find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n", __FUNCTION__,
           service->start_hdl, service->end_hdl, service->uuid.len);
    
    // 保存服务发现结果
    g_sle_find_service_result.start_hdl = service->start_hdl;
    g_sle_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

/* 发现特征回调 */
static void sle_client_find_property_cbk(uint8_t client_id,
                                         uint16_t conn_id,
                                         ssapc_find_property_result_t *property,
                                         errcode_t status)
{
    printf(
        "[%s] client id: %d, conn id: %d, operate ind: %d, "
        "descriptors count: %d status:%d property->handle %d\r\n",
        __FUNCTION__, client_id, conn_id, property->operate_indication, property->descriptors_count, status,
        property->handle);
    
    // 保存特征句柄用于后续数据发送
    g_sle_send_param.handle = property->handle;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
}

/* 发现特征完成回调 */
static void sle_client_find_structure_cmp_cbk(uint8_t client_id,
                                              uint16_t conn_id,
                                              ssapc_find_structure_result_t *structure_result,
                                              errcode_t status)
{
    unused(conn_id);
    printf("[%s] client id:%d status:%d type:%d uuid len:%d \r\n", __FUNCTION__, client_id, status,
           structure_result->type, structure_result->uuid.len);
}

/* 收到写响应回调 */
static void sle_client_write_cfm_cbk(uint8_t client_id,
                                     uint16_t conn_id,
                                     ssapc_write_result_t *write_result,
                                     errcode_t status)
{
    printf("[%s] conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n", __FUNCTION__, conn_id, client_id, status,
           write_result->handle, write_result->type);
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

/* SSAP客户端回调注册函数 */
static void sle_client_ssapc_cbk_register(void)
{
    // 设置SSAP客户端所有回调函数
    g_sle_uart_ssapc_cbk.exchange_info_cb = sle_client_exchange_info_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cb = sle_client_find_structure_cbk;
    g_sle_uart_ssapc_cbk.ssapc_find_property_cbk = sle_client_find_property_cbk;
    g_sle_uart_ssapc_cbk.find_structure_cmp_cb = sle_client_find_structure_cmp_cbk;
    g_sle_uart_ssapc_cbk.write_cfm_cb = sle_client_write_cfm_cbk;
    g_sle_uart_ssapc_cbk.notification_cb = sle_notification_cbk;
    g_sle_uart_ssapc_cbk.indication_cb = sle_indication_cbk;
    ssapc_register_callbacks(&g_sle_uart_ssapc_cbk); // 注册回调
}

/* 客户端初始化函数 */
void sle_client_init(void)
{
    osal_msleep(500); /* 500: 延时500ms等待系统稳定 */
    printf("sle enable.\r\n");
    sle_client_seek_cbk_register();        // 注册扫描回调
    sle_client_connect_cbk_register();     // 注册连接回调
    sle_client_ssapc_cbk_register();       // 注册SSAP客户端回调
    
    // 使能星闪协议栈
    if (enable_sle() != ERRCODE_SUCC) {
        printf("sle enbale fail !\r\n");
    }
}

/* 客户端主任务 */
static void *sle_client_task(char *arg)
{
    unused(arg);
    app_uart_init_config();                // 初始化串口配置
    sle_client_init();                     // 初始化客户端
    
    uint8_t send_id = 0;
    while (1) {
        msg_data_t msg_data = {0};
        // 从消息队列读取数据
        int msg_ret = osal_msg_queue_read_copy(g_msg_queue, &msg_data, &g_msg_rev_size, OSAL_WAIT_FOREVER);
        if (msg_ret != OSAL_SUCCESS) {
            printf("msg queue read copy fail.");
            if (msg_data.value != NULL) {
                osal_vfree(msg_data.value); // 释放内存
            }
        }
        
        // 处理接收到的数据
        if (msg_data.value != NULL) {
            ssapc_write_param_t *sle_uart_send_param = &g_sle_send_param;
            sle_uart_send_param->data_len = msg_data.value_len - 3; // 数据长度（去掉前3字节ID）
            sle_uart_send_param->data = msg_data.value + 3;         // 数据内容（跳过前3字节ID）
            
            // 解析目标设备ID（前3字节）
            send_id = atoi((const char *)msg_data.value);
            printf("send_id: %d\n", send_id);
            
            // 向指定设备发送数据
            ssapc_write_cmd(0, sle_conn_device[send_id].g_sle_conn_id, sle_uart_send_param);
        }
    }
    return NULL;
}

/* 客户端入口函数 */
static void sle_client_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    
    // 创建消息队列
    int ret = osal_msg_queue_create("sle_msg", QUEUE_MAX_NUM, &g_msg_queue, 0, g_msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        printf("create queue failure!,error:%x\n", ret);
    }

    // 创建客户端任务
    task_handle =
        osal_kthread_create((osal_kthread_handler)sle_client_task, 0, "sle_client_task", SLE_SERVER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO); // 设置任务优先级
        osal_kfree(task_handle);              // 释放任务句柄内存
    }
    osal_kthread_unlock();
}

/* 运行客户端入口函数 */
app_run(sle_client_entry);

/* 串口接收回调函数 */
void sle_uart_client_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    msg_data_t msg_data = {0};
    
    // 分配内存并拷贝接收到的数据
    void *buffer_cpy = osal_vmalloc(length);
    if (memcpy_s(buffer_cpy, length, buffer, length) != EOK) {
        osal_vfree(buffer_cpy);
        return;
    }
    
    msg_data.value = (uint8_t *)buffer_cpy;
    msg_data.value_len = length;
    
    // 将数据写入消息队列
    osal_msg_queue_write_copy(g_msg_queue, (void *)&msg_data, g_msg_rev_size, 0);
}

/* 串口初始化配置函数 */
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    
    // 配置串口引脚模式
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_PIN_MODE);
    
    // 配置串口属性
    uart_attr_t attr = {
        .baud_rate = 115200,               // 波特率：115200
        .data_bits = UART_DATA_BIT_8,      // 数据位：8位
        .stop_bits = UART_STOP_BIT_1,      // 停止位：1位
        .parity = UART_PARITY_NONE         // 校验位：无
    };
    
    // 配置串口缓冲区
    uart_buffer_config.rx_buffer_size = SLE_MTU_SIZE_DEFAULT; // 接收缓冲区大小
    uart_buffer_config.rx_buffer = uart_rx_buffer;            // 接收缓冲区指针
    
    // 配置串口引脚
    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO0,                // 发送引脚
        .rx_pin = S_MGPIO1,                // 接收引脚
        .cts_pin = PIN_NONE,               // 清除发送引脚：无
        .rts_pin = PIN_NONE                // 请求发送引脚：无
    };
    
    // 先去初始化串口
    uapi_uart_deinit(CONFIG_UART_ID);
    
    // 重新初始化串口
    int res = uapi_uart_init(CONFIG_UART_ID, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    
    // 注册串口接收回调函数
    if (uapi_uart_register_rx_callback(CONFIG_UART_ID, UART_RX_CONDITION_MASK_IDLE, 1, sle_uart_client_read_handler) ==
        ERRCODE_SUCC) {
        printf("uart%d int mode register receive callback succ!\r\n", CONFIG_UART_ID);
    }
}