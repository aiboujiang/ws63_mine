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
void sle_sample_connect_state_changed_cbk(uint16_t conn_id,
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

void sle_sample_auth_complete_cbk(uint16_t conn_id,
                                  const sle_addr_t *addr,
                                  errcode_t status,
                                  const sle_auth_info_evt_t *evt);
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
#endif /* SLE_CLIENT_CALLBACKS_H */