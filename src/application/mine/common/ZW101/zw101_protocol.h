/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: ZW101 fingerprint protocol adapter.
 */

#ifndef ZW101_PROTOCOL_H
#define ZW101_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define ZW101_WAIT_UP_TIME 6000

#define ZW101_ENROLL_COUNTER (5)
#define ZW101_EMPTY_ACK_TIMEOUT (2000)

#define ZW101_COMMON_TIMEOUT (1000)
#define ZW101_MATCH_TIMEOUT 2300
#define ZW101_DEFAULT_BAUD (57600)
#define ZW101_PWRON_WAIT_PERIOD (300)
#define ZW101_SLEEP_TIMEOUT (400)
#define ZW101_CAPTURE_TIMEOUT (480)
#define ZW101_RGBCTRL_TIMEOUT (780)

#define ZW101_FIRST_HEAD (0xEF)
#define ZW101_SECOND_HEAD (0x01)

#define ZW101_CMD_PKG (0x01)
#define ZW101_DATA_PKG (0x02)
#define ZW101_EOF_PKG (0x08)

#define ZW101_CALC_SUM_START_POS (6)
#define ZW101_CMD_CODE_START_POS (9)
#define ZW101_VARIABLE_FIELD_START_POS (10)

#define ZW101_PROTOCOL_RCV_BUFFER_SIZE (256)
#define ZW101_PROTOCOL_CMD_BUFFER_SIZE (64)
#define ZW101_DEVICE_ADDR_DEFAULT (0xFFFFFFFFU)

#define ZW101_PS_OK 0x00
#define ZW101_PS_NO_FINGER 0x02
#define ZW101_PS_NOT_MATCH 0x08
#define ZW101_PS_NOT_SEARCHED 0x09
#define ZW101_PS_ADDRESS_OVER 0x0B
#define ZW101_PS_ENROLL_ERR 0x1E
#define ZW101_PS_LIB_FULL_ERR 0x1F
#define ZW101_PS_TMPL_NOT_EMPTY 0x22
#define ZW101_PS_TMPL_EMPTY 0x23
#define ZW101_PS_TIME_OUT 0x26
#define ZW101_PS_FP_DUPLICATION 0x27
#define ZW101_PS_ENROLL_CANCEL 0x2C
#define ZW101_PS_IMAGE_SMALL 0x33
#define ZW101_PS_IMAGE_UNAVAILABLE 0x34
#define ZW101_PS_ENROLL_TIMES_NOT_ENOUGH 0x40
#define ZW101_PS_COMMUNICATE_TIMEOUT 0x41

typedef enum {
    ZW101_CMD_MATCH_GETIMAGE = 0x01,
    ZW101_CMD_ENROLL_GETIMAGE = 0x29,
    ZW101_CMD_GEN_EXTRACT = 0x02,
    ZW101_CMD_SEARCH_TEMPLATE = 0x04,
    ZW101_CMD_GEN_TEMPLATE = 0x05,
    ZW101_CMD_STORE_TEMPLATE = 0x06,
    ZW101_CMD_DEL_TEMPLATE = 0x0C,
    ZW101_CMD_EMPTY_TEMPLATE = 0x0D,
    ZW101_CMD_WRITE_SYSPARA = 0x0E,
    ZW101_CMD_READ_SYSPARA = 0x0F,
    ZW101_CMD_READ_VALID_TEMPLATE_NUMS = 0x1D,
    ZW101_CMD_READ_TEMPLATE_INDEX_TABLE = 0x1F,
    ZW101_CMD_AUTO_CANCEL = 0x30,
    ZW101_CMD_AUTO_ENROLL = 0x31,
    ZW101_CMD_AUTO_MATCH = 0x32,
    ZW101_CMD_INTO_SLEEP = 0x33,
    ZW101_CMD_HANDSHAKE = 0x35,
    ZW101_CMD_RGB_CTRL = 0x3C,
    ZW101_CMD_CHECK_FINGER = 0x9D,
} zw101_cmd_t;

typedef enum {
    ZW101_RGB_BREATH = 0x01,
    ZW101_RGB_FLICK,
    ZW101_RGB_OPEN,
    ZW101_RGB_OFF,
    ZW101_RGB_GRADUAL_OPEN,
    ZW101_RGB_GRADUAL_OFF,
    ZW101_RGB_HORSE,
} zw101_rgb_func_t;

typedef enum {
    ZW101_RGB_COLOR_OFF = 0x00,
    ZW101_RGB_COLOR_B,
    ZW101_RGB_COLOR_G,
    ZW101_RGB_COLOR_GB,
    ZW101_RGB_COLOR_R,
    ZW101_RGB_COLOR_RB,
    ZW101_RGB_COLOR_RG,
    ZW101_RGB_COLOR_RGB,
    ZW101_RGB_COLOR_RGLOOP = 0x16,
    ZW101_RGB_COLOR_RBLOOP = 0x15,
    ZW101_RGB_COLOR_GBLOOP = 0x13,
    ZW101_RGB_COLOR_RGBLOOP = 0x17,
    ZW101_RGB_COLOR_RGALTERNATE = 0x26,
    ZW101_RGB_COLOR_RBALTERNATE = 0x25,
    ZW101_RGB_COLOR_GBALTERNATE = 0x23,
} zw101_rgb_color_t;

typedef enum {
    ZW101_RGB_DUTY_RLT_SUCC = 0x91,
    ZW101_RGB_DUTY_NORMAL = 0x11,
} zw101_rgb_duty_t;

typedef enum {
    ZW101_ACK_SUCCESS = 0x00,
    ZW101_ACK_ERR_PKG_RCV = 0x01,
    ZW101_ACK_ERR_NO_FINGER = 0x02,
} zw101_ack_t;

typedef enum {
    ZW101_RCV_FIRST_HEAD = 0x01,
    ZW101_RCV_SECOND_HEAD,
    ZW101_RCV_PKG_SIZE,
    ZW101_RCV_DATA,
} zw101_frame_state_t;

#pragma pack(1)
typedef struct {
    uint16_t header;
    uint32_t dev_addr;
    uint8_t pkg_identification;
    uint16_t data_size;
    uint8_t data[];
} zw101_pkg_t;
#pragma pack()

typedef struct {
    int (*uart_send)(const uint8_t *data, uint16_t size);
    uint32_t (*get_tick_ms)(void);
    void (*delay_ms)(uint32_t ms);
} zw101_hal_t;

typedef struct {
    uint8_t cmd;
    uint8_t ack_code;
    uint8_t pkg_identification;
    const uint8_t *payload;
    uint16_t payload_len;
} zw101_ack_evt_t;

typedef void (*zw101_ack_callback_t)(const zw101_ack_evt_t *evt);
typedef void (*zw101_packet_callback_t)(const uint8_t *packet, uint16_t size);

typedef struct {
    zw101_hal_t hal;

    uint8_t cmd_buf[ZW101_PROTOCOL_CMD_BUFFER_SIZE];
    uint16_t cmd_size;

    uint8_t ack_cmd;
    bool waiting_ack;
    bool ack_done;
    uint8_t ack_code;

    zw101_ack_callback_t ack_cb;
    zw101_packet_callback_t packet_cb;

    uint16_t sum;
    uint16_t rcv_size;
    uint16_t rcv_pkg_dlen;
    zw101_frame_state_t rcv_state;
    uint8_t rcv_buf[ZW101_PROTOCOL_RCV_BUFFER_SIZE];
} zw101_context_t;

void zw101_init(zw101_context_t *ctx, const zw101_hal_t *hal);
void zw101_set_callbacks(zw101_context_t *ctx, zw101_ack_callback_t ack_cb, zw101_packet_callback_t packet_cb);
void zw101_reset_protocol_parse(zw101_context_t *ctx);
void zw101_protocol_parse(zw101_context_t *ctx, const uint8_t *data, uint16_t len);
int zw101_pkg_handle(zw101_context_t *ctx, const uint8_t *data, uint16_t size);

int zw101_send_command(zw101_context_t *ctx, uint8_t cmd, const uint8_t *params, uint16_t params_len);
int zw101_wait_ack(zw101_context_t *ctx, uint8_t cmd, uint32_t timeout_ms, uint8_t *ack_code);

int zw101_cmd_handshake(zw101_context_t *ctx, uint8_t *ack_code);
int zw101_cmd_check_finger(zw101_context_t *ctx);
int zw101_cmd_into_sleep(zw101_context_t *ctx);
int zw101_cmd_rgb_ctrl(zw101_context_t *ctx, zw101_rgb_func_t func_code, zw101_rgb_color_t start_color,
    uint8_t end_color_or_duty, uint8_t loop_times, uint8_t cycle);
int zw101_cmd_capture_image(zw101_context_t *ctx, uint8_t operate_cmd);
int zw101_cmd_general_extract(zw101_context_t *ctx, uint8_t buffer_id);
int zw101_cmd_general_template(zw101_context_t *ctx);
int zw101_cmd_store_template(zw101_context_t *ctx, uint8_t buffer_id, uint16_t page_id);
int zw101_cmd_match1n(zw101_context_t *ctx, uint8_t buffer_id, uint16_t start_page, uint16_t page_num);
int zw101_cmd_del_template(zw101_context_t *ctx, uint16_t template_id, uint16_t template_nums);
int zw101_cmd_empty_template(zw101_context_t *ctx);
int zw101_cmd_get_id_availability(zw101_context_t *ctx, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* ZW101_PROTOCOL_H */
