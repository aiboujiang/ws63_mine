/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Slave side SLE/SSAPC module.
 */

#include "sle_uart_slave_module.h"

#include <string.h>

#include "common_def.h"
#include "mac_addr.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "soc_osal.h"

#define osal_printk mine_slave_log

static volatile uint16_t g_mine_conn_id = 0;
static volatile bool g_mine_peer_connected = false;
static volatile bool g_mine_property_ready = false;

static bool g_mine_seek_started = false;
static bool g_mine_seek_stop_pending = false;
static bool g_mine_connecting_pending = false;

static sle_addr_t g_mine_remote_addr = {0};
static ssapc_write_param_t g_mine_write_param = {0};

static const uint8_t g_mine_slave_fallback_sle_mac[MINE_SLE_MAC_ADDR_LEN] = MINE_SLAVE_FALLBACK_SLE_MAC;

static sle_announce_seek_callbacks_t g_mine_seek_cbks = {0};
static sle_connection_callbacks_t g_mine_conn_cbks = {0};
static ssapc_callbacks_t g_mine_ssapc_cbks = {0};

/**
 * @brief 判断广播负载中是否包含目标设备名。
 *
 * @param data     广播负载数据。
 * @param data_len 数据长度。
 * @param name     目标名称。
 * @return true  包含目标名称。
 * @return false 不包含或参数无效。
 */
static bool mine_adv_data_contains_name(const uint8_t *data, uint8_t data_len, const char *name)
{
    size_t name_len;
    uint8_t i;

    if ((data == NULL) || (name == NULL) || (data_len == 0)) {
        return false;
    }

    name_len = strlen(name);
    if ((name_len == 0) || (data_len < name_len)) {
        return false;
    }

    for (i = 0; i + name_len <= data_len; i++) {
        if (memcmp(&data[i], name, name_len) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 从 16 位 UUID 结构中提取主机字节序的 UUID 值。
 *
 * @param uuid UUID 结构。
 * @return uint16_t 16 位 UUID，失败返回 0。
 */
static uint16_t mine_get_uuid_u16(const sle_uuid_t *uuid)
{
    if ((uuid == NULL) || (uuid->len != 2)) {
        return 0;
    }

    return (uint16_t)(((uint16_t)uuid->uuid[MINE_UUID_BASE_INDEX_15] << 8) |
        uuid->uuid[MINE_UUID_BASE_INDEX_14]);
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
        osal_printk("[mine slave] use system sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            sle_mac[0], sle_mac[1], sle_mac[2], sle_mac[3], sle_mac[4], sle_mac[5]);
        return;
    }

    ret = set_dev_addr(g_mine_slave_fallback_sle_mac, MINE_SLE_MAC_ADDR_LEN, IFTYPE_SLE);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[mine slave] set fallback sle mac:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
            g_mine_slave_fallback_sle_mac[0], g_mine_slave_fallback_sle_mac[1], g_mine_slave_fallback_sle_mac[2],
            g_mine_slave_fallback_sle_mac[3], g_mine_slave_fallback_sle_mac[4], g_mine_slave_fallback_sle_mac[5]);
    } else {
        osal_printk("[mine slave] set fallback sle mac failed, ret:%x\r\n", ret);
    }
}

/**
 * @brief 设置 Slave 本地 SLE 地址与设备名。
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
        if (memcpy_s(local_mac, sizeof(local_mac), g_mine_slave_fallback_sle_mac,
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
        osal_printk("[mine slave] sle_set_local_addr failed:%x\r\n", ret);
        return ret;
    }

    ret = sle_set_local_name((const uint8_t *)MINE_SLE_UART_SLAVE_NAME,
        (uint8_t)strlen(MINE_SLE_UART_SLAVE_NAME));
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] sle_set_local_name failed:%x\r\n", ret);
        return ret;
    }

    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief 将 UART 消息分片通过 SSAPC write command 发给 Host。
 *
 * @param msg 待发送 UART 消息。
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 发送成功。
 * @retval 其他值             发送失败。
 */
errcode_t mine_sle_uart_slave_send_to_host(const mine_sle_uart_slave_msg_t *msg)
{
    uint16_t offset = 0;
    uint16_t chunk_len;
    uint16_t conn_id_snapshot;
    uint16_t property_handle_snapshot;
    uint8_t uart_bus_snapshot;
    bool peer_connected_snapshot;
    bool property_ready_snapshot;
    errcode_t ret;

    if ((msg == NULL) || (msg->value == NULL) || (msg->value_len == 0)) {
        osal_printk("[mine slave] invalid uart->sle msg\r\n");
        mine_slave_oled_push_state("SEND INVALID");
        return ERRCODE_SLE_FAIL;
    }

    peer_connected_snapshot = g_mine_peer_connected;
    property_ready_snapshot = g_mine_property_ready;
    conn_id_snapshot = g_mine_conn_id;
    property_handle_snapshot = g_mine_write_param.handle;
    uart_bus_snapshot = msg->uart_bus;

    if ((!peer_connected_snapshot) || (!property_ready_snapshot) || (property_handle_snapshot == 0)) {
        osal_printk("[mine slave] drop %s data, link:%u prop:%u cid:%u handle:%u\r\n",
            mine_slave_uart_bus_name(uart_bus_snapshot),
            (unsigned int)peer_connected_snapshot, (unsigned int)property_ready_snapshot,
            conn_id_snapshot, property_handle_snapshot);
        mine_slave_oled_push_state("SEND DROP");
        return ERRCODE_SLE_FAIL;
    }

    while (offset < msg->value_len) {
        chunk_len = (uint16_t)((msg->value_len - offset) > MINE_SLE_SAFE_CHUNK_LEN ?
            MINE_SLE_SAFE_CHUNK_LEN : (msg->value_len - offset));

        g_mine_write_param.data = (uint8_t *)(msg->value + offset);
        g_mine_write_param.data_len = chunk_len;

        ret = ssapc_write_cmd(0, conn_id_snapshot, &g_mine_write_param);
        if (ret != ERRCODE_SLE_SUCCESS) {
            osal_printk("[mine slave] write cmd failed, ret=%x\r\n", ret);
            mine_slave_oled_push_state("SEND FAIL");
            return ret;
        }

        offset = (uint16_t)(offset + chunk_len);
    }

    osal_printk("[mine slave] %s->sle write len:%u\r\n",
        mine_slave_uart_bus_name(uart_bus_snapshot), msg->value_len);
    mine_slave_oled_push_data_event(uart_bus_snapshot, "UART TX", msg->value, msg->value_len);

    return ERRCODE_SLE_SUCCESS;
}

/**
 * @brief 启动从机扫描流程。
 *
 * 仅在未连接、未连接中、未扫描中的状态下触发，
 * 并配置扫描参数后启动 seek。
 */
void mine_sle_uart_slave_start_scan(void)
{
    sle_seek_param_t seek_param = {0};
    errcode_t ret;

    if (g_mine_peer_connected || g_mine_connecting_pending) {
        return;
    }

    if (g_mine_seek_started || g_mine_seek_stop_pending) {
        return;
    }

    seek_param.own_addr_type = 0;
    seek_param.filter_duplicates = 0;
    seek_param.seek_filter_policy = 0;
    seek_param.seek_phys = 1;
    seek_param.seek_type[0] = 1;
    seek_param.seek_interval[0] = MINE_SLE_SEEK_INTERVAL_DEFAULT;
    seek_param.seek_window[0] = MINE_SLE_SEEK_WINDOW_DEFAULT;

    ret = sle_set_seek_param(&seek_param);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] set seek param failed:%x\r\n", ret);
        mine_slave_oled_push_state("SCAN PARAM FAIL");
        return;
    }

    ret = sle_start_seek();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] start seek failed:%x\r\n", ret);
        mine_slave_oled_push_state("SCAN START FAIL");
        return;
    }

    g_mine_seek_started = true;
    g_mine_seek_stop_pending = false;
    mine_slave_oled_push_state("SCANNING");
    osal_printk("[mine slave] start scan, interval:%u window:%u\r\n",
        MINE_SLE_SEEK_INTERVAL_DEFAULT, MINE_SLE_SEEK_WINDOW_DEFAULT);
}

/**
 * @brief SLE 使能结果回调。
 *
 * @param status 使能结果码。
 */
static void mine_sle_enable_cb(errcode_t status)
{
    if (status == ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] sle enabled\r\n");
        mine_slave_oled_push_state("SLE ON");
    } else {
        osal_printk("[mine slave] sle enable failed:%x\r\n", status);
        mine_slave_oled_push_state("SLE FAIL");
    }
}

/**
 * @brief 扫描启动结果回调。
 *
 * @param status 启动结果码。
 */
static void mine_seek_enable_cb(errcode_t status)
{
    if (status != ERRCODE_SLE_SUCCESS) {
        g_mine_seek_started = false;
        osal_printk("[mine slave] seek enable failed:%x\r\n", status);
        mine_slave_oled_push_state("SCAN FAIL");
    } else {
        g_mine_seek_started = true;
        osal_printk("[mine slave] seek enabled\r\n");
        mine_slave_oled_push_state("SCAN ON");
    }
}

/**
 * @brief 扫描结果回调。
 *
 * 命中目标广播名后停止扫描并准备发起连接。
 *
 * @param seek_result_data 扫描结果数据。
 */
static void mine_seek_result_cb(sle_seek_result_info_t *seek_result_data)
{
    errcode_t ret;

    if ((seek_result_data == NULL) || (seek_result_data->data == NULL) || (seek_result_data->data_length == 0)) {
        return;
    }

    if (g_mine_seek_stop_pending || g_mine_connecting_pending || g_mine_peer_connected) {
        return;
    }

    if (mine_adv_data_contains_name(seek_result_data->data, seek_result_data->data_length, MINE_SLE_UART_HOST_NAME)) {
        if (memcpy_s(&g_mine_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t)) != EOK) {
            return;
        }

        g_mine_seek_stop_pending = true;
        osal_printk("[mine slave] found target adv, stop seek\r\n");
        mine_slave_oled_push_state("TARGET FOUND");

        ret = sle_stop_seek();
        if (ret != ERRCODE_SLE_SUCCESS) {
            g_mine_seek_stop_pending = false;
            osal_printk("[mine slave] stop seek failed:%x\r\n", ret);
            mine_slave_oled_push_state("STOP FAIL");
        }
    }
}

/**
 * @brief 扫描停止回调。
 *
 * 扫描停止后若命中目标，会进入连接流程。
 *
 * @param status 停止结果码。
 */
static void mine_seek_disable_cb(errcode_t status)
{
    errcode_t ret;

    g_mine_seek_started = false;

    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] seek disable failed:%x\r\n", status);
        mine_slave_oled_push_state("STOP SCAN FAIL");
        g_mine_seek_stop_pending = false;
        return;
    }

    if (!g_mine_seek_stop_pending) {
        osal_printk("[mine slave] seek stopped (no target)\r\n");
        return;
    }

    g_mine_seek_stop_pending = false;
    g_mine_connecting_pending = true;

    osal_printk("[mine slave] seek stopped, try connect\r\n");
    mine_slave_oled_push_state("CONNECTING");
    ret = sle_remove_paired_remote_device(&g_mine_remote_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] remove pair failed:%x\r\n", ret);
    }

    ret = sle_connect_remote_device(&g_mine_remote_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        g_mine_connecting_pending = false;
        osal_printk("[mine slave] connect request failed:%x\r\n", ret);
        mine_slave_oled_push_state("CONN REQ FAIL");
        mine_sle_uart_slave_start_scan();
    }
}

/**
 * @brief 连接状态变化回调。
 *
 * 负责驱动配对、断链重扫和状态变量维护。
 */
static void mine_connect_state_changed_cb(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    unused(disc_reason);

    if (addr != NULL) {
        osal_printk("[mine slave] remote:%02x:**:**:**:%02x:%02x state:%x\r\n",
            addr->addr[0], addr->addr[4], addr->addr[5], conn_state);
    }

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_mine_seek_started = false;
        g_mine_seek_stop_pending = false;
        g_mine_connecting_pending = false;
        g_mine_conn_id = conn_id;
        g_mine_peer_connected = true;
        osal_printk("[mine slave] connected, conn_id:%u pair_state:%u\r\n", conn_id, pair_state);
        if (pair_state == SLE_PAIR_NONE) {
            osal_printk("[mine slave] start pair\r\n");
            mine_slave_oled_push_state("PAIRING");
            (void)sle_pair_remote_device(&g_mine_remote_addr);
        } else {
            mine_slave_oled_push_state("CONNECTED");
        }
        return;
    }

    if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_mine_seek_started = false;
        g_mine_seek_stop_pending = false;
        g_mine_connecting_pending = false;
        g_mine_conn_id = 0;
        g_mine_peer_connected = false;
        g_mine_property_ready = false;
        g_mine_write_param.handle = 0;
        osal_printk("[mine slave] disconnected, restart scan\r\n");
        mine_slave_oled_set_uuid_default();
        mine_slave_oled_push_state("DISCONN SCAN");
        mine_sle_uart_slave_start_scan();
    }
}

/**
 * @brief 配对完成回调。
 *
 * 配对成功后发起 MTU 交换请求。
 */
static void mine_pair_complete_cb(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    ssap_exchange_info_t exchange_info = {0};

    unused(addr);

    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] pair failed:%x\r\n", status);
        mine_slave_oled_push_state("PAIR FAIL");
        return;
    }

    osal_printk("[mine slave] pair ok, request exchange mtu\r\n");
    mine_slave_oled_push_state("PAIR OK");

    exchange_info.mtu_size = MINE_SLE_DEFAULT_MTU_SIZE;
    exchange_info.version = 1;
    (void)ssapc_exchange_info_req(0, conn_id, &exchange_info);
}

/**
 * @brief MTU 交换完成回调。
 *
 * 交换成功后启动特征发现流程。
 */
static void mine_exchange_info_cb(uint8_t client_id, uint16_t conn_id,
    ssap_exchange_info_t *param, errcode_t status)
{
    ssapc_find_structure_param_t find_param = {0};

    unused(client_id);

    if ((status != ERRCODE_SLE_SUCCESS) || (param == NULL)) {
        osal_printk("[mine slave] exchange info failed:%x\r\n", status);
        mine_slave_oled_push_state("MTU FAIL");
        return;
    }

    osal_printk("[mine slave] exchange ok, mtu:%u\r\n", param->mtu_size);
    mine_slave_oled_push_state("MTU OK");

    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    (void)ssapc_find_structure(0, conn_id, &find_param);
}

/**
 * @brief 服务发现过程回调。
 *
 * @param client_id 客户端 ID。
 * @param conn_id   连接 ID。
 * @param service   服务结果。
 * @param status    状态码。
 */
static void mine_find_structure_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_service_result_t *service, errcode_t status)
{
    unused(client_id);
    unused(conn_id);

    if ((status == ERRCODE_SLE_SUCCESS) && (service != NULL)) {
        osal_printk("[mine slave] find service start:%u end:%u\r\n", service->start_hdl, service->end_hdl);
    }
}

/**
 * @brief 特征发现回调。
 *
 * 命中目标 UUID 后记录可写句柄并置位可发送状态。
 */
static void mine_find_property_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    uint16_t uuid16;

    unused(client_id);
    unused(conn_id);

    if ((status != ERRCODE_SLE_SUCCESS) || (property == NULL)) {
        return;
    }

    uuid16 = mine_get_uuid_u16(&property->uuid);
    osal_printk("[mine slave] find property handle:%u uuid:0x%04x\r\n", property->handle, uuid16);
    if (uuid16 == MINE_SLE_UART_PROPERTY_UUID) {
        g_mine_write_param.handle = property->handle;
        g_mine_write_param.type = SSAP_PROPERTY_TYPE_VALUE;
        g_mine_property_ready = true;
        osal_printk("[mine slave] property ready, handle:%u\r\n", property->handle);
        mine_slave_oled_set_uuid_property(uuid16, property->handle);
        mine_slave_oled_push_state("PROP READY");
    }
}

/**
 * @brief 结构发现完成回调。
 */
static void mine_find_structure_cmp_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(structure_result);
    osal_printk("[mine slave] find structure complete, status:%x\r\n", status);
    if (status != ERRCODE_SLE_SUCCESS) {
        mine_slave_oled_push_state("DISC FAIL");
    }
}

/**
 * @brief 写命令确认回调。
 */
static void mine_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_write_result_t *write_result, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(write_result);
    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] write confirm failed:%x\r\n", status);
        mine_slave_oled_push_state("WRITE FAIL");
    }
}

/**
 * @brief Notify 数据接收回调。
 *
 * 将 Host 下发数据广播到已启用 UART，并更新 OLED。
 */
static void mine_notification_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);

    if ((data == NULL) || (data->data == NULL) || (data->data_len == 0)) {
        return;
    }

    osal_printk("[mine slave] sle notify rx len:%u\r\n", data->data_len);
    mine_slave_uart_write_enabled_buses(data->data, data->data_len);
    mine_slave_oled_push_data_event(MINE_UART_BUS_INVALID, "SLE RX", data->data, data->data_len);
}

/**
 * @brief Indication 数据接收回调。
 *
 * 将 Host 下发数据广播到已启用 UART，并更新 OLED。
 */
static void mine_indication_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    unused(status);

    if ((data == NULL) || (data->data == NULL) || (data->data_len == 0)) {
        return;
    }

    osal_printk("[mine slave] sle indication rx len:%u\r\n", data->data_len);
    mine_slave_uart_write_enabled_buses(data->data, data->data_len);
    mine_slave_oled_push_data_event(MINE_UART_BUS_INVALID, "SLE RX", data->data, data->data_len);
}

/**
 * @brief 注册扫描相关回调。
 *
 * @return errcode_t 注册结果。
 */
static errcode_t mine_register_seek_callbacks(void)
{
    g_mine_seek_cbks.sle_enable_cb = mine_sle_enable_cb;
    g_mine_seek_cbks.seek_enable_cb = mine_seek_enable_cb;
    g_mine_seek_cbks.seek_result_cb = mine_seek_result_cb;
    g_mine_seek_cbks.seek_disable_cb = mine_seek_disable_cb;

    return sle_announce_seek_register_callbacks(&g_mine_seek_cbks);
}

/**
 * @brief 注册连接态与配对回调。
 *
 * @return errcode_t 注册结果。
 */
static errcode_t mine_register_conn_callbacks(void)
{
    g_mine_conn_cbks.connect_state_changed_cb = mine_connect_state_changed_cb;
    g_mine_conn_cbks.pair_complete_cb = mine_pair_complete_cb;

    return sle_connection_register_callbacks(&g_mine_conn_cbks);
}

/**
 * @brief 注册 SSAPC 客户端回调。
 *
 * @return errcode_t 注册结果。
 */
static errcode_t mine_register_ssapc_callbacks(void)
{
    g_mine_ssapc_cbks.exchange_info_cb = mine_exchange_info_cb;
    g_mine_ssapc_cbks.find_structure_cb = mine_find_structure_cb;
    g_mine_ssapc_cbks.ssapc_find_property_cbk = mine_find_property_cb;
    g_mine_ssapc_cbks.find_structure_cmp_cb = mine_find_structure_cmp_cb;
    g_mine_ssapc_cbks.write_cfm_cb = mine_write_cfm_cb;
    g_mine_ssapc_cbks.notification_cb = mine_notification_cb;
    g_mine_ssapc_cbks.indication_cb = mine_indication_cb;

    return ssapc_register_callbacks(&g_mine_ssapc_cbks);
}

/**
 * @brief Slave 侧 SLE/SSAPC 总初始化。
 *
 * 初始化内容包括回调注册、SLE 使能、本地地址名称设置与扫描启动。
 *
 * @return errcode_t 初始化结果。
 */
errcode_t mine_sle_uart_slave_init(void)
{
    errcode_t ret;

    g_mine_seek_started = false;
    g_mine_seek_stop_pending = false;
    g_mine_connecting_pending = false;

    mine_prepare_sle_mac();

    ret = mine_register_seek_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] register seek callbacks failed:%x\r\n", ret);
        mine_slave_oled_push_state("SEEK CB FAIL");
        return ret;
    }

    ret = mine_register_conn_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] register conn callbacks failed:%x\r\n", ret);
        mine_slave_oled_push_state("CONN CB FAIL");
        return ret;
    }

    ret = mine_register_ssapc_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine slave] register ssapc callbacks failed:%x\r\n", ret);
        mine_slave_oled_push_state("SSAPC CB FAIL");
        return ret;
    }

    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine slave] enable sle failed:%x\r\n", ret);
        mine_slave_oled_push_state("ENABLE FAIL");
        return ret;
    }

    ret = mine_apply_local_addr_and_name();
    if (ret != ERRCODE_SLE_SUCCESS) {
        mine_slave_oled_push_state("SET LOCAL FAIL");
        return ret;
    }

    mine_sle_uart_slave_start_scan();

    osal_printk("[mine slave] init ok\r\n");
    mine_slave_oled_push_state("SLE INIT OK");
    return ERRCODE_SLE_SUCCESS;
}
