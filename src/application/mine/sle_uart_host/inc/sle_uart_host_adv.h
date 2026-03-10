/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - SLE UART host advertisement module.
 */

#ifndef MINE_SLE_UART_HOST_ADV_H
#define MINE_SLE_UART_HOST_ADV_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 通用广播字段结构（type + length + value）。
 */
struct sle_adv_common_value {
	uint8_t type;
	uint8_t length;
	uint8_t value;
};

/**
 * @brief 广播信道图定义。
 */
typedef enum mine_sle_adv_channel_map {
	SLE_ADV_CHANNEL_MAP_77 = 0x01,
	SLE_ADV_CHANNEL_MAP_78 = 0x02,
	SLE_ADV_CHANNEL_MAP_79 = 0x04,
	SLE_ADV_CHANNEL_MAP_DEFAULT = 0x07
} mine_sle_adv_channel_map_t;

/**
 * @brief 广播数据类型定义。
 */
typedef enum mine_sle_adv_data_type {
	SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL = 0x01,
	SLE_ADV_DATA_TYPE_ACCESS_MODE = 0x02,
	SLE_ADV_DATA_TYPE_SERVICE_DATA_16BIT_UUID = 0x03,
	SLE_ADV_DATA_TYPE_SERVICE_DATA_128BIT_UUID = 0x04,
	SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x05,
	SLE_ADV_DATA_TYPE_COMPLETE_LIST_OF_128BIT_SERVICE_UUIDS = 0x06,
	SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_16BIT_SERVICE_UUIDS = 0x07,
	SLE_ADV_DATA_TYPE_INCOMPLETE_LIST_OF_128BIT_SERVICE_UUIDS = 0x08,
	SLE_ADV_DATA_TYPE_SERVICE_STRUCTURE_HASH_VALUE = 0x09,
	SLE_ADV_DATA_TYPE_SHORTENED_LOCAL_NAME = 0x0A,
	SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME = 0x0B,
	SLE_ADV_DATA_TYPE_TX_POWER_LEVEL = 0x0C,
	SLE_ADV_DATA_TYPE_SLB_COMMUNICATION_DOMAIN = 0x0D,
	SLE_ADV_DATA_TYPE_SLB_MEDIA_ACCESS_LAYER_ID = 0x0E,
	SLE_ADV_DATA_TYPE_EXTENDED = 0xFE,
	SLE_ADV_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA = 0xFF
} mine_sle_adv_data_type_t;

/**
 * @brief 注册广播相关回调。
 *
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 注册成功。
 * @retval 其他值             注册失败。
 */
errcode_t mine_sle_uart_host_adv_register_callbacks(void);

/**
 * @brief 初始化并启动 Host 广播。
 *
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 初始化成功。
 * @retval 其他值             初始化失败。
 */
errcode_t mine_sle_uart_host_adv_init(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* MINE_SLE_UART_HOST_ADV_H */
