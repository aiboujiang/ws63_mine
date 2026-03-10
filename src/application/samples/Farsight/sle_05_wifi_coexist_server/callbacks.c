#include "common_def.h"
#include "app_init.h"
#include "soc_osal.h"
#include "sle_device_discovery.h"
#include "securec.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "server.h"
#include "callbacks.h"
#include "sle_ssap_server.h"

/* 星闪协议栈使能回调 */
void sle_client_sle_enable_cbk(errcode_t status)
{
    unused(status);
    printf("SLE enable.\r\n");
}

/* ============= 广播回调函数 ============= */

/**
 * 广播使能回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[%s]  id:%02x, state:%x\r\n", __FUNCTION__, announce_id, status);
}

/**
 * 广播禁用回调函数
 * @param announce_id 广播ID
 * @param status 状态码
 */
void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[%s] id:%02x, state:%x\r\n", __FUNCTION__, announce_id, status);
}

/**
 * 广播终止回调函数
 * @param announce_id 广播ID
 */
void sle_announce_terminal_cbk(uint32_t announce_id)
{
    printf("[%s] id:%02x\r\n", __FUNCTION__, announce_id);
}


/* ============= 连接管理回调函数 ============= */

/**
 * 连接状态改变回调函数
 */
void sle_connect_state_changed_cbk(uint16_t conn_id,
                                          const sle_addr_t *addr,
                                          sle_acb_state_t conn_state,
                                          sle_pair_state_t pair_state,
                                          sle_disc_reason_t disc_reason)
{
    printf("[%s] conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, disc_reason:0x%x\r\n", __FUNCTION__, conn_id,
           conn_state, pair_state, disc_reason);
    printf("[%s] addr:%02x:**:**:**:%02x:%02x\r\n", __FUNCTION__, addr->addr[0], addr->addr[4],addr->addr[5]); 
    
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_sle_conn_hdl = conn_id;
        conn_comp_flag=1;
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_hdl = 0;
        conn_comp_flag=0;
        sle_start_announce(SLE_ADV_HANDLE_DEFAULT); // 断开后重新开始广播
    }
}

/**
 * 配对完成回调函数
 */
void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    printf("[%s] conn_id:%02x, status:%x\r\n", __FUNCTION__, conn_id, status);
    printf("[%s] addr:%02x:**:**:**:%02x:%02x\r\n", __FUNCTION__, addr->addr[0], addr->addr[4],addr->addr[5]); /* 0 4: index */
}


/**
 * 将16位数据编码为小端字节序
 * @param ptr 目标指针
 * @param data 要编码的16位数据
 */
void encode2byte_little(uint8_t *ptr, uint16_t data)
{
    *(uint8_t *)((ptr) + 1) = (uint8_t)((data) >> 0x8); // 高字节在后
    *(uint8_t *)(ptr) = (uint8_t)(data);                // 低字节在前
}

/**
 * 设置星闪UUID的基础部分
 * @param out 输出的UUID结构体
 */
void sle_uuid_set_base(sle_uuid_t *out)
{
    errcode_t ret;
    ret = memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_base, SLE_UUID_LEN);
    if (ret != EOK) {
        printf("[%s] memcpy fail\n", __FUNCTION__);
        out->len = 0;
        return;
    }
    out->len = UUID_LEN_2;
}

/**
 * 设置16位UUID值（基于基础UUID）
 * @param u2 16位UUID值
 * @param out 输出的UUID结构体
 */
void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);              // 设置基础部分
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2); /* 14: 在基础UUID的最后2字节位置设置自定义UUID值 */
}

/**
 * 打印UUID内容
 * @param uuid 要打印的UUID结构体
 */
void sle_uuid_print(sle_uuid_t *uuid)
{
    printf("[%s] ", __FUNCTION__);
    if (uuid == NULL) {
        printf("uuid is null\r\n");
        return;
    }
    if (uuid->len == UUID_16BIT_LEN) {
        printf("uuid: %02x %02x.\n", uuid->uuid[14], uuid->uuid[15]); /* 14 15: 下标，显示最后2字节 */
    } else if (uuid->len == UUID_128BIT_LEN) {
        for (size_t i = 0; i < UUID_128BIT_LEN; i++) {
            printf("0x%02x ", uuid->uuid[i]); // 打印完整的128位UUID
        }
    }
}
/* ============= SSAPS (星闪服务器协议栈) 回调函数 ============= */

/**
 * MTU改变回调函数
 */
void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id, ssap_exchange_info_t *mtu_size, errcode_t status)
{
    printf("[%s] server_id:%d, conn_id:%d, mtu_size:%d, status:%d\r\n", __FUNCTION__, server_id, conn_id,
           mtu_size->mtu_size, status);
}

/**
 * 服务启动回调函数
 */
void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    printf("[%s] server_id:%d, handle:%x, status:%x\r\n", __FUNCTION__, server_id, handle, status);
}
/**
 * 服务添加回调函数
 */
void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    printf("[%s] server_id:%x, handle:%x, status:%x\r\n", __FUNCTION__, server_id, handle, status);
    sle_uuid_print(uuid);
}

/**
 * 特征添加回调函数
 */
void ssaps_add_property_cbk(uint8_t server_id,
                                   sle_uuid_t *uuid,
                                   uint16_t service_handle,
                                   uint16_t handle,
                                   errcode_t status)
{
    printf("[%s] server_id:%x, service_handle:%x,handle:%x, status:%x\r\n", __FUNCTION__, server_id, service_handle,
           handle, status);
    sle_uuid_print(uuid);
}

/**
 * 描述符添加回调函数
 */
void ssaps_add_descriptor_cbk(uint8_t server_id,
                                     sle_uuid_t *uuid,
                                     uint16_t service_handle,
                                     uint16_t property_handle,
                                     errcode_t status)
{
    printf("[%s] server_id:%x, service_handle:%x, property_handle:%x,status:%x\r\n", __FUNCTION__, server_id,
           service_handle, property_handle, status);
    sle_uuid_print(uuid);
}

/**
 * 删除所有服务回调函数
 */
void ssaps_delete_all_service_cbk(uint8_t server_id, errcode_t status)
{
    printf("[%s] server_id:%x, status:%x\r\n", __FUNCTION__, server_id, status);
}

/**
 * 读请求回调函数
 */
void ssaps_read_request_cbk(uint8_t server_id,
                                   uint16_t conn_id,
                                   ssaps_req_read_cb_t *read_cb_para,
                                   errcode_t status)
{
    printf("[%s] server_id:%x, conn_id:%x, handle:%x, status:%x\r\n", __FUNCTION__, server_id, conn_id,
           read_cb_para->handle, status);
}

/**
 * 写请求回调函数 - 处理客户端发送的数据
 */
void ssaps_write_request_cbk(uint8_t server_id,
                                    uint16_t conn_id,
                                    ssaps_req_write_cb_t *write_cb_para,
                                    errcode_t status)
{
    unused(status);
    printf("[%s] server_id:%x, conn_id:%x, handle:%x\r\n", __FUNCTION__, server_id, conn_id, write_cb_para->handle);
    if ((write_cb_para->length > 0) && write_cb_para->value) {
        printf("recv len:%d data: ", write_cb_para->length);
        for (uint16_t i = 0; i < write_cb_para->length; i++) {
            printf("%c", write_cb_para->value[i]); // 打印接收到的数据内容
        }
        printf("\r\n");
    }
}