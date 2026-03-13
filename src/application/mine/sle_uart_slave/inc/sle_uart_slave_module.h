/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: Mine demo - Slave side internal shared declarations.
 */

#ifndef MINE_SLE_UART_SLAVE_MODULE_H
#define MINE_SLE_UART_SLAVE_MODULE_H

#include <stdbool.h>
#include <stdint.h>

#include "sle_uart_slave.h"

/* UART parameters. */
#define MINE_UART_RX_BUFFER_SIZE 512
#define MINE_UART_BUS_COUNT 3
#define MINE_UART_BUS_INVALID 0xFF

/* SLE parameters. */
#define MINE_SLE_DEFAULT_MTU_SIZE 512
#define MINE_SLE_SAFE_CHUNK_LEN 200
#define MINE_SLE_SEEK_INTERVAL_DEFAULT 100
#define MINE_SLE_SEEK_WINDOW_DEFAULT 100
#define MINE_UUID_BASE_INDEX_14 14
#define MINE_UUID_BASE_INDEX_15 15
#define MINE_SLE_MAC_ADDR_LEN 6

/* Task timing parameters. */
#define MINE_INIT_DELAY_MS 500
#define MINE_TASK_LOOP_WAIT_MS 80

/* OLED panel layout. */
#define MINE_OLED_LINE_COUNT 8
#define MINE_OLED_LINE_CHARS 21
#define MINE_OLED_DATA_LINE_COUNT 3
#define MINE_OLED_DATA_TOTAL_CHARS (MINE_OLED_LINE_CHARS * MINE_OLED_DATA_LINE_COUNT)
#define MINE_OLED_FONT_SIZE TEXT_SIZE_8
#define MINE_OLED_EVENT_BUFFER_LEN 64
#define MINE_OLED_LINE_TITLE 0
#define MINE_OLED_LINE_STATE 1
#define MINE_OLED_LINE_DATA0 2
#define MINE_OLED_LINE_DATA1 3
#define MINE_OLED_LINE_DATA2 4
#define MINE_OLED_LINE_UUID 5
#define MINE_OLED_LINE_RX 6
#define MINE_OLED_LINE_TX 7

/* Logging. */
#define MINE_LOG_BUFFER_LEN 192

/* LD2402 status cache. */
#define MINE_LD2402_STATUS_TEXT_LEN 48
#define MINE_LD2402_SN_MAX_LEN 16

/* ZW101 status cache. */
#define MINE_ZW101_STATUS_TEXT_LEN 48

/**
 * @brief Slave 统一日志输出接口。
 *
 * 该接口会将日志同步输出到 OSAL 与 PRINT 通道，
 * 便于同时在系统日志和串口日志中定位问题。
 *
 * @param fmt printf 风格格式化字符串。
 */
void mine_slave_log(const char *fmt, ...);

/**
 * @brief 判断指定 UART 总线在当前掩码配置下是否启用。
 *
 * @param bus UART 总线号。
 * @return true  总线已启用。
 * @return false 总线未启用或参数越界。
 */
bool mine_slave_uart_bus_enabled(uart_bus_t bus);

/**
 * @brief 将 UART 总线号转换为可读名称。
 *
 * @param bus UART 总线号。
 * @return const char* UART 名称字符串。
 */
const char *mine_slave_uart_bus_name(uint8_t bus);

/**
 * @brief 向所有已启用 UART 总线广播写入数据。
 *
 * @param data 待发送数据指针。
 * @param len  待发送数据长度。
 */
void mine_slave_uart_write_enabled_buses(const uint8_t *data, uint16_t len);

/**
 * @brief 初始化 Slave 侧 OLED 页面和统计状态。
 */
void mine_slave_oled_init(void);

/**
 * @brief 刷新 OLED 的脏行数据到屏幕。
 */
void mine_slave_oled_flush_pending(void);

/**
 * @brief 在 OLED 状态行写入文本。
 *
 * @param text 状态文本（例如 SCANNING、PROP READY）。
 */
void mine_slave_oled_push_state(const char *text);

/**
 * @brief 上报一条数据事件到 OLED（方向、UART 来源、长度与预览）。
 *
 * @param uart_bus 数据来源 UART 总线。
 * @param prefix   事件前缀，用于识别 RX/TX 方向。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 */
void mine_slave_oled_push_data_event(uint8_t uart_bus, const char *prefix, const uint8_t *data, uint16_t len);

/**
 * @brief 将 OLED UUID 行恢复为默认服务/特征显示。
 */
void mine_slave_oled_set_uuid_default(void);

/**
 * @brief 更新 OLED UUID 行为已发现特征的 UUID 与句柄。
 *
 * @param uuid16  16 位特征 UUID。
 * @param handle  特征句柄。
 */
void mine_slave_oled_set_uuid_property(uint16_t uuid16, uint16_t handle);

/**
 * @brief 将 UART 消息通过 SSAPC 写命令发送到 Host。
 *
 * @param msg UART 到 SLE 的待发送消息。
 * @return errcode_t
 * @retval ERRCODE_SLE_SUCCESS 发送成功。
 * @retval 其他值             发送失败。
 */
errcode_t mine_sle_uart_slave_send_to_host(const mine_sle_uart_slave_msg_t *msg);

#endif /* MINE_SLE_UART_SLAVE_MODULE_H */