/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * Description: ZW101 fingerprint protocol adapter.
 */

#include "zw101_protocol.h"

#include <string.h>

#include "securec.h"

static const uint8_t g_zw101_fixed_cmd_match_capture_image[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, ZW101_CMD_MATCH_GETIMAGE, 0x00, 0x05
};
static const uint8_t g_zw101_fixed_cmd_enroll_capture_image[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, ZW101_CMD_ENROLL_GETIMAGE, 0x00, 0x2D
};
static const uint8_t g_zw101_fixed_cmd_general_template[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, ZW101_CMD_GEN_TEMPLATE, 0x00, 0x09
};
static const uint8_t g_zw101_variable_cmd_read_index_table[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, ZW101_CMD_READ_TEMPLATE_INDEX_TABLE, 0x00, 0x00, 0x00
};
static const uint8_t g_zw101_fixed_cmd_empty_template[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, ZW101_CMD_EMPTY_TEMPLATE, 0x00, 0x11
};
static const uint8_t g_zw101_fixed_cmd_sleep[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, ZW101_CMD_INTO_SLEEP, 0x00, 0x37
};

static const uint8_t g_zw101_variable_cmd_general_extract[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, ZW101_CMD_GEN_EXTRACT, 0x00, 0x00, 0x00
};
static const uint8_t g_zw101_variable_cmd_search_template[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, ZW101_CMD_SEARCH_TEMPLATE,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t g_zw101_variable_cmd_store_template[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x06, ZW101_CMD_STORE_TEMPLATE,
    0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t g_zw101_variable_cmd_del_template[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, ZW101_CMD_DEL_TEMPLATE,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t g_zw101_variable_cmd_rgb_ctrl[] = {
    0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, ZW101_CMD_RGB_CTRL,
    ZW101_RGB_BREATH, ZW101_RGB_COLOR_B, ZW101_RGB_COLOR_B, 0x00, 0x0F, 0x00, 0x00
};

static uint16_t zw101_calc_sum(const uint8_t *data, uint16_t size)
{
    uint16_t sum = 0;
    uint16_t pos;

    for (pos = 0; pos < size; pos++) {
        sum = (uint16_t)(sum + data[pos]);
    }
    return sum;
}

static void zw101_set_checksum(uint8_t *packet, uint16_t size)
{
    uint16_t sum;

    if ((packet == NULL) || (size < 2)) {
        return;
    }

    sum = zw101_calc_sum(&packet[ZW101_CALC_SUM_START_POS], (uint16_t)(size - 8));
    packet[size - 2] = (uint8_t)(sum >> 8);
    packet[size - 1] = (uint8_t)sum;
}

void zw101_init(zw101_context_t *ctx, const zw101_hal_t *hal)
{
    if ((ctx == NULL) || (hal == NULL)) {
        return;
    }

    if (memset_s(ctx, sizeof(*ctx), 0, sizeof(*ctx)) != EOK) {
        return;
    }

    ctx->hal = *hal;
    ctx->rcv_state = ZW101_RCV_FIRST_HEAD;
}

void zw101_set_callbacks(zw101_context_t *ctx, zw101_ack_callback_t ack_cb, zw101_packet_callback_t packet_cb)
{
    if (ctx == NULL) {
        return;
    }

    ctx->ack_cb = ack_cb;
    ctx->packet_cb = packet_cb;
}

void zw101_reset_protocol_parse(zw101_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->sum = 0;
    ctx->rcv_size = 0;
    ctx->rcv_pkg_dlen = 0;
    ctx->rcv_state = ZW101_RCV_FIRST_HEAD;
}

int zw101_send_command(zw101_context_t *ctx, uint8_t cmd, const uint8_t *params, uint16_t params_len)
{
    uint16_t frame_size;
    uint16_t data_len;

    if ((ctx == NULL) || (ctx->hal.uart_send == NULL)) {
        return -1;
    }

    data_len = (uint16_t)(params_len + 3);
    frame_size = (uint16_t)(params_len + 12);

    if (frame_size > ZW101_PROTOCOL_CMD_BUFFER_SIZE) {
        return -1;
    }

    ctx->cmd_buf[0] = ZW101_FIRST_HEAD;
    ctx->cmd_buf[1] = ZW101_SECOND_HEAD;
    ctx->cmd_buf[2] = 0xFF;
    ctx->cmd_buf[3] = 0xFF;
    ctx->cmd_buf[4] = 0xFF;
    ctx->cmd_buf[5] = 0xFF;
    ctx->cmd_buf[6] = ZW101_CMD_PKG;
    ctx->cmd_buf[7] = (uint8_t)(data_len >> 8);
    ctx->cmd_buf[8] = (uint8_t)data_len;
    ctx->cmd_buf[9] = cmd;

    if ((params_len > 0) && (params != NULL)) {
        if (memcpy_s(&ctx->cmd_buf[ZW101_VARIABLE_FIELD_START_POS],
            (size_t)(ZW101_PROTOCOL_CMD_BUFFER_SIZE - ZW101_VARIABLE_FIELD_START_POS), params, params_len) != EOK) {
            return -1;
        }
    }

    zw101_set_checksum(ctx->cmd_buf, frame_size);

    ctx->cmd_size = frame_size;
    ctx->ack_cmd = cmd;
    ctx->waiting_ack = true;
    ctx->ack_done = false;
    ctx->ack_code = 0xFF;

    if (ctx->hal.uart_send(ctx->cmd_buf, frame_size) != 0) {
        ctx->waiting_ack = false;
        return -1;
    }

    return 0;
}

int zw101_wait_ack(zw101_context_t *ctx, uint8_t cmd, uint32_t timeout_ms, uint8_t *ack_code)
{
    uint32_t start_ms;

    if ((ctx == NULL) || (ctx->hal.get_tick_ms == NULL) || (ctx->hal.delay_ms == NULL)) {
        return -1;
    }

    if (ctx->ack_cmd != cmd) {
        return -1;
    }

    start_ms = ctx->hal.get_tick_ms();
    while (!ctx->ack_done) {
        if ((uint32_t)(ctx->hal.get_tick_ms() - start_ms) >= timeout_ms) {
            ctx->waiting_ack = false;
            return -1;
        }
        ctx->hal.delay_ms(1);
    }

    ctx->waiting_ack = false;
    if (ack_code != NULL) {
        *ack_code = ctx->ack_code;
    }

    return (ctx->ack_code == ZW101_ACK_SUCCESS) ? 0 : -1;
}

int zw101_pkg_handle(zw101_context_t *ctx, const uint8_t *data, uint16_t size)
{
    const zw101_pkg_t *pkg;
    uint16_t payload_len;
    const uint8_t *payload;
    zw101_ack_evt_t evt;

    if ((ctx == NULL) || (data == NULL) || (size < 12)) {
        return -1;
    }

    pkg = (const zw101_pkg_t *)data;
    if (pkg->data_size < 2) {
        return -1;
    }

    payload_len = (uint16_t)(pkg->data_size - 2);
    payload = pkg->data;

    if (payload_len > 0) {
        ctx->ack_code = payload[0];
        ctx->ack_done = true;
    }

    if (ctx->packet_cb != NULL) {
        ctx->packet_cb(data, size);
    }

    if ((ctx->ack_cb != NULL) && (payload_len > 0)) {
        evt.cmd = ctx->ack_cmd;
        evt.ack_code = payload[0];
        evt.pkg_identification = pkg->pkg_identification;
        evt.payload = payload;
        evt.payload_len = payload_len;
        ctx->ack_cb(&evt);
    }

    return 0;
}

void zw101_protocol_parse(zw101_context_t *ctx, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t byte;

    if ((ctx == NULL) || (data == NULL) || (len == 0)) {
        return;
    }

    for (i = 0; i < len; i++) {
        byte = data[i];

        switch (ctx->rcv_state) {
            case ZW101_RCV_FIRST_HEAD:
                if (byte == ZW101_FIRST_HEAD) {
                    ctx->sum = 0;
                    ctx->rcv_size = 0;
                    ctx->rcv_pkg_dlen = 0;
                    ctx->rcv_buf[ctx->rcv_size++] = byte;
                    ctx->rcv_state = ZW101_RCV_SECOND_HEAD;
                }
                break;

            case ZW101_RCV_SECOND_HEAD:
                if (byte == ZW101_SECOND_HEAD) {
                    ctx->rcv_buf[ctx->rcv_size++] = byte;
                    ctx->rcv_state = ZW101_RCV_PKG_SIZE;
                } else {
                    zw101_reset_protocol_parse(ctx);
                }
                break;

            case ZW101_RCV_PKG_SIZE:
                if (ctx->rcv_size >= ZW101_PROTOCOL_RCV_BUFFER_SIZE) {
                    zw101_reset_protocol_parse(ctx);
                    break;
                }

                ctx->rcv_buf[ctx->rcv_size++] = byte;
                if (ctx->rcv_size >= 9) {
                    ctx->rcv_pkg_dlen = (uint16_t)((ctx->rcv_buf[7] << 8) + ctx->rcv_buf[8]);
                    if ((uint32_t)ctx->rcv_pkg_dlen + 9U > ZW101_PROTOCOL_RCV_BUFFER_SIZE) {
                        zw101_reset_protocol_parse(ctx);
                    } else {
                        ctx->rcv_state = ZW101_RCV_DATA;
                    }
                }
                break;

            case ZW101_RCV_DATA:
                if (ctx->rcv_size >= ZW101_PROTOCOL_RCV_BUFFER_SIZE) {
                    zw101_reset_protocol_parse(ctx);
                    break;
                }

                ctx->rcv_buf[ctx->rcv_size++] = byte;
                if (ctx->rcv_size >= (uint16_t)(9 + ctx->rcv_pkg_dlen)) {
                    uint16_t calc_sum;
                    uint8_t sum_h;
                    uint8_t sum_l;

                    calc_sum = zw101_calc_sum(&ctx->rcv_buf[ZW101_CALC_SUM_START_POS],
                        (uint16_t)(ctx->rcv_size - 8));
                    sum_h = (uint8_t)(calc_sum >> 8);
                    sum_l = (uint8_t)calc_sum;

                    if ((ctx->rcv_buf[ctx->rcv_size - 2] == sum_h) &&
                        (ctx->rcv_buf[ctx->rcv_size - 1] == sum_l)) {
                        (void)zw101_pkg_handle(ctx, ctx->rcv_buf, ctx->rcv_size);
                    }
                    zw101_reset_protocol_parse(ctx);
                }
                break;

            default:
                zw101_reset_protocol_parse(ctx);
                break;
        }

        if (ctx->rcv_size >= ZW101_PROTOCOL_RCV_BUFFER_SIZE) {
            zw101_reset_protocol_parse(ctx);
        }
    }
}

static int zw101_send_variable_cmd(zw101_context_t *ctx, const uint8_t *base_cmd, uint16_t cmd_size,
    const uint8_t *var_data, uint16_t var_len)
{
    if ((ctx == NULL) || (base_cmd == NULL) || (cmd_size > ZW101_PROTOCOL_CMD_BUFFER_SIZE)) {
        return -1;
    }

    if (memcpy_s(ctx->cmd_buf, sizeof(ctx->cmd_buf), base_cmd, cmd_size) != EOK) {
        return -1;
    }

    if ((var_len > 0) && (var_data != NULL)) {
        if (memcpy_s(&ctx->cmd_buf[ZW101_VARIABLE_FIELD_START_POS],
            (size_t)(sizeof(ctx->cmd_buf) - ZW101_VARIABLE_FIELD_START_POS), var_data, var_len) != EOK) {
            return -1;
        }
    }

    zw101_set_checksum(ctx->cmd_buf, cmd_size);

    ctx->cmd_size = cmd_size;
    ctx->ack_cmd = ctx->cmd_buf[ZW101_CMD_CODE_START_POS];
    ctx->waiting_ack = true;
    ctx->ack_done = false;
    ctx->ack_code = 0xFF;

    if (ctx->hal.uart_send(ctx->cmd_buf, cmd_size) != 0) {
        ctx->waiting_ack = false;
        return -1;
    }

    return 0;
}

int zw101_cmd_handshake(zw101_context_t *ctx, uint8_t *ack_code)
{
    if (zw101_send_command(ctx, ZW101_CMD_HANDSHAKE, NULL, 0) != 0) {
        return -1;
    }
    return zw101_wait_ack(ctx, ZW101_CMD_HANDSHAKE, ZW101_COMMON_TIMEOUT, ack_code);
}

int zw101_cmd_check_finger(zw101_context_t *ctx)
{
    if (zw101_send_command(ctx, ZW101_CMD_CHECK_FINGER, NULL, 0) != 0) {
        return -1;
    }
    return zw101_wait_ack(ctx, ZW101_CMD_CHECK_FINGER, ZW101_COMMON_TIMEOUT, NULL);
}

int zw101_cmd_into_sleep(zw101_context_t *ctx)
{
    if ((ctx == NULL) || (ctx->hal.uart_send == NULL)) {
        return -1;
    }

    if (memcpy_s(ctx->cmd_buf, sizeof(ctx->cmd_buf), g_zw101_fixed_cmd_sleep,
        sizeof(g_zw101_fixed_cmd_sleep)) != EOK) {
        return -1;
    }

    ctx->ack_cmd = ZW101_CMD_INTO_SLEEP;
    ctx->waiting_ack = true;
    ctx->ack_done = false;
    ctx->ack_code = 0xFF;

    if (ctx->hal.uart_send(ctx->cmd_buf, sizeof(g_zw101_fixed_cmd_sleep)) != 0) {
        ctx->waiting_ack = false;
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_INTO_SLEEP, ZW101_SLEEP_TIMEOUT, NULL);
}

int zw101_cmd_rgb_ctrl(zw101_context_t *ctx, zw101_rgb_func_t func_code, zw101_rgb_color_t start_color,
    uint8_t end_color_or_duty, uint8_t loop_times, uint8_t cycle)
{
    uint8_t var_data[5];

    var_data[0] = (uint8_t)func_code;
    var_data[1] = (uint8_t)start_color;
    var_data[2] = end_color_or_duty;
    var_data[3] = loop_times;
    var_data[4] = cycle;

    if (zw101_send_variable_cmd(ctx, g_zw101_variable_cmd_rgb_ctrl,
        sizeof(g_zw101_variable_cmd_rgb_ctrl), var_data, sizeof(var_data)) != 0) {
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_RGB_CTRL, ZW101_RGBCTRL_TIMEOUT, NULL);
}

int zw101_cmd_capture_image(zw101_context_t *ctx, uint8_t operate_cmd)
{
    const uint8_t *cmd = g_zw101_fixed_cmd_match_capture_image;
    uint16_t cmd_size = sizeof(g_zw101_fixed_cmd_match_capture_image);

    if (operate_cmd == ZW101_CMD_ENROLL_GETIMAGE) {
        cmd = g_zw101_fixed_cmd_enroll_capture_image;
        cmd_size = sizeof(g_zw101_fixed_cmd_enroll_capture_image);
    }

    if ((ctx == NULL) || (ctx->hal.uart_send == NULL)) {
        return -1;
    }

    if (memcpy_s(ctx->cmd_buf, sizeof(ctx->cmd_buf), cmd, cmd_size) != EOK) {
        return -1;
    }

    ctx->ack_cmd = ctx->cmd_buf[ZW101_CMD_CODE_START_POS];
    ctx->waiting_ack = true;
    ctx->ack_done = false;
    ctx->ack_code = 0xFF;

    if (ctx->hal.uart_send(ctx->cmd_buf, cmd_size) != 0) {
        ctx->waiting_ack = false;
        return -1;
    }

    return zw101_wait_ack(ctx, ctx->ack_cmd, ZW101_CAPTURE_TIMEOUT, NULL);
}

int zw101_cmd_general_extract(zw101_context_t *ctx, uint8_t buffer_id)
{
    if (zw101_send_variable_cmd(ctx, g_zw101_variable_cmd_general_extract,
        sizeof(g_zw101_variable_cmd_general_extract), &buffer_id, 1) != 0) {
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_GEN_EXTRACT, ZW101_COMMON_TIMEOUT, NULL);
}

int zw101_cmd_general_template(zw101_context_t *ctx)
{
    if ((ctx == NULL) || (ctx->hal.uart_send == NULL)) {
        return -1;
    }

    if (memcpy_s(ctx->cmd_buf, sizeof(ctx->cmd_buf), g_zw101_fixed_cmd_general_template,
        sizeof(g_zw101_fixed_cmd_general_template)) != EOK) {
        return -1;
    }

    ctx->ack_cmd = ZW101_CMD_GEN_TEMPLATE;
    ctx->waiting_ack = true;
    ctx->ack_done = false;
    ctx->ack_code = 0xFF;

    if (ctx->hal.uart_send(ctx->cmd_buf, sizeof(g_zw101_fixed_cmd_general_template)) != 0) {
        ctx->waiting_ack = false;
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_GEN_TEMPLATE, ZW101_COMMON_TIMEOUT, NULL);
}

int zw101_cmd_store_template(zw101_context_t *ctx, uint8_t buffer_id, uint16_t page_id)
{
    uint8_t var_data[3];

    var_data[0] = buffer_id;
    var_data[1] = (uint8_t)(page_id >> 8);
    var_data[2] = (uint8_t)page_id;

    if (zw101_send_variable_cmd(ctx, g_zw101_variable_cmd_store_template,
        sizeof(g_zw101_variable_cmd_store_template), var_data, sizeof(var_data)) != 0) {
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_STORE_TEMPLATE, ZW101_COMMON_TIMEOUT, NULL);
}

int zw101_cmd_match1n(zw101_context_t *ctx, uint8_t buffer_id, uint16_t start_page, uint16_t page_num)
{
    uint8_t var_data[5];

    var_data[0] = buffer_id;
    var_data[1] = (uint8_t)(start_page >> 8);
    var_data[2] = (uint8_t)start_page;
    var_data[3] = (uint8_t)(page_num >> 8);
    var_data[4] = (uint8_t)page_num;

    if (zw101_send_variable_cmd(ctx, g_zw101_variable_cmd_search_template,
        sizeof(g_zw101_variable_cmd_search_template), var_data, sizeof(var_data)) != 0) {
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_SEARCH_TEMPLATE, ZW101_MATCH_TIMEOUT, NULL);
}

int zw101_cmd_del_template(zw101_context_t *ctx, uint16_t template_id, uint16_t template_nums)
{
    uint8_t var_data[4];

    var_data[0] = (uint8_t)(template_id >> 8);
    var_data[1] = (uint8_t)template_id;
    var_data[2] = (uint8_t)(template_nums >> 8);
    var_data[3] = (uint8_t)template_nums;

    if (zw101_send_variable_cmd(ctx, g_zw101_variable_cmd_del_template,
        sizeof(g_zw101_variable_cmd_del_template), var_data, sizeof(var_data)) != 0) {
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_DEL_TEMPLATE, ZW101_COMMON_TIMEOUT, NULL);
}

int zw101_cmd_empty_template(zw101_context_t *ctx)
{
    if ((ctx == NULL) || (ctx->hal.uart_send == NULL)) {
        return -1;
    }

    if (memcpy_s(ctx->cmd_buf, sizeof(ctx->cmd_buf), g_zw101_fixed_cmd_empty_template,
        sizeof(g_zw101_fixed_cmd_empty_template)) != EOK) {
        return -1;
    }

    ctx->ack_cmd = ZW101_CMD_EMPTY_TEMPLATE;
    ctx->waiting_ack = true;
    ctx->ack_done = false;
    ctx->ack_code = 0xFF;

    if (ctx->hal.uart_send(ctx->cmd_buf, sizeof(g_zw101_fixed_cmd_empty_template)) != 0) {
        ctx->waiting_ack = false;
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_EMPTY_TEMPLATE, ZW101_EMPTY_ACK_TIMEOUT, NULL);
}

int zw101_cmd_get_id_availability(zw101_context_t *ctx, uint8_t index)
{
    if (zw101_send_variable_cmd(ctx, g_zw101_variable_cmd_read_index_table,
        sizeof(g_zw101_variable_cmd_read_index_table), &index, 1) != 0) {
        return -1;
    }

    return zw101_wait_ack(ctx, ZW101_CMD_READ_TEMPLATE_INDEX_TABLE, ZW101_COMMON_TIMEOUT, NULL);
}
