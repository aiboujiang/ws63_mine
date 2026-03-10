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
#include "osal_list.h"               // 操作系统抽象层链表
#include "bts_le_gap.h"              // BLE GAP层头文件
#include "bts_gatt_client.h"         // BLE GATT客户端头文件
#include "soc_osal.h"                // 操作系统抽象层
#include "app_init.h"                // 应用初始化
#include "securec.h"                 // 安全内存操作函数
#include "uart.h"                    // 串口通信
#include "common_def.h"              // 通用定义
#include "pinctrl.h"                 // 引脚控制
#include "platform_core.h"           // 平台核心功能
#include "ble_client.h"              // BLE客户端功能

/* 串口接收数据结构体 */
typedef struct {
    uint8_t *value;      // 数据指针
    uint16_t value_len;  // 数据长度
} msg_data_t;

/* 消息队列结构体 */
unsigned long g_msg_queue = 0;                   // 消息队列句柄
unsigned int g_msg_rev_size = sizeof(msg_data_t); // 消息大小

/* 串口接收缓冲区大小 */
#define UART_RX_MAX 512                          // 串口接收缓冲区大小512字节
uint8_t uart_rx_buffer[UART_RX_MAX];             // 串口接收缓冲区

/* 串口引脚配置 */
#define CONFIG_UART1_TXD_PIN 17                  // 串口1发送引脚
#define CONFIG_UART1_RXD_PIN 18                  // 串口1接收引脚
#define CONFIG_UART1_PIN_MODE 1                  // 引脚模式

/* 任务相关配置 */
#define BLE_UUID_CLIENT_TASK_PRIO 24             // BLE客户端任务优先级
#define BLE_UUID_CLIENT_STACK_SIZE 0x2000        // BLE客户端任务栈大小（8KB）

/* UUID长度定义 */
#define UUID16_LEN 2                             // 16位UUID长度（2字节）
#define DEFAULT_SCAN_INTERVAL 0x48               // 默认扫描间隔

/* 全局变量定义 */
uint8_t g_client_id = 0;                         // GATT客户端ID，0表示无效
uint8_t g_conn_id = 0;                           // BLE连接ID
uint8_t g_connection_state = 0;                  // BLE连接状态
static uint16_t g_ble_chara_hanle_write_value = 0; // 特征值写操作句柄
static uint16_t g_uart_mtu = 512;                // 最大传输单元，默认512字节
static bt_uuid_t g_client_app_uuid = {UUID16_LEN, {0}}; // 客户端应用UUID

/* 
 * 发现所有服务函数
 * @param conn_id: 连接ID
 * @return errcode_t: 操作结果
 */
errcode_t ble_gatt_client_discover_all_service(uint16_t conn_id)
{
    errcode_t ret = ERRCODE_BT_SUCCESS;
    osal_printk("[%s] \n", __FUNCTION__);
    bt_uuid_t service_uuid = {0}; /* UUID长度为0，表示发现所有服务 */
    ret |= gattc_discovery_service(g_client_id, conn_id, &service_uuid);
    return ret;
}

/* 
 * 发现服务回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param service: 服务发现结果
 * @param status: 操作状态
 */
static void ble_client_discover_service_cbk(uint8_t client_id,
                                            uint16_t conn_id,
                                            gattc_discovery_service_result_t *service,
                                            errcode_t status)
{
    gattc_discovery_character_param_t param = {0};
    osal_printk("[GATTClient]Discovery service----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            start handle:%d end handle:%d uuid_len:%d\n            uuid:", service->start_hdl,
                service->end_hdl, service->uuid.uuid_len);
    for (uint8_t i = 0; i < service->uuid.uuid_len; i++) {
        osal_printk("%02x", service->uuid.uuid[i]);
    }
    osal_printk("\n            status:%d\n", status);
    
    // 发现服务后，继续发现该服务的所有特征
    param.service_handle = service->start_hdl;
    param.uuid.uuid_len = 0; /* UUID长度为0，表示发现所有特征 */
    gattc_discovery_character(g_client_id, conn_id, &param);
}

/* 
 * 发现特征回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param character: 特征发现结果
 * @param status: 操作状态
 */
static void ble_client_discover_character_cbk(uint8_t client_id,
                                              uint16_t conn_id,
                                              gattc_discovery_character_result_t *character,
                                              errcode_t status)
{
    osal_printk("[GATTClient]Discovery character----client:%d conn_id:%d uuid_len:%d\n            uuid:", client_id,
                conn_id, character->uuid.uuid_len);
    for (uint8_t i = 0; i < character->uuid.uuid_len; i++) {
        osal_printk("%02x", character->uuid.uuid[i]);
    }
    osal_printk("\n            declare handle:%d value handle:%d properties:%x\n", character->declare_handle,
                character->value_handle, character->properties);
    osal_printk("            status:%d\n", status);
    
    // 保存特征值句柄，用于后续写操作
    g_ble_chara_hanle_write_value = character->value_handle;
    osal_printk(" g_client_id:%d value_handle:%d\n", g_client_id, character->value_handle);
    
    // 发现特征后，继续发现特征描述符
    gattc_discovery_descriptor(g_client_id, conn_id, character->declare_handle);
}

/* 
 * 发现特征描述符回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param descriptor: 描述符发现结果
 * @param status: 操作状态
 */
static void ble_client_discover_descriptor_cbk(uint8_t client_id,
                                               uint16_t conn_id,
                                               gattc_discovery_descriptor_result_t *descriptor,
                                               errcode_t status)
{
    osal_printk("[GATTClient]Discovery descriptor----client:%d conn_id:%d uuid len:%d\n            uuid:", client_id,
                conn_id, descriptor->uuid.uuid_len);
    for (uint8_t i = 0; i < descriptor->uuid.uuid_len; i++) {
        osal_printk("%02x", descriptor->uuid.uuid[i]);
    }
    osal_printk("\n            descriptor handle:%d\n", descriptor->descriptor_hdl);
    osal_printk("            status:%d\n", status);
}

/* 
 * 发现服务完成回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param uuid: 服务UUID
 * @param status: 操作状态
 */
static void ble_client_discover_service_compl_cbk(uint8_t client_id,
                                                  uint16_t conn_id,
                                                  bt_uuid_t *uuid,
                                                  errcode_t status)
{
    osal_printk("[GATTClient]Discovery service complete----client:%d conn_id:%d uuid len:%d\n            uuid:",
                client_id, conn_id, uuid->uuid_len);
    for (uint8_t i = 0; i < uuid->uuid_len; i++) {
        osal_printk("%02x", uuid->uuid[i]);
    }
    osal_printk("            status:%d\n", status);
}

/* 
 * 发现特征完成回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param param: 发现参数
 * @param status: 操作状态
 */
static void ble_client_discover_character_compl_cbk(uint8_t client_id,
                                                    uint16_t conn_id,
                                                    gattc_discovery_character_param_t *param,
                                                    errcode_t status)
{
    osal_printk("[GATTClient]Discovery character complete----client:%d conn_id:%d uuid len:%d\n            uuid:",
                client_id, conn_id, param->uuid.uuid_len);
    for (uint8_t i = 0; i < param->uuid.uuid_len; i++) {
        osal_printk("%02x", param->uuid.uuid[i]);
    }
    osal_printk("\n            service handle:%d\n", param->service_handle);
    osal_printk("            status:%d\n", status);
}

/* 
 * 发现特征描述符完成回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param character_handle: 特征句柄
 * @param status: 操作状态
 */
static void ble_client_discover_descriptor_compl_cbk(uint8_t client_id,
                                                     uint16_t conn_id,
                                                     uint16_t character_handle,
                                                     errcode_t status)
{
    osal_printk("[GATTClient]Discovery descriptor complete----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            charatcer handle:%d\n", character_handle);
    osal_printk("            status:%d\n", status);
}

/* 
 * 读取响应回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param read_result: 读取结果
 * @param status: 操作状态
 */
static void ble_client_read_cfm_cbk(uint8_t client_id,
                                    uint16_t conn_id,
                                    gattc_handle_value_t *read_result,
                                    gatt_status_t status)
{
    osal_printk("[GATTClient]Read result----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            handle:%d data_len:%d\ndata:", read_result->handle, read_result->data_len);
    for (uint16_t i = 0; i < read_result->data_len; i++) {
        osal_printk("%02x", read_result->data[i]);
    }
    osal_printk("\n            status:%d\n", status);
}

/* 
 * UUID读取完成回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param param: 读取参数
 * @param status: 操作状态
 */
static void ble_client_read_compl_cbk(uint8_t client_id,
                                      uint16_t conn_id,
                                      gattc_read_req_by_uuid_param_t *param,
                                      errcode_t status)
{
    osal_printk("[GATTClient]Read by uuid complete----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("start handle:%d end handle:%d uuid len:%d\n            uuid:", param->start_hdl, param->end_hdl,
                param->uuid.uuid_len);
    for (uint16_t i = 0; i < param->uuid.uuid_len; i++) {
        osal_printk("%02x", param->uuid.uuid[i]);
    }
    osal_printk("\n            status:%d\n", status);
}

/* 
 * 写特征值函数
 * @param data: 要写入的数据
 * @param len: 数据长度
 * @param handle: 特征句柄
 * @return errcode_t: 操作结果
 */
errcode_t ble_client_write_cmd(uint8_t *data, uint16_t len, uint16_t handle)
{
    gattc_handle_value_t uart_handle_value = {0};
    uart_handle_value.handle = handle;
    uart_handle_value.data_len = len;
    uart_handle_value.data = data;
    osal_printk(" ble_client_write_cmd len: %d, g_client_id: %d ", len, g_client_id);
    for (uint16_t i = 0; i < len; i++) {
        osal_printk("%c", data[i]);
    }
    osal_printk("\n");
    errcode_t ret = gattc_write_cmd(g_client_id, g_conn_id, &uart_handle_value);
    if (ret != ERRCODE_BT_SUCCESS) {
        osal_printk(" gattc_write_data failed\n");
        return ERRCODE_BT_FAIL;
    }
    return ERRCODE_BT_SUCCESS;
}

/* 
 * 写响应回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param handle: 特征句柄
 * @param status: 操作状态
 */
static void ble_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, uint16_t handle, gatt_status_t status)
{
    osal_printk("[GATTClient]Write result----client:%d conn_id:%d handle:%d\n", client_id, conn_id, handle);
    osal_printk("            status:%d\n", status);
}

/* 
 * MTU改变回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param mtu_size: MTU大小
 * @param status: 操作状态
 */
static void ble_client_mtu_changed_cbk(uint8_t client_id, uint16_t conn_id, uint16_t mtu_size, errcode_t status)
{
    osal_printk("[GATTClient]Mtu changed----client:%d conn_id:%d mtu size:%d\n", client_id, conn_id, mtu_size);
    osal_printk("            status:%d\n", status);
}

/* 
 * 通知回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param data: 通知数据
 * @param status: 操作状态
 */
static void ble_client_notification_cbk(uint8_t client_id,
                                        uint16_t conn_id,
                                        gattc_handle_value_t *data,
                                        errcode_t status)
{
    osal_printk("[GATTClient]Receive notification----client:%d conn_id:%d status:%d\n", client_id, conn_id, status);
    osal_printk("handle:%d data_len:%d data:", data->handle, data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        osal_printk("%c", data->data[i]);
    }
}

/* 
 * 指示回调函数
 * @param client_id: 客户端ID
 * @param conn_id: 连接ID
 * @param data: 指示数据
 * @param status: 操作状态
 */
static void ble_client_indication_cbk(uint8_t client_id, uint16_t conn_id, gattc_handle_value_t *data, errcode_t status)
{
    osal_printk("[GATTClient]Receive indication----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            handle:%d data_len:%d\ndata:", data->handle, data->data_len);
    for (uint16_t i = 0; i < data->data_len; i++) {
        osal_printk("%02x", data->data[i]);
    }
    osal_printk("\n            status:%d\n", status);
}

/* 
 * 蓝牙协议栈使能回调
 * @param status: 使能状态
 */
void ble_client_enable_cbk(errcode_t status)
{
    osal_printk("[ ble_client_enable_cbk ] Ble enable: %d\n", status);
}

/* 
 * 扫描结果回调函数
 * @param scan_result_data: 扫描结果数据
 */
static void ble_client_scan_result_cbk(gap_scan_result_data_t *scan_result_data)
{
    // 目标设备的MAC地址：12:34:56:78:90:00
    uint8_t ble_mac[BD_ADDR_LEN] = {0x00, 0x90, 0x78, 0x56, 0x34, 0x12};
    
    // 检查是否找到目标设备
    if (memcmp(scan_result_data->addr.addr, ble_mac, BD_ADDR_LEN) == 0) {
        osal_printk("[%s]Find The Target Device.\n", __FUNCTION__);
        gap_ble_stop_scan(); // 停止扫描
        
        bd_addr_t client_addr = {0};
        client_addr.type = scan_result_data->addr.type;
        if (memcpy_s(client_addr.addr, BD_ADDR_LEN, scan_result_data->addr.addr, BD_ADDR_LEN) != EOK) {
            osal_printk(" add server app addr memcpy failed\r\n");
            return;
        }
        // 连接到目标设备
        gap_ble_connect_remote_device(&client_addr);
    } else {
        // 打印其他设备的MAC地址
        osal_printk("\naddr:");
        for (uint8_t i = 0; i < BD_ADDR_LEN; i++) {
            osal_printk(" %02x:", scan_result_data->addr.addr[i]);
        }
    }
}

/* 
 * 连接状态改变回调函数
 * @param conn_id: 连接ID
 * @param addr: 设备地址
 * @param conn_state: 连接状态
 * @param pair_state: 配对状态
 * @param disc_reason: 断开原因
 */
static void ble_client_conn_state_change_cbk(uint16_t conn_id,
                                             bd_addr_t *addr,
                                             gap_ble_conn_state_t conn_state,
                                             gap_ble_pair_state_t pair_state,
                                             gap_ble_disc_reason_t disc_reason)
{
    osal_printk("[%s] Connect state change conn_id: %d, status: %d, pair_status:%d, disc_reason %x\n", __FUNCTION__,
                conn_id, conn_state, pair_state, disc_reason);
    g_conn_id = conn_id;
    g_connection_state = conn_state;
    
    if (conn_state == GAP_BLE_STATE_CONNECTED) {
        // 连接成功后开始配对
        gap_ble_pair_remote_device(addr);
        // 请求交换MTU
        gattc_exchange_mtu_req(g_client_id, g_conn_id, g_uart_mtu);
    }
}

/* 
 * 配对完成回调函数
 * @param conn_id: 连接ID
 * @param addr: 设备地址
 * @param status: 配对状态
 */
static void ble_client_pair_result_cbk(uint16_t conn_id, const bd_addr_t *addr, errcode_t status)
{
    unused(addr);
    osal_printk("[%s] Pair result conn_id: %d,status: %d \n", __FUNCTION__, conn_id, status);
    
    // 配对完成后再次请求交换MTU（确保MTU协商成功）
    gattc_exchange_mtu_req(g_client_id, g_conn_id, g_uart_mtu);
    // 开始发现所有服务
    ble_gatt_client_discover_all_service(conn_id);
}

/* 
 * 连接参数更新回调函数
 * @param conn_id: 连接ID
 * @param status: 操作状态
 * @param param: 连接参数
 */
static void ble_client_conn_param_update_cbk(uint16_t conn_id,
                                             errcode_t status,
                                             const gap_ble_conn_param_update_t *param)
{
    osal_printk("[%s] conn_param_update conn_id: %d,status: %d \n", __FUNCTION__, conn_id, status);
    osal_printk("interval:%d latency:%d timeout:%d.\n", param->interval, param->latency, param->timeout);
}

/* 
 * 注册GATT客户端回调函数
 * @return errcode_t: 操作结果
 */
errcode_t ble_gatt_client_callback_register(void)
{
    errcode_t ret = ERRCODE_BT_UNHANDLED;
    
    // 注册GAP回调函数
    gap_ble_callbacks_t gap_cb = {0};
    gap_cb.ble_enable_cb = ble_client_enable_cbk;
    gap_cb.scan_result_cb = ble_client_scan_result_cbk;
    gap_cb.conn_state_change_cb = ble_client_conn_state_change_cbk;
    gap_cb.pair_result_cb = ble_client_pair_result_cbk;
    gap_cb.conn_param_update_cb = ble_client_conn_param_update_cbk;
    ret |= gap_ble_register_callbacks(&gap_cb);
    if (ret != ERRCODE_BT_SUCCESS) {
        osal_printk("Reg gap cbk failed ret = %d\n", ret);
    }
    
    // 注册GATT回调函数
    gattc_callbacks_t cb = {0};
    cb.discovery_svc_cb = ble_client_discover_service_cbk;
    cb.discovery_svc_cmp_cb = ble_client_discover_service_compl_cbk;
    cb.discovery_chara_cb = ble_client_discover_character_cbk;
    cb.discovery_chara_cmp_cb = ble_client_discover_character_compl_cbk;
    cb.discovery_desc_cb = ble_client_discover_descriptor_cbk;
    cb.discovery_desc_cmp_cb = ble_client_discover_descriptor_compl_cbk;
    cb.read_cb = ble_client_read_cfm_cbk;
    cb.read_cmp_cb = ble_client_read_compl_cbk;
    cb.write_cb = ble_client_write_cfm_cbk;
    cb.mtu_changed_cb = ble_client_mtu_changed_cbk;
    cb.notification_cb = ble_client_notification_cbk;
    cb.indication_cb = ble_client_indication_cbk;
    ret |= gattc_register_callbacks(&cb);
    if (ret != ERRCODE_BT_SUCCESS) {
        osal_printk("Reg gatt cbk failed ret = %d\n", ret);
    }
    return ret;
}

/* 
 * 开始扫描函数
 * @return errcode_t: 操作结果
 */
errcode_t ble_cliant_start_scan(void)
{
    errcode_t ret = ERRCODE_BT_SUCCESS;
    gap_ble_scan_params_t ble_device_scan_params = {0};
    
    // 配置扫描参数
    ble_device_scan_params.scan_interval = DEFAULT_SCAN_INTERVAL;  // 扫描间隔
    ble_device_scan_params.scan_window = DEFAULT_SCAN_INTERVAL;    // 扫描窗口
    ble_device_scan_params.scan_type = 0x00;                       // 扫描类型：被动扫描
    ble_device_scan_params.scan_phy = GAP_BLE_PHY_2M;              // 物理层：2M PHY
    ble_device_scan_params.scan_filter_policy = 0x00;              // 过滤策略
    
    ret |= gap_ble_set_scan_parameters(&ble_device_scan_params);
    ret |= gap_ble_start_scan(); // 开始扫描
    return ret;
}

/* 
 * BLE客户端初始化函数
 * @return errcode_t: 操作结果
 */
errcode_t ble_client_init(void)
{
    errcode_t ret = ERRCODE_BT_SUCCESS;
    osal_printk("BLE CLIENT ENTRY.\n");
    
    // 注册回调函数
    ret |= ble_gatt_client_callback_register();
    
    // 使能BLE协议栈
    ret |= enable_ble();
    osal_printk("[ ble_client_init ] Enable_ble ret = %x\n", ret);
    
    // 注册GATT客户端
    ret |= gattc_register_client(&g_client_app_uuid, &g_client_id);
    osal_printk("[ ble_client_init ] Ble_gatt_client_init, ret:%x.\n", ret);
    
    // 开始扫描
    ret |= ble_cliant_start_scan();
    osal_printk("[ ble_client_init ] Ble_client_start_scan, ret:%x.\n", ret);
    
    return ret;
}

/* 
 * BLE主任务函数
 * 处理消息队列中的串口数据并通过BLE发送
 */
void ble_main_task(void)
{
    /* 初始化BLE协议栈及相关配置 */
    ble_client_init();
    
    /* 初始化串口0 */
    app_uart_init_config();
    
    /* 不断地从队列中读取数据 */
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
        
        /* 如果接收到串口数据，则通过写特征值的方式进行发送 */
        if (msg_data.value != NULL) {
            uint16_t write_handle = g_ble_chara_hanle_write_value;
            ble_client_write_cmd(msg_data.value, msg_data.value_len, write_handle);
            osal_vfree(msg_data.value); // 发送完成后释放内存
        }
    }
}

/* 
 * 任务创建函数
 * 创建消息队列和BLE主任务
 */
static void ble_client_entry(void)
{
    // 创建消息队列
    int ret = osal_msg_queue_create("ble_msg", g_msg_rev_size, &g_msg_queue, 0, g_msg_rev_size);
    if (ret != OSAL_SUCCESS) {
        osal_printk("create queue failure!,error:%x\n", ret);
    }
    
    // 创建BLE主任务
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle =
        osal_kthread_create((osal_kthread_handler)ble_main_task, 0, "ble_gatt_client", BLE_UUID_CLIENT_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, BLE_UUID_CLIENT_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 应用运行宏：启动BLE客户端 */
app_run(ble_client_entry);

/* 
 * 串口接收回调函数
 * @param buffer: 接收到的数据缓冲区
 * @param length: 数据长度
 * @param error: 错误标志
 */
void ble_uart_client_read_handler(const void *buffer, uint16_t length, bool error)
{
    osal_printk("ble_uart_client_read_handler.\r\n");
    unused(error);
    
    /* 当蓝牙连接时执行 */
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
        
        // 将串口接收到的数据写入队列
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
    
    /* 引脚模式复用为串口模式 */
    uapi_pin_set_mode(CONFIG_UART1_TXD_PIN, CONFIG_UART1_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART1_RXD_PIN, CONFIG_UART1_PIN_MODE);
    
    /* 配置串口属性 */
    uart_attr_t attr = {
        .baud_rate = 115200,        // 波特率：115200
        .data_bits = UART_DATA_BIT_8, // 数据位：8位
        .stop_bits = UART_STOP_BIT_1, // 停止位：1位
        .parity = UART_PARITY_NONE   // 校验位：无
    };
    
    /* 串口缓冲区配置 */
    uart_buffer_config.rx_buffer_size = UART_RX_MAX;    // 接收缓冲区大小
    uart_buffer_config.rx_buffer = uart_rx_buffer;      // 接收缓冲区指针
    
    /* 串口引脚配置 */
    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO0,     // 发送引脚
        .rx_pin = S_MGPIO1,     // 接收引脚
        .cts_pin = PIN_NONE,    // 清除发送引脚：无
        .rts_pin = PIN_NONE     // 请求发送引脚：无
    };
    
    /* 先去初始化串口 */
    uapi_uart_deinit(UART_BUS_0);
    
    /* 然后重新初始化串口 */
    int res = uapi_uart_init(UART_BUS_0, &pin_config, &attr, NULL, &uart_buffer_config);
    if (res != 0) {
        printf("uart init failed res = %02x\r\n", res);
    }
    
    /* 串口中断回调函数注册 */
    if (uapi_uart_register_rx_callback(UART_BUS_0, UART_RX_CONDITION_MASK_IDLE, 1, ble_uart_client_read_handler) ==
        ERRCODE_SUCC) {
        osal_printk("uart%d int mode register receive callback succ!\r\n", UART_BUS_0);
    }
}