#ifndef SLE_CLIENT_CALLBACKS_H
#define SLE_CLIENT_CALLBACKS_H

#include <stdint.h>
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "sle_device_discovery.h"

/* 星闪协议栈使能回调 */
void sle_client_sle_enable_cbk(errcode_t status);

/* 扫描使能回调函数 */
void sle_client_seek_enable_cbk(errcode_t status);

/* 返回扫描结果回调 */
void sle_client_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data);

/* 扫描关闭回调函数 */
void sle_client_seek_disable_cbk(errcode_t status);

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
                                          sle_disc_reason_t disc_reason);

/**
 * @brief 配对完成回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param status 操作状态
 */
void sle_sample_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);

/**
 * @brief 连接参数更新请求回调函数
 * @param conn_id 连接ID
 * @param status 操作状态
 * @param param 连接参数
 */
void sle_sample_update_req_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_req_t *param);

/**
 * @brief 连接参数更新回调函数
 * @param conn_id 连接ID
 * @param status 操作状态
 * @param param 连接参数
 */
void sle_sample_update_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_evt_t *param);

/**
 * @brief 读取RSSI回调函数
 * @param conn_id 连接ID
 * @param rssi RSSI值
 * @param status 操作状态
 */
void sle_sample_read_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status);

/**
 * @brief 更新MTU回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param param 交换信息参数
 * @param status 操作状态
 */
void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status);

/**
 * @brief 发现服务回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param service 服务发现结果
 * @param status 操作状态
 */
void sle_client_find_structure_cbk(uint8_t client_id,
                                   uint16_t conn_id,
                                   ssapc_find_service_result_t *service,
                                   errcode_t status);

/**
 * @brief 发现特征回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param property 特征发现结果
 * @param status 操作状态
 */
void sle_client_find_property_cbk(uint8_t client_id,
                                  uint16_t conn_id,
                                  ssapc_find_property_result_t *property,
                                  errcode_t status);

/**
 * @brief 发现服务完成回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param structure_result 发现结构结果
 * @param status 操作状态
 */
void sle_client_find_structure_cmp_cbk(uint8_t client_id,
                                       uint16_t conn_id,
                                       ssapc_find_structure_result_t *structure_result,
                                       errcode_t status);

/**
 * @brief 收到写响应回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param write_result 写操作结果
 * @param status 操作状态
 */
void sle_client_write_cfm_cbk(uint8_t client_id,
                              uint16_t conn_id,
                              ssapc_write_result_t *write_result,
                              errcode_t status);

/**
 * @brief 读取操作确认回调函数
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param read_data 读取的数据
 * @param status 操作状态
 */
void sle_client_read_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *read_data, errcode_t status);

    /**
     * @brief 收到通知回调
     * @param client_id 客户端ID
     * @param conn_id 连接ID
     * @param data 接收到的数据
     * @param status 操作状态
     */
    void sle_notification_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status);

/**
 * @brief 收到指示回调
 * @param client_id 客户端ID
 * @param conn_id 连接ID
 * @param data 接收到的数据
 * @param status 操作状态
 */
void sle_indication_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data, errcode_t status);

/* ============= UUID相关函数 ============= */

/**
 * 将16位数据编码为小端字节序
 * @param ptr 目标指针
 * @param data 要编码的16位数据
 */
void encode2byte_little(uint8_t *ptr, uint16_t data);

/**
 * 设置星闪UUID的基础部分
 * @param out 输出的UUID结构体
 */
void sle_uuid_set_base(sle_uuid_t *out);

/**
 * 设置16位UUID值（基于基础UUID）
 * @param u2 16位UUID值
 * @param out 输出的UUID结构体
 */
void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out);

/**
 * 打印UUID内容
 * @param uuid 要打印的UUID结构体
 */
void sle_uuid_print(sle_uuid_t *uuid);

/* ============= SSAPS (星闪服务器协议栈) 回调函数 ============= */

/**
 * MTU改变回调函数
 * @param server_id 服务器ID
 * @param conn_id 连接ID
 * @param mtu_size MTU大小信息
 * @param status 状态码
 */
void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size, errcode_t status);

/**
 * 服务启动回调函数
 * @param server_id 服务器ID
 * @param handle 服务句柄
 * @param status 状态码
 */
void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status);

/**
 * 服务添加回调函数
 * @param server_id 服务器ID
 * @param uuid 服务UUID
 * @param handle 服务句柄
 * @param status 状态码
 */
void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t handle, errcode_t status);

/**
 * 特征添加回调函数
 * @param server_id 服务器ID
 * @param uuid 特征UUID
 * @param service_handle 服务句柄
 * @param handle 特征句柄
 * @param status 状态码
 */
void ssaps_add_property_cbk(uint8_t server_id,
                            sle_uuid_t *uuid,
                            uint16_t service_handle,
                            uint16_t handle,
                            errcode_t status);

/**
 * 描述符添加回调函数
 * @param server_id 服务器ID
 * @param uuid 描述符UUID
 * @param service_handle 服务句柄
 * @param property_handle 特征句柄
 * @param status 状态码
 */
void ssaps_add_descriptor_cbk(uint8_t server_id,
                              sle_uuid_t *uuid,
                              uint16_t service_handle,
                              uint16_t property_handle,
                              errcode_t status);

/**
 * 删除所有服务回调函数
 * @param server_id 服务器ID
 * @param status 状态码
 */
void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status);

/**
 * 读请求回调函数
 * @param server_id 服务器ID
 * @param conn_id 连接ID
 * @param read_cb_para 读请求参数
 * @param status 状态码
 */
void ssaps_read_request_cbk(uint8_t server_id,
                            uint16_t conn_id,
                            ssaps_req_read_cb_t *read_cb_para,
                            errcode_t status);

/**
 * 写请求回调函数 - 处理客户端发送的数据
 * @param server_id 服务器ID
 * @param conn_id 连接ID
 * @param write_cb_para 写请求参数
 * @param status 状态码
 */
void ssaps_write_request_cbk(uint8_t server_id,
                             uint16_t conn_id,
                             ssaps_req_write_cb_t *write_cb_para,
                             errcode_t status);

#endif /* SLE_CLIENT_CALLBACKS_H */