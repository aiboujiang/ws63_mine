#ifndef SLE_SERVER_CALLBACKS_H
#define SLE_SERVER_CALLBACKS_H

#include "common_def.h"
#include "soc_osal.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include <stdint.h>
#include "sle_ssap_server.h"

/* ============= 全局变量声明 ============= */
extern uint16_t g_sle_conn_hdl;

/* ============= 广播回调函数 ============= */

/**
 * 星闪协议栈使能回调
 * @param status 状态码
 */
void sle_client_sle_enable_cbk(errcode_t status);

/**
 * 广播使能回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status);

/**
 * 广播禁用回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status);

/**
 * 广播终止回调函数
 * @param announce_id 广播ID
 */
void sle_announce_terminal_cbk(uint32_t announce_id);


/* ============= 连接管理回调函数 ============= */

/**
 * 连接状态改变回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param conn_state 连接状态
 * @param pair_state 配对状态
 * @param disc_reason 断开原因
 */
void sle_connect_state_changed_cbk(uint16_t conn_id,
                                   const sle_addr_t *addr,
                                   sle_acb_state_t conn_state,
                                   sle_pair_state_t pair_state,
                                   sle_disc_reason_t disc_reason);

/**
 * 配对完成回调函数
 * @param conn_id 连接ID
 * @param addr 设备地址
 * @param status 状态码
 */
void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status);


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

#ifdef __cplusplus
}
#endif

#endif /* SLE_SERVER_CALLBACKS_H */