/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - SLE UART host advertisement module.
 */

#include "sle_uart_host_adv.h"

#include "securec.h"
#include "sle_common.h"
#include "sle_device_discovery.h"
#include "sle_errcode.h"
#include "soc_osal.h"

#include "sle_uart_host.h"

/* 广播句柄。该值与服务端其他位置保持一致。 */
#define MINE_SLE_ADV_HANDLE_DEFAULT 1

/* 广播/连接参数。单位请参考 SLE 协议说明（125us / 10ms）。 */
#define MINE_SLE_CONN_INTV_MIN_DEFAULT            0x32
#define MINE_SLE_CONN_INTV_MAX_DEFAULT            0x32
#define MINE_SLE_ADV_INTERVAL_MIN_DEFAULT         0xC8
#define MINE_SLE_ADV_INTERVAL_MAX_DEFAULT         0xC8
#define MINE_SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT 0x1F4
#define MINE_SLE_CONN_MAX_LATENCY                 0x1F3

/* 发送功率和广播数据最大长度。 */
#define MINE_SLE_ADV_TX_POWER 20
#define MINE_SLE_ADV_DATA_LEN_MAX 251

/**
 * @brief 将本地设备名写入扫描响应包。
 *
 * @param adv_data 广播缓冲区起始地址。
 * @param max_len  当前剩余可写长度。
 * @return uint16_t 实际写入长度。
 */
static uint16_t mine_sle_uart_set_adv_local_name(uint8_t *adv_data, uint16_t max_len)
{
    errno_t copy_ret;
    uint8_t index = 0;
    const uint8_t *local_name = (const uint8_t *)MINE_SLE_UART_HOST_NAME;
    uint8_t local_name_len = (uint8_t)(sizeof(MINE_SLE_UART_HOST_NAME) - 1);

    if ((adv_data == NULL) || (max_len < (uint16_t)(local_name_len + 2))) {
        return 0;
    }

    adv_data[index++] = local_name_len + 1;
    adv_data[index++] = SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME;

    copy_ret = memcpy_s(&adv_data[index], max_len - index, local_name, local_name_len);
    if (copy_ret != EOK) {
        osal_printk("[mine host adv] copy local name failed\r\n");
        return 0;
    }
    return (uint16_t)(index + local_name_len);
}

/**
 * @brief 组装广播包（announce data）。
 *
 * @param adv_data 广播数据缓冲区。
 * @return uint16_t 实际写入长度。
 */
static uint16_t mine_sle_uart_set_adv_data(uint8_t *adv_data)
{
    uint16_t index = 0;
    errno_t copy_ret;
    size_t common_len = sizeof(struct sle_adv_common_value);

    struct sle_adv_common_value adv_disc_level = {
        .length = common_len - 1,
        .type = SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL,
        .value = SLE_ANNOUNCE_LEVEL_NORMAL,
    };

    struct sle_adv_common_value adv_access_mode = {
        .length = common_len - 1,
        .type = SLE_ADV_DATA_TYPE_ACCESS_MODE,
        .value = 0,
    };

    copy_ret = memcpy_s(&adv_data[index], MINE_SLE_ADV_DATA_LEN_MAX - index, &adv_disc_level, common_len);
    if (copy_ret != EOK) {
        return 0;
    }
    index += (uint16_t)common_len;

    copy_ret = memcpy_s(&adv_data[index], MINE_SLE_ADV_DATA_LEN_MAX - index, &adv_access_mode, common_len);
    if (copy_ret != EOK) {
        return 0;
    }
    index += (uint16_t)common_len;

    return index;
}

/**
 * @brief 组装扫描响应包（scan response data）。
 *
 * @param scan_rsp_data 扫描响应缓冲区。
 * @return uint16_t 实际写入长度。
 */
static uint16_t mine_sle_uart_set_scan_rsp_data(uint8_t *scan_rsp_data)
{
    uint16_t index = 0;
    errno_t copy_ret;
    size_t common_len = sizeof(struct sle_adv_common_value);

    struct sle_adv_common_value tx_power = {
        .length = common_len - 1,
        .type = SLE_ADV_DATA_TYPE_TX_POWER_LEVEL,
        .value = MINE_SLE_ADV_TX_POWER,
    };

    copy_ret = memcpy_s(scan_rsp_data, MINE_SLE_ADV_DATA_LEN_MAX, &tx_power, common_len);
    if (copy_ret != EOK) {
        return 0;
    }
    index += (uint16_t)common_len;

    index += mine_sle_uart_set_adv_local_name(&scan_rsp_data[index], MINE_SLE_ADV_DATA_LEN_MAX - index);
    return index;
}

/**
 * @brief 配置广播参数。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_set_default_announce_param(void)
{
    sle_announce_param_t param = {0};

    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = MINE_SLE_ADV_HANDLE_DEFAULT;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = SLE_ADV_CHANNEL_MAP_DEFAULT;
    param.announce_interval_min = MINE_SLE_ADV_INTERVAL_MIN_DEFAULT;
    param.announce_interval_max = MINE_SLE_ADV_INTERVAL_MAX_DEFAULT;
    param.conn_interval_min = MINE_SLE_CONN_INTV_MIN_DEFAULT;
    param.conn_interval_max = MINE_SLE_CONN_INTV_MAX_DEFAULT;
    param.conn_max_latency = MINE_SLE_CONN_MAX_LATENCY;
    param.conn_supervision_timeout = MINE_SLE_CONN_SUPERVISION_TIMEOUT_DEFAULT;
    param.announce_tx_power = MINE_SLE_ADV_TX_POWER;

    return sle_set_announce_param(param.announce_handle, &param);
}

/**
 * @brief 配置广播数据与扫描响应数据。
 *
 * @return errcode_t
 */
static errcode_t mine_sle_uart_set_default_announce_data(void)
{
    sle_announce_data_t data = {0};
    uint8_t announce_data[MINE_SLE_ADV_DATA_LEN_MAX] = {0};
    uint8_t scan_rsp_data[MINE_SLE_ADV_DATA_LEN_MAX] = {0};

    data.announce_data_len = (uint8_t)mine_sle_uart_set_adv_data(announce_data);
    data.announce_data = announce_data;
    data.seek_rsp_data_len = (uint8_t)mine_sle_uart_set_scan_rsp_data(scan_rsp_data);
    data.seek_rsp_data = scan_rsp_data;

    return sle_set_announce_data(MINE_SLE_ADV_HANDLE_DEFAULT, &data);
}

/**
 * @brief 广播开启回调。
 */
static void mine_sle_uart_announce_enable_cb(uint32_t announce_id, errcode_t status)
{
    osal_printk("[mine host adv] announce enable id:%u status:%x\r\n", announce_id, status);
}

/**
 * @brief 广播关闭回调。
 */
static void mine_sle_uart_announce_disable_cb(uint32_t announce_id, errcode_t status)
{
    osal_printk("[mine host adv] announce disable id:%u status:%x\r\n", announce_id, status);
}

/**
 * @brief 广播终止回调。
 */
static void mine_sle_uart_announce_terminal_cb(uint32_t announce_id)
{
    osal_printk("[mine host adv] announce terminal id:%u\r\n", announce_id);
}

errcode_t mine_sle_uart_host_adv_register_callbacks(void)
{
    sle_announce_seek_callbacks_t seek_cbks = {0};

    seek_cbks.announce_enable_cb = mine_sle_uart_announce_enable_cb;
    seek_cbks.announce_disable_cb = mine_sle_uart_announce_disable_cb;
    seek_cbks.announce_terminal_cb = mine_sle_uart_announce_terminal_cb;

    return sle_announce_seek_register_callbacks(&seek_cbks);
}

errcode_t mine_sle_uart_host_adv_init(void)
{
    errcode_t ret;

    ret = mine_sle_uart_host_adv_register_callbacks();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host adv] register callbacks failed:%x\r\n", ret);
        return ret;
    }

    ret = mine_sle_uart_set_default_announce_param();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host adv] set announce param failed:%x\r\n", ret);
        return ret;
    }

    ret = mine_sle_uart_set_default_announce_data();
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host adv] set announce data failed:%x\r\n", ret);
        return ret;
    }

    ret = sle_start_announce(MINE_SLE_ADV_HANDLE_DEFAULT);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[mine host adv] start announce failed:%x\r\n", ret);
        return ret;
    }

    osal_printk("[mine host adv] advertise started, name:%s\r\n", MINE_SLE_UART_HOST_NAME);
    return ERRCODE_SLE_SUCCESS;
}
