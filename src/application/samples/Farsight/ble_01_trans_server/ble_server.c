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

/* 包含必要的头文件 */
#include "osal_addr.h"               // 操作系统抽象层地址处理
#include "bts_def.h"                 // 蓝牙协议栈定义
#include "securec.h"                 // 安全内存操作函数
#include "errcode.h"                 // 错误码定义
#include "bts_def.h"                 // 蓝牙协议栈定义（重复包含）
#include "bts_le_gap.h"              // BLE GAP层头文件
#include "bts_gatt_stru.h"           // BLE GATT结构定义
#include "bts_gatt_server.h"         // BLE GATT服务器头文件
#include "soc_osal.h"                // 操作系统抽象层
#include "app_init.h"                // 应用初始化
#include "ble_server_adv.h"          // BLE服务器广播功能
#include "ble_server.h"              // BLE服务器功能
#include "common_def.h"              // 通用定义
#include "platform_core.h"           // 平台核心功能

/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;      // 数据指针
    uint16_t value_len;  // 数据长度
} msg_data_t;

/* 消息队列结构体 */
unsigned long g_msg_queue = 0;                   // 消息队列句柄
unsigned int g_msg_rev_size = sizeof(msg_data_t); // 消息大小

/* ble连接状态 */
uint8_t g_connection_state = 0;                  // BLE连接状态，0表示未连接

/* 任务相关配置 */
#define BLE_SERVER_TASK_PRIO 24                  // BLE服务器任务优先级
#define BLE_SERVER_STACK_SIZE 0x2000             // BLE服务器任务栈大小（8KB）

/* 串口接收缓冲区大小 */
#define UART_RX_MAX 512                          // 串口接收缓冲区大小512字节
uint8_t g_uart_rx_buffer[UART_RX_MAX];           // 串口接收缓冲区

/* 串口引脚配置 */
#define CONFIG_UART0_TXD_PIN 17                  // 串口0发送引脚
#define CONFIG_UART0_RXD_PIN 18                  // 串口0接收引脚
#define CONFIG_UART0_PIN_MODE 1                  // 引脚模式

/* 最大传输单元，默认是512字节 */
static uint16_t g_uart_mtu = 512;                // MTU大小

/* 服务器应用UUID用于测试 */
char g_uuid_app_uuid[] = {0x0, 0x0};             // 应用UUID（16位）

/* BLE服务器地址：12:34:56:78:90:00 */
static uint8_t g_ble_uart_server_addr[] = {0x12, 0x34, 0x56, 0x78, 0x90, 0x00};

/* BLE通知特征属性句柄 */
uint16_t g_notification_characteristic_att_hdl = 0; // 用于通知的特征值句柄

/* BLE连接句柄 */
uint16_t g_conn_hdl = 0;                          // 连接标识符

/* BLE服务器ID */
uint8_t g_server_id = 0;                          // 服务器标识符

/* 16位UUID长度定义 */
#define UUID_LEN_2 2                              // 2字节UUID长度

/* 
 * 将uint16类型的UUID数字转换为bt_uuid_t结构体
 * @param uuid_data: 16位UUID数据
 * @param out_uuid: 输出的UUID结构体
 */
void stream_data_to_uuid(uint16_t uuid_data, bt_uuid_t *out_uuid)
{
    char uuid[] = {(uint8_t)(uuid_data >> 8), (uint8_t)uuid_data}; // 保存服务的高8位和低8位
    out_uuid->uuid_len = UUID_LEN_2;
    if (memcpy_s(out_uuid->uuid, out_uuid->uuid_len, uuid, UUID_LEN_2) != EOK) { // 将服务的uuid拷贝出来
        return;
    }
}

/* 
 * 比较两个UUID是否相同
 * @param uuid1: 第一个UUID
 * @param uuid2: 第二个UUID
 * @return errcode_t: 相同返回SUCCESS，不同返回FAIL
 */
errcode_t compare_service_uuid(bt_uuid_t *uuid1, bt_uuid_t *uuid2)
{
    if (uuid1->uuid_len != uuid2->uuid_len) {
        return ERRCODE_BT_FAIL;
    }
    if (memcmp(uuid1->uuid, uuid2->uuid, uuid1->uuid_len) != 0) {
        return ERRCODE_BT_FAIL;
    }
    return ERRCODE_BT_SUCCESS;
}

/* 
 * 添加HID服务的所有特征和描述符，包括客户端特性配置描述符
 * @param server_id: 服务器ID
 * @param srvc_handle: 服务句柄
 */
static void ble_server_add_characters_and_descriptors(uint32_t server_id, uint32_t srvc_handle)
{
    bt_uuid_t server_uuid = {0};
    uint8_t server_value[] = {0x12, 0x34}; // 特征初始值
    osal_printk("[ble_server_add_characters] Beginning add characteristic\r\n");
    
    // 转换报告特征UUID
    stream_data_to_uuid(BLE_UUID_UUID_SERVER_REPORT, &server_uuid);
    
    // 配置特征属性：可读、可通知、可无响应写入
    gatts_add_chara_info_t character;
    character.chara_uuid = server_uuid;
    character.properties = GATT_CHARACTER_PROPERTY_BIT_READ | GATT_CHARACTER_PROPERTY_BIT_NOTIFY |
                           GATT_CHARACTER_PROPERTY_BIT_WRITE_NO_RSP;
    character.permissions = GATT_ATTRIBUTE_PERMISSION_READ | GATT_ATTRIBUTE_PERMISSION_WRITE;
    character.value_len = sizeof(server_value);
    character.value = server_value;
    
    // 添加特征
    gatts_add_characteristic(server_id, srvc_handle, &character);

    // 添加客户端特性配置描述符（CCC）
    bt_uuid_t ccc_uuid = {0};
    uint8_t ccc_data_val[] = {0x00, 0x00}; // CCC初始值：通知禁用
    osal_printk("[ble_server_add_descriptors] Beginning add descriptors\r\n");
    
    // 转换CCC描述符UUID
    stream_data_to_uuid(BLE_UUID_CLIENT_CHARACTERISTIC_CONFIGURATION, &ccc_uuid);
    
    // 配置描述符属性
    gatts_add_desc_info_t descriptor;
    descriptor.desc_uuid = ccc_uuid;
    descriptor.permissions = GATT_ATTRIBUTE_PERMISSION_READ | GATT_ATTRIBUTE_PERMISSION_WRITE;
    descriptor.value_len = sizeof(ccc_data_val);
    descriptor.value = ccc_data_val;
    
    // 添加描述符
    gatts_add_descriptor(server_id, srvc_handle, &descriptor);
    osal_vfree(ccc_uuid.uuid); // 释放UUID内存
}

/* 
 * 服务添加回调函数
 * @param server_id: 服务器ID
 * @param uuid: 服务UUID
 * @param handle: 服务句柄
 * @param status: 操作状态
 */
static void ble_server_service_add_cbk(uint8_t server_id, bt_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    bt_uuid_t service_uuid = {0};
    osal_printk(
        "ble_server_service_add_cbk] Add service cbk: server: %d, status: %d, srv_handle: %d, uuid_len: %d,uuid:",
        server_id, status, handle, uuid->uuid_len);
    
    // 打印UUID内容
    for (int8_t i = 0; i < uuid->uuid_len; i++) {
        osal_printk("%02x", (uint8_t)uuid->uuid[i]);
    }
    osal_printk("\n");
    
    // 检查是否为目标服务UUID
    stream_data_to_uuid(BLE_UUID_UUID_SERVER_SERVICE, &service_uuid);
    if (compare_service_uuid(uuid, &service_uuid) == ERRCODE_BT_SUCCESS) {
        // 添加特征和描述符
        ble_server_add_characters_and_descriptors(server_id, handle);
        osal_printk("[ble_server_service_add_cbk] Start service\r\n");
        // 启动服务
        gatts_start_service(server_id, handle);
    } else {
        osal_printk("[ble_server_service_add_cbk] Unknown service uuid\r\n");
        return;
    }
}

/* 
 * 特征添加回调函数
 * @param server_id: 服务器ID
 * @param uuid: 特征UUID
 * @param service_handle: 服务句柄
 * @param result: 添加结果
 * @param status: 操作状态
 */
static void ble_server_characteristic_add_cbk(uint8_t server_id,
                                              bt_uuid_t *uuid,
                                              uint16_t service_handle,
                                              gatts_add_character_result_t *result,
                                              errcode_t status)
{
    int8_t i = 0;
    osal_printk(
        "[ble_server_characteristic_add_cbk] Add characteristic cbk: server: %d, status: %d, srv_hdl: %d "
        "char_hdl: %x, char_val_hdl: %x, uuid_len: %d, uuid: ",
        server_id, status, service_handle, result->handle, result->value_handle, uuid->uuid_len);
    
    // 打印UUID内容
    for (i = 0; i < uuid->uuid_len; i++) {
        osal_printk("%02x", (uint8_t)uuid->uuid[i]);
    }
    osal_printk("\n");
    
    // 保存通知特征的值句柄
    g_notification_characteristic_att_hdl = result->value_handle;
}

/* 
 * 描述符添加回调函数
 * @param server_id: 服务器ID
 * @param uuid: 描述符UUID
 * @param service_handle: 服务句柄
 * @param handle: 描述符句柄
 * @param status: 操作状态
 */
static void ble_server_descriptor_add_cbk(uint8_t server_id,
                                          bt_uuid_t *uuid,
                                          uint16_t service_handle,
                                          uint16_t handle,
                                          errcode_t status)
{
    int8_t i = 0;
    osal_printk(
        "[ble_server_descriptor_add_cbk] Add descriptor cbk : server: %d, status: %d, srv_hdl: %d, desc_hdl: %x ,"
        "uuid_len:%d, uuid: ",
        server_id, status, service_handle, handle, uuid->uuid_len);
    
    // 打印UUID内容
    for (i = 0; i < uuid->uuid_len; i++) {
        osal_printk("%02x", (uint8_t)uuid->uuid[i]);
    }
    osal_printk("\n");
}

/* 
 * 服务启动回调函数
 * @param server_id: 服务器ID
 * @param handle: 服务句柄
 * @param status: 操作状态
 */
static void ble_server_service_start_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("[ble_server_service_start_cbk] Start service cbk : server: %d status: %d srv_hdl: %d\n", server_id,
                handle, status);
}

/* 
 * 写特征值请求回调函数
 * @param server_id: 服务器ID
 * @param conn_id: 连接ID
 * @param write_cb_para: 写请求参数
 * @param status: 操作状态
 */
static void ble_server_receive_write_req_cbk(uint8_t server_id,
                                             uint16_t conn_id,
                                             gatts_req_write_cb_t *write_cb_para,
                                             errcode_t status)
{
    osal_printk("[ble_server_receive_write_req_cbk]server_id:%d conn_id:%d,status:%d ", server_id, conn_id, status);
    osal_printk("request_id:%d att_handle:%d\n", write_cb_para->request_id, write_cb_para->handle);
    osal_printk("data_len:%d data:\n", write_cb_para->length);
    
    // 打印接收到的数据内容
    for (uint16_t i = 0; i < write_cb_para->length; i++) {
        osal_printk("%c", write_cb_para->value[i]);
    }
    osal_printk("\n");
}

/* 
 * 读特征值请求回调函数
 * @param server_id: 服务器ID
 * @param conn_id: 连接ID
 * @param read_cb_para: 读请求参数
 * @param status: 操作状态
 */
static void ble_server_receive_read_req_cbk(uint8_t server_id,
                                            uint16_t conn_id,
                                            gatts_req_read_cb_t *read_cb_para,
                                            errcode_t status)
{
    osal_printk("[ble_server_receive_read_req_cbk]server_id:%d conn_id:%d,status:%d\n", server_id, conn_id, status);
    osal_printk("request_id:%d att_handle:%d \n", read_cb_para->request_id, read_cb_para->handle);
}

/* 
 * 广播使能回调函数
 * @param adv_id: 广播ID
 * @param status: 广播状态
 */
static void ble_server_adv_enable_cbk(uint8_t adv_id, adv_status_t status)
{
    osal_printk("[ble_server_adv_enable_cbk] Adv enable adv_id: %d, status:%d\n", adv_id, status);
}

/* 
 * 广播禁用回调函数
 * @param adv_id: 广播ID
 * @param status: 广播状态
 */
static void ble_server_adv_disable_cbk(uint8_t adv_id, adv_status_t status)
{
    osal_printk("[ble_server_adv_disable_cbk] adv_id: %d, status:%d\n", adv_id, status);
}

/* 
 * 连接状态改变回调函数
 * @param conn_id: 连接ID
 * @param addr: 设备地址
 * @param conn_state: 连接状态
 * @param pair_state: 配对状态
 * @param disc_reason: 断开原因
 */
void ble_server_connect_change_cbk(uint16_t conn_id,
                                   bd_addr_t *addr,
                                   gap_ble_conn_state_t conn_state,
                                   gap_ble_pair_state_t pair_state,
                                   gap_ble_disc_reason_t disc_reason)
{
    osal_printk(
        "[ble_server_connect_change_cbk] Connect state change conn_id: %d, status: %d, pair_status:%d, addr %x "
        "disc_reason %x\n",
        conn_id, conn_state, pair_state, addr[0], disc_reason);
    g_conn_hdl = conn_id; // 保存连接句柄
    g_connection_state = (uint8_t)conn_state; // 更新连接状态
}

/* 
 * MTU改变回调函数
 * @param server_id: 服务器ID
 * @param conn_id: 连接ID
 * @param mtu_size: MTU大小
 * @param status: 操作状态
 */
void ble_server_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, uint16_t mtu_size, errcode_t status)
{
    osal_printk("[ble_server_mtu_changed_cbk] Mtu change change server_id: %d, conn_id: %d, mtu_size: %d, status:%d \n",
                server_id, conn_id, mtu_size, status);
}

/* 
 * BLE使能回调函数
 * @param status: 使能状态
 */
void ble_server_enable_cbk(errcode_t status)
{
    osal_printk("[ble_server_enable_cbk] Ble server enable status: %d\n", status);
    // 设置MTU大小
    if (gatts_set_mtu_size(g_server_id, g_uart_mtu) == ERRCODE_SUCC) {
        osal_printk("gatts_set_mtu_size 100\n");
    }
}

/* 
 * 创建服务函数
 * @return uint8_t: 操作结果
 */
uint8_t ble_uuid_add_service(void)
{
    osal_printk("[ble_uuid_add_service] Ble uuid add service in\r\n");
    bt_uuid_t service_uuid = {0};
    // 将UUID转换到结构体中
    stream_data_to_uuid(BLE_UUID_UUID_SERVER_SERVICE, &service_uuid);
    // 添加服务（主服务）
    gatts_add_service(g_server_id, &service_uuid, true);
    osal_printk("[ble_uuid_add_service] Ble uuid add service out\r\n");
    return ERRCODE_BT_SUCCESS;
}

/* 
 * 发送报告数据（通知）
 * @param data: 要发送的数据
 * @param len: 数据长度
 * @return errcode_t: 操作结果
 */
errcode_t ble_server_send_report(uint8_t *data, uint16_t len)
{
    gatts_ntf_ind_t param = {0};
    uint16_t conn_id = g_conn_hdl;
    param.attr_handle = g_notification_characteristic_att_hdl; // 通知特征句柄
    param.value_len = len;
    param.value = data;
    osal_printk("send input report indicate_handle:%d\n", g_notification_characteristic_att_hdl);
    // 发送通知
    gatts_notify_indicate(g_server_id, conn_id, &param);
    return ERRCODE_BT_SUCCESS;
}

/* 
 * 注册BLE服务器回调函数
 * @return errcode_t: 操作结果
 */
static errcode_t ble_server_register_callbacks(void)
{
    errcode_t ret;
    gap_ble_callbacks_t gap_cb = {0};
    gatts_callbacks_t service_cb = {0};

    // 注册GAP层回调
    gap_cb.start_adv_cb = ble_server_adv_enable_cbk;             // 开启广播回调函数
    gap_cb.stop_adv_cb = ble_server_adv_disable_cbk;             // 广播关闭回调函数
    gap_cb.conn_state_change_cb = ble_server_connect_change_cbk; // 连接状态改变回调函数
    gap_cb.ble_enable_cb = ble_server_enable_cbk;                // 开启蓝牙回调函数
    ret = gap_ble_register_callbacks(&gap_cb);
    if (ret != ERRCODE_BT_SUCCESS) {
        osal_printk("[ ble_server_register_callbacks ] Reg gap cbk failed\r\n");
        return ERRCODE_BT_FAIL;
    }

    // 注册GATT层回调
    service_cb.add_service_cb = ble_server_service_add_cbk;          // 服务添加回调
    service_cb.add_characteristic_cb = ble_server_characteristic_add_cbk; // 特征添加回调
    service_cb.add_descriptor_cb = ble_server_descriptor_add_cbk;    // 描述符添加回调
    service_cb.start_service_cb = ble_server_service_start_cbk;      // 服务启动回调
    service_cb.read_request_cb = ble_server_receive_read_req_cbk;    // 读请求回调
    service_cb.write_request_cb = ble_server_receive_write_req_cbk;  // 写请求回调
    service_cb.mtu_changed_cb = ble_server_mtu_changed_cbk;          // MTU改变回调
    ret = gatts_register_callbacks(&service_cb);
    if (ret != ERRCODE_BT_SUCCESS) {
        osal_printk("[ ble_server_register_callbacks ] Reg service cbk failed\r\n");
        return ERRCODE_BT_FAIL;
    }
    return ret;
}

/* 
 * 初始化BLE服务器
 * 包括注册回调、使能蓝牙、设置地址、创建服务、启动广播
 */
void ble_server_init(void)
{
    osal_printk("BLE SERVER ENTRY.\n");
    /* 注册GATT server用户回调函数 */
    ble_server_register_callbacks();
    osal_printk("BLE START ENABLE.\n");
    /* 打开蓝牙开关 */
    enable_ble();

    // 配置应用UUID
    bt_uuid_t app_uuid = {0};
    bd_addr_t ble_addr = {0};
    app_uuid.uuid_len = sizeof(g_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.uuid_len, g_uuid_app_uuid, sizeof(g_uuid_app_uuid)) != EOK) {
        return;
    }
    
    // 配置BLE设备地址
    ble_addr.type = BLE_PUBLIC_DEVICE_ADDRESS; // 公共设备地址
    if (memcpy_s(ble_addr.addr, BD_ADDR_LEN, g_ble_uart_server_addr, sizeof(g_ble_uart_server_addr)) != EOK) {
        osal_printk("add server app addr memcpy failed\n");
        return;
    }
    gap_ble_set_local_addr(&ble_addr); // 设置本地地址
    
    /* 1.创建一个server */
    gatts_register_server(&app_uuid, &g_server_id);
    /* 2.根据UUID创建service */
    ble_uuid_add_service();
    /* 3.设置广播参数并启动广播 */
    ble_start_adv();
}

/* 
 * BLE主任务函数
 * @param arg: 任务参数（未使用）
 */
void ble_main_task(const char *arg)
{
    arg = arg;
    // 初始化BLE服务器
    ble_server_init();
    // 初始化串口配置
    app_uart_init_config();
    
    // 主循环：处理消息队列中的数据
    while (1) {
        msg_data_t msg_data = {0};
        // 从消息队列读取数据
        int msg_ret = osal_msg_queue_read_copy(g_msg_queue, &msg_data, &g_msg_rev_size, OSAL_WAIT_FOREVER);
        if (msg_ret != OSAL_SUCCESS) {
            osal_printk("msg queue read copy fail.");
            if (msg_data.value != NULL) {
                osal_vfree(msg_data.value); // 释放内存
            }
            continue;
        }
        
        // 如果接收到串口数据，则通过BLE通知发送
        if (msg_data.value != NULL) {
            ble_server_send_report(msg_data.value, msg_data.value_len);
            osal_vfree(msg_data.value); // 发送完成后释放内存
        }
    }
}

/* 
 * BLE服务器入口函数
 * 创建消息队列和BLE主任务
 */
static void ble_server_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    
    // 创建消息队列
    int ret = osal_msg_queue_create("ble_msg", g_msg_rev_size, &g_msg_queue, 0, g_msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        osal_printk("create queue failure!,error:%x\n", ret);
    }

    // 创建BLE主任务
    task_handle = osal_kthread_create((osal_kthread_handler)ble_main_task, 0, "ble_main_task", BLE_SERVER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, BLE_SERVER_TASK_PRIO); // 设置任务优先级
        osal_kfree(task_handle); // 释放任务句柄内存
    }
    osal_kthread_unlock();
}

/* 应用运行宏：启动BLE服务器 */
app_run(ble_server_entry);

/* 
 * 串口接收回调函数
 * @param buffer: 接收到的数据缓冲区
 * @param length: 数据长度
 * @param error: 错误标志
 */
void ble_uart_server_read_handler(const void *buffer, uint16_t length, bool error)
{
    osal_printk("ble_uart_server_read_handler.\r\n");
    unused(error);
    
    // 当BLE连接时处理串口数据
    if (g_connection_state != 0) {
        msg_data_t msg_data = {0};
        // 分配内存并复制数据
        void *buffer_cpy = osal_vmalloc(length);
        if (memcpy_s(buffer_cpy, length, buffer, length) != EOK) {
            osal_vfree(buffer_cpy);
            return;
        }
        msg_data.value = (uint8_t *)buffer_cpy;
        msg_data.value_len = length;
        
        // 将串口接收到的数据写入消息队列
        osal_msg_queue_write_copy(g_msg_queue, (void *)&msg_data, g_msg_rev_size, 0);
    }
}

/* 
 * 串口初始化配置函数
 * 配置串口引脚、参数和接收回调
 */
void app_uart_init_config(void)
{
    uart_buffer_config_t uart_buffer_config;
    
    // 配置串口引脚模式
    uapi_pin_set_mode(CONFIG_UART0_TXD_PIN, CONFIG_UART0_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART0_RXD_PIN, CONFIG_UART0_PIN_MODE);
    
    // 配置串口属性
    uart_attr_t attr = {
        .baud_rate = 115200,        // 波特率：115200
        .data_bits = UART_DATA_BIT_8, // 数据位：8位
        .stop_bits = UART_STOP_BIT_1, // 停止位：1位
        .parity = UART_PARITY_NONE   // 校验位：无
    };
    
    // 配置串口缓冲区
    uart_buffer_config.rx_buffer_size = UART_RX_MAX;    // 接收缓冲区大小
    uart_buffer_config.rx_buffer = g_uart_rx_buffer;    // 接收缓冲区指针
    
    // 配置串口引脚
    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO0,     // 发送引脚
        .rx_pin = S_MGPIO1,     // 接收引脚
        .cts_pin = PIN_NONE,    // 清除发送引脚：无
        .rts_pin = PIN_NONE     // 请求发送引脚：无
    };
    
    // 先去初始化串口（重要步骤，否则可能初始化失败）
    uapi_uart_deinit(UART_BUS_0);
    
    // 重新初始化串口
    int res = uapi_uart_init(UART_BUS_0, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    
    // 注册串口接收回调函数
    if (uapi_uart_register_rx_callback(UART_BUS_0, UART_RX_CONDITION_MASK_IDLE, 1, ble_uart_server_read_handler) ==
        ERRCODE_SUCC) {
        osal_printk("uart%d int mode register receive callback succ!\r\n", UART_BUS_0);
    }
}