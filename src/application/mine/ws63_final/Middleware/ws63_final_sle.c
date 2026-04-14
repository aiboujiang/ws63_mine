/**
 * @file ws63_final_sle.c
 * @brief WS63 final 的 SLE 从机桥接中间件实现。
 */

#include "ws63_final_sle.h"

#include <string.h>

#include "mac_addr.h"
#include "osal_debug.h"
#include "securec.h"
#include "sle_common.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "sle_ssap_client.h"
#include "soc_osal.h"

#include "ws63_final_config.h"
#include "ws63_final_osal.h"

#define WS63_SLE_TAG_LD2402 "[LD2402]"
#define WS63_SLE_TAG_ZW101  "[ZW101]"
#define WS63_SLE_TAG_CAMERA "[CAMERA]"
#define WS63_SLE_TAG_DEBUG  "[DEBUG]"

#if (WS63_SLE_LOG_ENABLE == 1U)
#define WS63_SLE_LOG(fmt, ...) osal_printk("[ws63 sle] " fmt "\r\n", ##__VA_ARGS__)
#else
#define WS63_SLE_LOG(...) do { } while (0)
#endif

/* UUID 在 SDK 结构体中的 16bit 高低字节索引。 */
#define WS63_UUID_BASE_INDEX_14 14
#define WS63_UUID_BASE_INDEX_15 15

#if (WS63_SLE_CORE_ENABLE == 1U)

/* 连接状态快照：用于发送路径快速判断。 */
static volatile uint16_t g_ws63_sle_conn_id = 0;
static volatile bool g_ws63_sle_peer_connected = false;
static volatile bool g_ws63_sle_property_ready = false;

/* 扫描/连接状态机标志。 */
static bool g_ws63_sle_seek_started = false;
static bool g_ws63_sle_seek_stop_pending = false;
static bool g_ws63_sle_connecting_pending = false;

static sle_addr_t g_ws63_sle_remote_addr = {0};
static ssapc_write_param_t g_ws63_sle_write_param = {0};
static ws63_sle_downlink_cb_t g_ws63_sle_downlink_cb = NULL;

/* 系统地址不可读时使用兜底地址，保证调试场景可连通。 */
static const uint8_t g_ws63_sle_fallback_mac[SLE_ADDR_LEN] = WS63_SLE_FALLBACK_MAC;

static sle_announce_seek_callbacks_t g_ws63_sle_seek_cbks = {0};
static sle_connection_callbacks_t g_ws63_sle_conn_cbks = {0};
static ssapc_callbacks_t g_ws63_sle_ssapc_cbks = {0};

/* 上行 success 日志控制：默认关闭逐包 success，仅在需要时打开。 */
static uint8_t g_ws63_sle_uplink_success_log_enable = WS63_SLE_UPLINK_SUCCESS_LOG_ENABLE_DEFAULT;
static uint32_t g_ws63_sle_uplink_success_log_gap_ms = WS63_SLE_UPLINK_SUCCESS_LOG_GAP_MS_DEFAULT;
static uint32_t g_ws63_sle_uplink_success_last_log_ms = 0U;
static uint32_t g_ws63_sle_uplink_success_skip_count = 0U;

/**
 * @brief 统一执行“带标签”的 SLE 上行发送。
 */
static errcode_t ws63_sle_send_tagged_data(const char *tag, const uint8_t *data, uint16_t len);

/**
 * @brief 按时间窗口输出上行 success 日志，避免高频数据导致串口刷屏。
 */
static void ws63_sle_log_uplink_success_limited(uint8_t sub_port, uint16_t len)
{
    uint32_t now_ms;

    if (g_ws63_sle_uplink_success_log_enable == 0U) {
        return;
    }

    now_ms = ws63_os_tick_ms();
    if ((g_ws63_sle_uplink_success_log_gap_ms > 0U) &&
        ((uint32_t)(now_ms - g_ws63_sle_uplink_success_last_log_ms) < g_ws63_sle_uplink_success_log_gap_ms)) {
        g_ws63_sle_uplink_success_skip_count++;
        return;
    }

    g_ws63_sle_uplink_success_last_log_ms = now_ms;
    WS63_SLE_LOG("uplink send success, sub_port=%u, len=%u, suppressed=%u",
        (unsigned int)sub_port,
        (unsigned int)len,
        (unsigned int)g_ws63_sle_uplink_success_skip_count);
    g_ws63_sle_uplink_success_skip_count = 0U;
}

/**
 * @brief 按子口映射对应的上行模块标签。
 *
 * 只有显式启用的模块才允许上行，未启用模块返回 NULL。
 */
static const char *ws63_sle_get_tag_by_subport(uint8_t sub_port)
{
    (void)sub_port;

#if (WS63_SLE_LD2402_ENABLE == 1U)
    if (sub_port == WS63_SLE_LD2402_SUBPORT) {
        return WS63_SLE_TAG_LD2402;
    }
#endif

#if (WS63_SLE_ZW101_ENABLE == 1U)
    if (sub_port == WS63_SLE_ZW101_SUBPORT) {
        return WS63_SLE_TAG_ZW101;
    }
#endif

#if (WS63_SLE_CAMERA_ENABLE == 1U)
    if (sub_port == WS63_SLE_CAMERA_SUBPORT) {
        return WS63_SLE_TAG_CAMERA;
    }
#endif

    return NULL;
}

/**
 * @brief 广播数据中匹配目标设备名。
 */
static bool ws63_sle_adv_contains_name(const uint8_t *data, uint8_t data_len, const char *name)
{
    size_t name_len;
    uint8_t i;

    if ((data == NULL) || (name == NULL) || (data_len == 0U)) {
        return false;
    }

    name_len = strlen(name);
    if ((name_len == 0U) || (data_len < name_len)) {
        return false;
    }

    for (i = 0U; i + name_len <= data_len; i++) {
        if (memcmp(&data[i], name, name_len) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 从 UUID 结构体提取 16bit UUID。
 */
static uint16_t ws63_sle_get_uuid_u16(const sle_uuid_t *uuid)
{
    if ((uuid == NULL) || (uuid->len != 2U)) {
        return 0U;
    }

    return (uint16_t)(((uint16_t)uuid->uuid[WS63_UUID_BASE_INDEX_15] << 8) |
        uuid->uuid[WS63_UUID_BASE_INDEX_14]);
}

/**
 * @brief 准备本地 SLE MAC，系统地址失败时回退到配置地址。
 */
static void ws63_sle_prepare_mac(void)
{
    uint8_t sle_mac[SLE_ADDR_LEN] = {0};

    if (get_dev_addr(sle_mac, SLE_ADDR_LEN, IFTYPE_SLE) == ERRCODE_SUCC) {
        WS63_SLE_LOG("use system mac");
        return;
    }

    (void)set_dev_addr(g_ws63_sle_fallback_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    WS63_SLE_LOG("use fallback mac");
}

/**
 * @brief 设置本地地址与设备名。
 */
static errcode_t ws63_sle_apply_local_info(void)
{
    sle_addr_t local_addr = {0};
    uint8_t local_mac[SLE_ADDR_LEN] = {0};
    errcode_t ret;

    ret = get_dev_addr(local_mac, SLE_ADDR_LEN, IFTYPE_SLE);
    if (ret != ERRCODE_SUCC) {
        if (memcpy_s(local_mac, sizeof(local_mac), g_ws63_sle_fallback_mac, SLE_ADDR_LEN) != EOK) {
            return ERRCODE_FAIL;
        }
    }

    local_addr.type = 0;
    if (memcpy_s(local_addr.addr, SLE_ADDR_LEN, local_mac, SLE_ADDR_LEN) != EOK) {
        return ERRCODE_FAIL;
    }

    ret = sle_set_local_addr(&local_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] set local addr failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = sle_set_local_name((const uint8_t *)WS63_SLE_LOCAL_NAME,
        (uint8_t)strlen(WS63_SLE_LOCAL_NAME));
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] set local name failed, ret=0x%x\r\n", (unsigned int)ret);
    }

    return ret;
}

/**
 * @brief 启动扫描流程（内部带状态守卫，防止重复触发）。
 */
static void ws63_sle_start_scan(void)
{
    sle_seek_param_t seek_param = {0};
    errcode_t ret;

    if (g_ws63_sle_peer_connected || g_ws63_sle_connecting_pending) {
        return;
    }

    if (g_ws63_sle_seek_started || g_ws63_sle_seek_stop_pending) {
        return;
    }

    seek_param.own_addr_type = 0;
    seek_param.filter_duplicates = 0;
    seek_param.seek_filter_policy = 0;
    seek_param.seek_phys = 1;
    seek_param.seek_type[0] = 1;
    seek_param.seek_interval[0] = WS63_SLE_SEEK_INTERVAL;
    seek_param.seek_window[0] = WS63_SLE_SEEK_WINDOW;

    ret = sle_set_seek_param(&seek_param);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] set seek param failed, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    ret = sle_start_seek();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] start seek failed, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_sle_seek_started = true;
    g_ws63_sle_seek_stop_pending = false;
    /* 仅在扫描真正启动成功后打印一次，避免周期流程刷屏。 */
    WS63_SLE_LOG("scan start success(interval=%u, window=%u)",
        (unsigned int)WS63_SLE_SEEK_INTERVAL, (unsigned int)WS63_SLE_SEEK_WINDOW);
}

/**
 * @brief 构造“模块标签 + 原始数据”的上行缓冲区。
 */
static errcode_t ws63_sle_build_payload(const char *tag, const uint8_t *src, uint16_t src_len,
    uint8_t **out_payload, uint16_t *out_len)
{
    uint16_t tag_len;
    uint32_t total_len;
    uint8_t *payload;

    if ((tag == NULL) || (src == NULL) || (src_len == 0U) || (out_payload == NULL) || (out_len == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    tag_len = (uint16_t)strlen(tag);
    if (tag_len == 0U) {
        return ERRCODE_INVALID_PARAM;
    }

    total_len = (uint32_t)tag_len + (uint32_t)src_len;
    if (total_len > 0xFFFFU) {
        return ERRCODE_INVALID_PARAM;
    }

    payload = osal_vmalloc((uint16_t)total_len);
    if (payload == NULL) {
        return ERRCODE_FAIL;
    }

    if (memcpy_s(payload, (uint16_t)total_len, tag, tag_len) != EOK) {
        osal_vfree(payload);
        return ERRCODE_FAIL;
    }

    if (memcpy_s(payload + tag_len, (uint16_t)total_len - tag_len, src, src_len) != EOK) {
        osal_vfree(payload);
        return ERRCODE_FAIL;
    }

    *out_payload = payload;
    *out_len = (uint16_t)total_len;
    return ERRCODE_SUCC;
}

/**
 * @brief SLE 使能回调。
 */
static void ws63_sle_enable_cb(errcode_t status)
{
    if (status == ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] enabled\r\n");
    } else {
        osal_printk("[ws63 sle] enable failed, ret=0x%x\r\n", (unsigned int)status);
    }
}

/**
 * @brief 扫描启动结果回调。
 */
static void ws63_sle_seek_enable_cb(errcode_t status)
{
    if (status != ERRCODE_SLE_SUCCESS) {
        g_ws63_sle_seek_started = false;
        osal_printk("[ws63 sle] seek enable failed, ret=0x%x\r\n", (unsigned int)status);
    }
}

/**
 * @brief 扫描结果回调：命中目标名后停止扫描并准备连接。
 */
static void ws63_sle_seek_result_cb(sle_seek_result_info_t *seek_result_data)
{
    errcode_t ret;

    if ((seek_result_data == NULL) || (seek_result_data->data == NULL) || (seek_result_data->data_length == 0U)) {
        return;
    }

    if (g_ws63_sle_seek_stop_pending || g_ws63_sle_connecting_pending || g_ws63_sle_peer_connected) {
        return;
    }

    if (!ws63_sle_adv_contains_name(seek_result_data->data, seek_result_data->data_length, WS63_SLE_HOST_NAME)) {
        return;
    }

    if (memcpy_s(&g_ws63_sle_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t)) != EOK) {
        return;
    }

    WS63_SLE_LOG("target advertisement matched");
    g_ws63_sle_seek_stop_pending = true;
    ret = sle_stop_seek();
    if (ret != ERRCODE_SLE_SUCCESS) {
        g_ws63_sle_seek_stop_pending = false;
        osal_printk("[ws63 sle] stop seek failed, ret=0x%x\r\n", (unsigned int)ret);
    }
}

/**
 * @brief 扫描停止回调：命中目标后发起连接。
 */
static void ws63_sle_seek_disable_cb(errcode_t status)
{
    errcode_t ret;

    g_ws63_sle_seek_started = false;
    if (status != ERRCODE_SLE_SUCCESS) {
        g_ws63_sle_seek_stop_pending = false;
        osal_printk("[ws63 sle] seek disable failed, ret=0x%x\r\n", (unsigned int)status);
        return;
    }

    if (!g_ws63_sle_seek_stop_pending) {
        return;
    }

    g_ws63_sle_seek_stop_pending = false;
    g_ws63_sle_connecting_pending = true;

    (void)sle_remove_paired_remote_device(&g_ws63_sle_remote_addr);
    WS63_SLE_LOG("scan stopped, connect start");
    ret = sle_connect_remote_device(&g_ws63_sle_remote_addr);
    if (ret != ERRCODE_SLE_SUCCESS) {
        g_ws63_sle_connecting_pending = false;
        osal_printk("[ws63 sle] connect request failed, ret=0x%x\r\n", (unsigned int)ret);
        ws63_sle_start_scan();
    }
}

/**
 * @brief 连接状态变化回调。
 */
static void ws63_sle_connect_state_changed_cb(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    (void)addr;
    (void)disc_reason;

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_ws63_sle_seek_started = false;
        g_ws63_sle_seek_stop_pending = false;
        g_ws63_sle_connecting_pending = false;
        g_ws63_sle_conn_id = conn_id;
        g_ws63_sle_peer_connected = true;
        WS63_SLE_LOG("connected, conn_id=%u", (unsigned int)conn_id);

        if (pair_state == SLE_PAIR_NONE) {
            (void)sle_pair_remote_device(&g_ws63_sle_remote_addr);
        }
        return;
    }

    if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_ws63_sle_seek_started = false;
        g_ws63_sle_seek_stop_pending = false;
        g_ws63_sle_connecting_pending = false;
        g_ws63_sle_conn_id = 0U;
        g_ws63_sle_peer_connected = false;
        g_ws63_sle_property_ready = false;
        g_ws63_sle_write_param.handle = 0U;
        WS63_SLE_LOG("disconnected, conn_id=%u", (unsigned int)conn_id);
        ws63_sle_start_scan();
    }
}

/**
 * @brief 配对完成回调：配对成功后请求 MTU 交换。
 */
static void ws63_sle_pair_complete_cb(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    ssap_exchange_info_t exchange_info = {0};

    (void)addr;

    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] pair failed, ret=0x%x\r\n", (unsigned int)status);
        return;
    }

    exchange_info.mtu_size = WS63_SLE_DEFAULT_MTU_SIZE;
    exchange_info.version = 1;
    WS63_SLE_LOG("pair success, request mtu exchange");
    (void)ssapc_exchange_info_req(0U, conn_id, &exchange_info);
}

/**
 * @brief MTU 交换完成回调：交换成功后启动特征发现。
 */
static void ws63_sle_exchange_info_cb(uint8_t client_id, uint16_t conn_id,
    ssap_exchange_info_t *param, errcode_t status)
{
    ssapc_find_structure_param_t find_param = {0};

    (void)client_id;

    if ((status != ERRCODE_SLE_SUCCESS) || (param == NULL)) {
        osal_printk("[ws63 sle] exchange info failed, ret=0x%x\r\n", (unsigned int)status);
        return;
    }

    WS63_SLE_LOG("mtu exchange success, mtu=%u", (unsigned int)param->mtu_size);
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1U;
    find_param.end_hdl = 0xFFFFU;
    (void)ssapc_find_structure(0U, conn_id, &find_param);
}

/**
 * @brief 服务发现过程回调。
 */
static void ws63_sle_find_structure_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_service_result_t *service, errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)service;
    (void)status;
}

/**
 * @brief 特征发现回调：命中目标 UUID 后记录可写句柄。
 */
static void ws63_sle_find_property_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_property_result_t *property, errcode_t status)
{
    uint16_t uuid16;

    (void)client_id;
    (void)conn_id;

    if ((status != ERRCODE_SLE_SUCCESS) || (property == NULL)) {
        return;
    }

    uuid16 = ws63_sle_get_uuid_u16(&property->uuid);
    if (uuid16 != WS63_SLE_PROPERTY_UUID) {
        return;
    }

    g_ws63_sle_write_param.handle = property->handle;
    g_ws63_sle_write_param.type = SSAP_PROPERTY_TYPE_VALUE;
    g_ws63_sle_property_ready = true;
    WS63_SLE_LOG("property ready, handle=%u", (unsigned int)property->handle);
}

/**
 * @brief 结构发现完成回调。
 */
static void ws63_sle_find_structure_cmp_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_find_structure_result_t *structure_result, errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)structure_result;
    (void)status;
}

/**
 * @brief 写确认回调。
 */
static void ws63_sle_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_write_result_t *write_result, errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)write_result;

    if (status != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] write confirm failed, ret=0x%x\r\n", (unsigned int)status);
    }
}

/**
 * @brief Notify 下行回调。
 */
static void ws63_sle_notification_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)status;

    if ((data == NULL) || (data->data == NULL) || (data->data_len == 0U)) {
        return;
    }

    WS63_SLE_LOG("notify downlink len=%u", (unsigned int)data->data_len);
    if (g_ws63_sle_downlink_cb != NULL) {
        (void)g_ws63_sle_downlink_cb(data->data, data->data_len);
    }
}

/**
 * @brief Indication 下行回调。
 */
static void ws63_sle_indication_cb(uint8_t client_id, uint16_t conn_id,
    ssapc_handle_value_t *data, errcode_t status)
{
    (void)client_id;
    (void)conn_id;
    (void)status;

    if ((data == NULL) || (data->data == NULL) || (data->data_len == 0U)) {
        return;
    }

    WS63_SLE_LOG("indication downlink len=%u", (unsigned int)data->data_len);
    if (g_ws63_sle_downlink_cb != NULL) {
        (void)g_ws63_sle_downlink_cb(data->data, data->data_len);
    }
}

/**
 * @brief 注册扫描回调。
 */
static errcode_t ws63_sle_register_seek_callbacks(void)
{
    g_ws63_sle_seek_cbks.sle_enable_cb = ws63_sle_enable_cb;
    g_ws63_sle_seek_cbks.seek_enable_cb = ws63_sle_seek_enable_cb;
    g_ws63_sle_seek_cbks.seek_result_cb = ws63_sle_seek_result_cb;
    g_ws63_sle_seek_cbks.seek_disable_cb = ws63_sle_seek_disable_cb;

    return sle_announce_seek_register_callbacks(&g_ws63_sle_seek_cbks);
}

/**
 * @brief 注册连接回调。
 */
static errcode_t ws63_sle_register_conn_callbacks(void)
{
    g_ws63_sle_conn_cbks.connect_state_changed_cb = ws63_sle_connect_state_changed_cb;
    g_ws63_sle_conn_cbks.pair_complete_cb = ws63_sle_pair_complete_cb;

    return sle_connection_register_callbacks(&g_ws63_sle_conn_cbks);
}

/**
 * @brief 注册 SSAPC 回调。
 */
static errcode_t ws63_sle_register_ssapc_callbacks(void)
{
    g_ws63_sle_ssapc_cbks.exchange_info_cb = ws63_sle_exchange_info_cb;
    g_ws63_sle_ssapc_cbks.find_structure_cb = ws63_sle_find_structure_cb;
    g_ws63_sle_ssapc_cbks.ssapc_find_property_cbk = ws63_sle_find_property_cb;
    g_ws63_sle_ssapc_cbks.find_structure_cmp_cb = ws63_sle_find_structure_cmp_cb;
    g_ws63_sle_ssapc_cbks.write_cfm_cb = ws63_sle_write_cfm_cb;
    g_ws63_sle_ssapc_cbks.notification_cb = ws63_sle_notification_cb;
    g_ws63_sle_ssapc_cbks.indication_cb = ws63_sle_indication_cb;

    return ssapc_register_callbacks(&g_ws63_sle_ssapc_cbks);
}

errcode_t ws63_sle_init(ws63_sle_downlink_cb_t downlink_cb)
{
    errcode_t ret;

    WS63_SLE_LOG("init start");

    g_ws63_sle_downlink_cb = downlink_cb;
    g_ws63_sle_seek_started = false;
    g_ws63_sle_seek_stop_pending = false;
    g_ws63_sle_connecting_pending = false;
    g_ws63_sle_conn_id = 0U;
    g_ws63_sle_peer_connected = false;
    g_ws63_sle_property_ready = false;
    g_ws63_sle_write_param.handle = 0U;

    ws63_sle_prepare_mac();

    ret = ws63_sle_register_seek_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] register seek callback failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = ws63_sle_register_conn_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] register conn callback failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = ws63_sle_register_ssapc_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[ws63 sle] register ssapc callback failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ws63 sle] enable sle failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = ws63_sle_apply_local_info();
    if (ret != ERRCODE_SLE_SUCCESS) {
        return ret;
    }

    ws63_sle_start_scan();
    WS63_SLE_LOG("init success");
    return ERRCODE_SUCC;
}

void ws63_sle_process(void)
{
    if (!g_ws63_sle_peer_connected && !g_ws63_sle_seek_started && !g_ws63_sle_connecting_pending) {
        ws63_sle_start_scan();
    }
}

bool ws63_sle_ready(void)
{
    return (g_ws63_sle_peer_connected && g_ws63_sle_property_ready && (g_ws63_sle_write_param.handle != 0U));
}

errcode_t ws63_sle_send_subport_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    const char *tag;
    errcode_t ret;

    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    tag = ws63_sle_get_tag_by_subport(sub_port);
    if (tag == NULL) {
        /* 未启用模块不转发，避免越权上行。 */
        return ERRCODE_FAIL;
    }

    ret = ws63_sle_send_tagged_data(tag, data, len);
    if (ret != ERRCODE_SUCC) {
        WS63_SLE_LOG("uplink send failed, sub_port=%u, len=%u, ret=0x%x",
            (unsigned int)sub_port, (unsigned int)len, (unsigned int)ret);
        return ret;
    }

    ws63_sle_log_uplink_success_limited(sub_port, len);
    return ERRCODE_SUCC;
}

/**
 * @brief 将调试日志文本按 DEBUG 标签上行到主机。
 */
errcode_t ws63_sle_send_debug_data(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    return ws63_sle_send_tagged_data(WS63_SLE_TAG_DEBUG, data, len);
}

errcode_t ws63_sle_set_uplink_success_log_enable(uint8_t enable)
{
    g_ws63_sle_uplink_success_log_enable = (enable != 0U) ? 1U : 0U;
    return ERRCODE_SUCC;
}

uint8_t ws63_sle_get_uplink_success_log_enable(void)
{
    return g_ws63_sle_uplink_success_log_enable;
}

errcode_t ws63_sle_set_uplink_success_log_gap_ms(uint32_t gap_ms)
{
    g_ws63_sle_uplink_success_log_gap_ms = gap_ms;
    g_ws63_sle_uplink_success_last_log_ms = 0U;
    g_ws63_sle_uplink_success_skip_count = 0U;
    return ERRCODE_SUCC;
}

uint32_t ws63_sle_get_uplink_success_log_gap_ms(void)
{
    return g_ws63_sle_uplink_success_log_gap_ms;
}

/**
 * @brief 统一执行“带标签”的 SLE 上行发送。
 *
 * @param tag  标签字符串。
 * @param data 原始数据。
 * @param len  原始长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
static errcode_t ws63_sle_send_tagged_data(const char *tag, const uint8_t *data, uint16_t len)
{
    uint8_t *payload = NULL;
    uint16_t payload_len = 0U;
    uint16_t offset = 0U;
    uint16_t remain;
    uint16_t chunk_len;
    uint16_t conn_id_snapshot;
    uint16_t handle_snapshot;
    errcode_t ret;

    if ((tag == NULL) || (data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (!ws63_sle_ready()) {
        return ERRCODE_FAIL;
    }

    ret = ws63_sle_build_payload(tag, data, len, &payload, &payload_len);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    conn_id_snapshot = g_ws63_sle_conn_id;
    handle_snapshot = g_ws63_sle_write_param.handle;

    while (offset < payload_len) {
        remain = (uint16_t)(payload_len - offset);
        chunk_len = (remain > (uint16_t)WS63_SLE_SAFE_CHUNK_LEN) ?
            (uint16_t)WS63_SLE_SAFE_CHUNK_LEN : remain;

        g_ws63_sle_write_param.handle = handle_snapshot;
        g_ws63_sle_write_param.type = SSAP_PROPERTY_TYPE_VALUE;
        g_ws63_sle_write_param.data = payload + offset;
        g_ws63_sle_write_param.data_len = chunk_len;

        ret = ssapc_write_cmd(0U, conn_id_snapshot, &g_ws63_sle_write_param);
        if (ret != ERRCODE_SLE_SUCCESS) {
            osal_vfree(payload);
            return ret;
        }

        offset = (uint16_t)(offset + chunk_len);
    }

    osal_vfree(payload);
    return ERRCODE_SUCC;
}

#else

errcode_t ws63_sle_init(ws63_sle_downlink_cb_t downlink_cb)
{
    (void)downlink_cb;
    return ERRCODE_SUCC;
}

void ws63_sle_process(void)
{
}

bool ws63_sle_ready(void)
{
    return false;
}

errcode_t ws63_sle_send_subport_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    (void)sub_port;
    (void)data;
    (void)len;
    return ERRCODE_FAIL;
}

errcode_t ws63_sle_send_debug_data(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
    return ERRCODE_FAIL;
}

errcode_t ws63_sle_set_uplink_success_log_enable(uint8_t enable)
{
    (void)enable;
    return ERRCODE_FAIL;
}

uint8_t ws63_sle_get_uplink_success_log_enable(void)
{
    return 0U;
}

errcode_t ws63_sle_set_uplink_success_log_gap_ms(uint32_t gap_ms)
{
    (void)gap_ms;
    return ERRCODE_FAIL;
}

uint32_t ws63_sle_get_uplink_success_log_gap_ms(void)
{
    return 0U;
}

#endif
