/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Host side SLE/SSAPS module.
 */

#include "sle_uart_host_module.h"

#include <string.h>

#include "mac_addr.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "soc_osal.h"

#include "sle_uart_host_adv.h"

#define osal_printk mine_host_log

static volatile bool g_mine_peer_connected = false;
static volatile uint16_t g_mine_conn_id = 0;
static uint8_t g_mine_server_id = 0;
static uint16_t g_mine_service_handle = 0;
static uint16_t g_mine_property_handle = 0;

static char g_mine_sle_app_uuid[MINE_UUID_APP_LEN] = {0x00, 0x00};
static char g_mine_property_init_value[MINE_PROPERTY_INIT_VALUE_LEN] = {0};

static uint8_t g_mine_sle_uuid_base[SLE_UUID_LEN] = {
    0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t g_mine_host_fallback_sle_mac[MINE_SLE_MAC_ADDR_LEN] = MINE_HOST_FALLBACK_SLE_MAC;

/**
 * @brief 查询 Host 侧链路连接状态。
 *
 * @return true  当前存在可用连接。
 * @return false 当前未连接。
 */
bool mine_sle_uart_host_is_connected(void)
{
    return g_mine_peer_connected;
}

/**
 * @brief 将 16 位 UUID 映射到 SLE 128 位基 UUID 结构。
 *
 * @param uuid16 16 位 UUID。
 * @param out    输出 UUID 结构。
 */
static void mine_set_uuid_u16(uint16_t uuid16, sle_uuid_t *out)
{
    if (out == NULL) {
        return;
    }

    if (memcpy_s(out->uuid, SLE_UUID_LEN, g_mine_sle_uuid_base, SLE_UUID_LEN) != EOK) {
        out->len = 0;
        return;
    }

    out->len = 2;
    out->uuid[MINE_UUID_BASE_INDEX_14] = (uint8_t)uuid16;
    out->uuid[MINE_UUID_BASE_INDEX_14 + 1] = (uint8_t)(uuid16 >> MINE_SHIFT_8_BITS);
}

/**
 * @brief 准备本机 SLE MAC 地址，优先系统地址，失败时回退到预设地址。
 */
static void mine_prepare_sle_mac(void)
{
    uint8_t sle_mac[SLE_ADDR_LEN] = {0};
    errcode_t ret;

    ret = get_dev_addr(sle_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[mine host] use system sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            sle_mac[0], sle_mac[1], sle_mac[2], sle_mac[3], sle_mac[4], sle_mac[5]);
        return;
    }

    ret = set_dev_addr(g_mine_host_fallback_sle_mac, MINE_SLE_MAC_ADDR_LEN, IFTYPE_SLE);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[mine host] set fallback sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            g_mine_host_fallback_sle_mac[0], g_mine_host_fallback_sle_mac[1], g_mine_host_fallback_sle_mac[2],
            g_mine_host_fallback_sle_mac[3], g_mine_host_fallback_sle_mac[4], g_mine_host_fallback_sle_mac[5]);
    } else {
        osal_printk("[mine host] set fallback sle mac failed, ret:%x\r\n", ret);
    }
}

/**
 * @brief 设置本机 SLE 地址与设备名。
 *
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 设置成功。
 * @retval 其他值             设置失败。
 */
static errcode_t mine_apply_local_addr_and_name(void)
{
    sle_addr_t local_addr = {0};
    uint8_t local_mac[SLE_ADDR_LEN] = {0};
    errcode_t ret;

    ret = get_dev_addr(local_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    if (ret != ERRCODE_SUCC) {
        if (memcpy_s(local_mac, sizeof(local_mac), g_mine_host_fallback_sle_mac,
            MINE_SLE_MAC_ADDR_LEN) != EOK) {
            return ERRCODE_SLE_FAIL;
        }
    }

    local_addr.type = 0;
    if (memcpy_s(local_addr.addr, SLE_ADDR_LEN, local_mac, SLE_ADDR_LEN) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    ret = sle_set_local_addr(&local_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] sle_set_local_addr failed:%x\r\n", ret);
        return ret;
    }

    ret = sle_set_local_name((const uint8_t *)MINE_SLE_UART_HOST_NAME,
        (uint8_t)strlen(MINE_SLE_UART_HOST_NAME));
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] sle_set_local_name failed:%x\r\n", ret);
        return ret;
    }

    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief 将 UART 消息分片通过 SSAPS notify 发送给对端。
 *
 * @param msg 待发送消息。
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 发送成功。
 * @retval 其他值             发送失败。
 */
errcode_t mine_sle_uart_host_send_by_handle(const mine_sle_uart_host_msg_t *msg)
{
    ssaps_ntf_ind_t notify_param = {0};
    uint16_t offset = 0;
    uint16_t chunk_len;
    uint16_t conn_id_snapshot;
    uint8_t uart_bus_snapshot;
    bool peer_connected_snapshot;
    errcode_t ret;

    if ((msg == NULL) || (msg->value == NULL) || (msg->value_len == 0)) {
        osal_printk("[mine host] invalid uart->sle msg\r\n");
        return ERRCODE_SLE_FAIL;
    }

    peer_connected_snapshot = g_mine_peer_connected;
    conn_id_snapshot = g_mine_conn_id;
    uart_bus_snapshot = msg->uart_bus;

    if (!peer_connected_snapshot) {
        osal_printk("[mine host] drop %s data, link:%u cid:%u\r\n",
            mine_host_uart_bus_name(uart_bus_snapshot),
            (unsigned int)peer_connected_snapshot, conn_id_snapshot);
        return ERRCODE_SLE_FAIL;
    }

    notify_param.handle = g_mine_property_handle;
    notify_param.type = SSAP_PROPERTY_TYPE_VALUE;

    while (offset < msg->value_len) {
        chunk_len = (uint16_t)((msg->value_len - offset) > MINE_SLE_SAFE_CHUNK_LEN ?
            MINE_SLE_SAFE_CHUNK_LEN : (msg->value_len - offset));

        notify_param.value = (uint8_t *)(msg->value + offset);
        notify_param.value_len = chunk_len;

        ret = ssaps_notify_indicate(g_mine_server_id, conn_id_snapshot, &notify_param);
        if (ret != ERRCODE_SLE_SUCCESS) {
            osal_printk("[mine host] notify failed, ret=%x\r\n", ret);
            return ret;
        }
        offset = (uint16_t)(offset + chunk_len);
    }

    osal_printk("[mine host] %s->sle notify len:%u\r\n",
        mine_host_uart_bus_name(uart_bus_snapshot), msg->value_len);

    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief SSAPS 读请求回调（当前无业务处理，仅占位）。
 */
static void mine_ssaps_read_request_cb(uint8_t server_id, uint16_t conn_id,
    ssaps_req_read_cb_t *read_cb_param, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(read_cb_param);
    unused(status);
}

/**
 * @brief SSAPS 写请求回调。
 *
 * 将对端下发数据转发到已启用 UART，并更新 OLED 事件信息。
 */
static void mine_ssaps_write_request_cb(uint8_t server_id, uint16_t conn_id,
    ssaps_req_write_cb_t *write_cb_param, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(status);

    if ((write_cb_param == NULL) || (write_cb_param->value == NULL) || (write_cb_param->length == 0)) {
        return;
    }

    osal_printk("[mine host] sle->uart len:%u\r\n", write_cb_param->length);
    mine_host_uart_write_enabled_buses(write_cb_param->value, write_cb_param->length);
    mine_host_oled_push_data_event(MINE_UART_BUS_INVALID, "SLE RX", write_cb_param->value, write_cb_param->length);
}

/**
 * @brief MTU 交换变更回调。
 */
static void mine_ssaps_mtu_changed_cb(uint8_t server_id, uint16_t conn_id,
    ssap_exchange_info_t *mtu_info, errcode_t status)
{
    unused(server_id);
    unused(conn_id);
    unused(status);

    if (mtu_info != NULL) {
        osal_printk("[mine host] mtu changed: %u\r\n", mtu_info->mtu_size);
    }
}

/**
 * @brief 服务启动完成回调。
 */
static void mine_ssaps_start_service_cb(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("[mine host] start service cb, server:%u handle:%u status:%x\r\n",
        server_id, handle, status);
}

/**
 * @brief 注册 SSAPS 相关回调。
 *
 * @return errcode_t 注册结果。
 */
static errcode_t mine_sle_uart_host_register_ssaps_callbacks(void)
{
    ssaps_callbacks_t ssaps_cb = {0};

    ssaps_cb.start_service_cb = mine_ssaps_start_service_cb;
    ssaps_cb.mtu_changed_cb = mine_ssaps_mtu_changed_cb;
    ssaps_cb.read_request_cb = mine_ssaps_read_request_cb;
    ssaps_cb.write_request_cb = mine_ssaps_write_request_cb;

    return ssaps_register_callbacks(&ssaps_cb);
}

/**
 * @brief 向 SSAPS Server 添加 UART 服务。
 *
 * @return errcode_t 添加结果。
 */
static errcode_t mine_sle_uart_host_add_service(void)
{
    sle_uuid_t service_uuid = {0};

    mine_set_uuid_u16(MINE_SLE_UART_SERVICE_UUID, &service_uuid);
    return ssaps_add_service_sync(g_mine_server_id, &service_uuid, true, &g_mine_service_handle);
}

/**
 * @brief 向 UART 服务添加可读写特征和描述符。
 *
 * @return errcode_t 添加结果。
 */
static errcode_t mine_sle_uart_host_add_property(void)
{
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t descriptor_value[] = {0x01, 0x00};
    errcode_t ret;

    property.permissions = (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE);
    property.operate_indication = (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY);
    mine_set_uuid_u16(MINE_SLE_UART_PROPERTY_UUID, &property.uuid);

    property.value = osal_vmalloc(sizeof(g_mine_property_init_value));
    if (property.value == NULL) {
        return ERRCODE_SLE_FAIL;
    }

    if (memcpy_s(property.value, sizeof(g_mine_property_init_value), g_mine_property_init_value,
        sizeof(g_mine_property_init_value)) != EOK) {
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_add_property_sync(g_mine_server_id, g_mine_service_handle, &property, &g_mine_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_vfree(property.value);
        return ret;
    }

    descriptor.permissions = (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE);
    descriptor.operate_indication = (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE);
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = descriptor_value;
    descriptor.value_len = sizeof(descriptor_value);

    ret = ssaps_add_descriptor_sync(g_mine_server_id, g_mine_service_handle, g_mine_property_handle, &descriptor);
    osal_vfree(property.value);

    return ret;
}

/**
 * @brief 注册 Server 并串行完成服务、特征、启动。
 *
 * @return errcode_t 初始化结果。
 */
static errcode_t mine_sle_uart_host_add_server(void)
{
    sle_uuid_t app_uuid = {0};
    errcode_t ret;

    app_uuid.len = sizeof(g_mine_sle_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_mine_sle_app_uuid, sizeof(g_mine_sle_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }

    ret = ssaps_register_server(&app_uuid, &g_mine_server_id);
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }

    ret = mine_sle_uart_host_add_service();
    if (ret != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_mine_server_id);
        return ret;
    }

    ret = mine_sle_uart_host_add_property();
    if (ret != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_mine_server_id);
        return ret;
    }

    ret = ssaps_start_service(g_mine_server_id, g_mine_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_mine_server_id);
        return ret;
    }

    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief 连接状态变化回调。
 *
 * 负责维护连接状态机，并在断开时重启广播。
 */
static void mine_sle_uart_host_connect_state_cb(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    unused(pair_state);
    unused(disc_reason);

    if (addr != NULL) {
        osal_printk("[mine host] remote:%02x:**:**:**:%02x:%02x state:%x\r\n",
            addr->addr[0], addr->addr[4], addr->addr[5], conn_state);
    }

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_mine_peer_connected = true;
        g_mine_conn_id = conn_id;
        osal_printk("[mine host] connected, conn_id:%u\r\n", conn_id);
        (void)sle_set_data_len(conn_id, MINE_SLE_DATA_LEN_AFTER_CONNECTED);
        mine_host_oled_push_text("CONNECTED");
        return;
    }

    if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_mine_peer_connected = false;
        g_mine_conn_id = 0;
        osal_printk("[mine host] disconnected, restart advertise\r\n");
        (void)sle_start_announce(MINE_SLE_ADV_HANDLE_DEFAULT);
        mine_host_oled_push_text("DISCONNECTED");
    }
}

/**
 * @brief 配对完成回调。
 */
static void mine_sle_uart_host_pair_complete_cb(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    unused(addr);
    osal_printk("[mine host] pair complete cid:%u status:%x\r\n", conn_id, status);
    if (status == ERRCODE_SLE_SUCCESS) {
        mine_host_oled_push_text("PAIR OK");
    } else {
        mine_host_oled_push_text("PAIR FAIL");
    }
}

/**
 * @brief 注册连接态与配对回调。
 *
 * @return errcode_t 注册结果。
 */
static errcode_t mine_sle_uart_host_register_conn_callbacks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};

    conn_cbks.connect_state_changed_cb = mine_sle_uart_host_connect_state_cb;
    conn_cbks.pair_complete_cb = mine_sle_uart_host_pair_complete_cb;

    return sle_connection_register_callbacks(&conn_cbks);
}

/**
 * @brief Host 侧 SLE/SSAPS 总初始化。
 *
 * 初始化内容包括：SLE 使能、本地地址名称、连接回调、
 * SSAPS 回调、服务特征构建、MTU 设置和广播启动。
 *
 * @return errcode_t 初始化结果。
 */
errcode_t mine_sle_uart_host_init(void)
{
    ssap_exchange_info_t exchange_info = {0};
    errcode_t ret;

    mine_prepare_sle_mac();

    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine host] enable sle failed:%x\r\n", ret);
        mine_host_oled_push_text("ENABLE SLE FAIL");
        return ret;
    }

    ret = mine_apply_local_addr_and_name();
    if (ret != ERRCODE_SLE_SUCCESS) {
        mine_host_oled_push_text("SET LOCAL FAIL");
        return ret;
    }

    ret = mine_sle_uart_host_register_conn_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] register conn callbacks failed:%x\r\n", ret);
        mine_host_oled_push_text("CONN CB FAIL");
        return ret;
    }

    ret = mine_sle_uart_host_register_ssaps_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] register ssaps callbacks failed:%x\r\n", ret);
        mine_host_oled_push_text("SSAPS CB FAIL");
        return ret;
    }

    ret = mine_sle_uart_host_add_server();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] add server failed:%x\r\n", ret);
        mine_host_oled_push_text("ADD SERVER FAIL");
        return ret;
    }

    exchange_info.mtu_size = MINE_SLE_DEFAULT_MTU_SIZE;
    exchange_info.version = 1;
    (void)ssaps_set_info(g_mine_server_id, &exchange_info);

    ret = mine_sle_uart_host_adv_init();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host] adv init failed:%x\r\n", ret);
        mine_host_oled_push_text("ADV START FAIL");
        return ret;
    }

    osal_printk("[mine host] init ok\r\n");
    mine_host_oled_push_text("SLE INIT OK");
    return ERRCODE_SLE_SUCCESS;
}
