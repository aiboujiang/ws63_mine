#include "LD2402.h"

#include <string.h>

#include "securec.h"

typedef enum {
    STATE_IDLE = 0,
    STATE_HEADER,
    STATE_LEN_L,
    STATE_LEN_H,
    STATE_PAYLOAD,
    STATE_TAIL
} LD2402_RxState_t;

#define LD2402_FRAME_TYPE_CMD  0
#define LD2402_FRAME_TYPE_DATA 1

#define LD2402_HEADER_LEN 4
#define LD2402_LEN_FIELD_LEN 2
#define LD2402_TAIL_LEN 4
#define LD2402_FIXED_OVERHEAD (LD2402_HEADER_LEN + LD2402_LEN_FIELD_LEN + LD2402_TAIL_LEN)

#define LD2402_ACK_FRAME_MIN_DATA_LEN 4
#define LD2402_DATA_FRAME_MIN_DATA_LEN 3

static const uint8_t g_ld2402_cmd_header[LD2402_HEADER_LEN] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t g_ld2402_cmd_tail[LD2402_TAIL_LEN] = {0x04, 0x03, 0x02, 0x01};
static const uint8_t g_ld2402_data_header[LD2402_HEADER_LEN] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t g_ld2402_data_tail[LD2402_TAIL_LEN] = {0xF8, 0xF7, 0xF6, 0xF5};

static void WriteUint16_LE(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static void WriteUint32_LE(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static uint16_t ReadUint16_LE(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t ReadUint32_LE(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static void LD2402_ResetRxState(LD2402_Handle_t *h)
{
    h->state = STATE_IDLE;
    h->rx_index = 0;
    h->frame_len = 0;
    h->frame_type = LD2402_FRAME_TYPE_CMD;
}

static bool LD2402_CheckHeader(const uint8_t *src, uint8_t frame_type)
{
    const uint8_t *expect_header;

    expect_header = (frame_type == LD2402_FRAME_TYPE_CMD) ? g_ld2402_cmd_header : g_ld2402_data_header;
    return (memcmp(src, expect_header, LD2402_HEADER_LEN) == 0);
}

static bool LD2402_CheckTailByte(uint8_t frame_type, uint8_t tail_idx, uint8_t byte)
{
    const uint8_t *expect_tail;

    if (tail_idx >= LD2402_TAIL_LEN) {
        return false;
    }

    expect_tail = (frame_type == LD2402_FRAME_TYPE_CMD) ? g_ld2402_cmd_tail : g_ld2402_data_tail;
    return (expect_tail[tail_idx] == byte);
}

static uint16_t LD2402_GetExpectAckCmd(uint16_t cmd)
{
    return (uint16_t)(cmd + LD2402_ACK_CMD_OFFSET);
}

static int LD2402_SendFrame(LD2402_Handle_t *h, uint16_t cmd, const uint8_t *val, uint16_t v_len)
{
    uint8_t frame[LD2402_FRAME_BUFFER_SIZE] = {0};
    uint16_t pos = 0;
    uint16_t data_len;

    if ((h == NULL) || (h->hal.uart_send == NULL)) {
        return -1;
    }

    data_len = (uint16_t)(2 + v_len);
    if ((uint32_t)data_len + LD2402_FIXED_OVERHEAD > LD2402_FRAME_BUFFER_SIZE) {
        return -1;
    }

    frame[pos++] = g_ld2402_cmd_header[0];
    frame[pos++] = g_ld2402_cmd_header[1];
    frame[pos++] = g_ld2402_cmd_header[2];
    frame[pos++] = g_ld2402_cmd_header[3];

    WriteUint16_LE(&frame[pos], data_len);
    pos += 2;

    WriteUint16_LE(&frame[pos], cmd);
    pos += 2;

    if ((v_len > 0) && (val != NULL)) {
        if (memcpy_s(&frame[pos], sizeof(frame) - pos, val, v_len) != EOK) {
            return -1;
        }
        pos = (uint16_t)(pos + v_len);
    }

    frame[pos++] = g_ld2402_cmd_tail[0];
    frame[pos++] = g_ld2402_cmd_tail[1];
    frame[pos++] = g_ld2402_cmd_tail[2];
    frame[pos++] = g_ld2402_cmd_tail[3];

    return h->hal.uart_send(frame, pos);
}

static void LD2402_UpdateAck(LD2402_Handle_t *h, uint16_t ack_cmd, uint16_t ack_status,
    const uint8_t *payload, uint16_t payload_len)
{
    uint16_t copy_len = payload_len;

    h->cmd_last_ack_cmd = ack_cmd;
    h->cmd_last_payload_len = 0;

    if ((payload != NULL) && (payload_len > 0)) {
        if (copy_len > LD2402_FRAME_BUFFER_SIZE) {
            copy_len = LD2402_FRAME_BUFFER_SIZE;
        }
        if (memcpy_s(h->cmd_last_payload, sizeof(h->cmd_last_payload), payload, copy_len) == EOK) {
            h->cmd_last_payload_len = copy_len;
        }
    }

    if (h->cmd_waiting && (ack_cmd == h->cmd_wait_expect_ack)) {
        h->cmd_wait_status = ack_status;
        h->cmd_done = true;
    }
}

static void LD2402_ParseAckFrame(LD2402_Handle_t *h)
{
    uint16_t ack_cmd;
    uint16_t ack_status;
    uint16_t payload_len;
    const uint8_t *payload_ptr;

    if (h->frame_len < LD2402_ACK_FRAME_MIN_DATA_LEN) {
        return;
    }

    ack_cmd = ReadUint16_LE(&h->rx_buffer[6]);
    ack_status = ReadUint16_LE(&h->rx_buffer[8]);
    payload_len = (uint16_t)(h->frame_len - LD2402_ACK_FRAME_MIN_DATA_LEN);
    payload_ptr = &h->rx_buffer[10];

    LD2402_UpdateAck(h, ack_cmd, ack_status, payload_ptr, payload_len);
}

static void LD2402_ParseDataFrame(LD2402_Handle_t *h)
{
    LD2402_DataFrame_t frame = {0};
    uint16_t energy_bytes;
    uint16_t gate_count;
    uint16_t idx;

    if ((h->on_data_received == NULL) || (h->frame_len < LD2402_DATA_FRAME_MIN_DATA_LEN)) {
        return;
    }

    frame.status = (LD2402_Status_t)h->rx_buffer[6];
    frame.distance_cm = ReadUint16_LE(&h->rx_buffer[7]);

    energy_bytes = (uint16_t)(h->frame_len - LD2402_DATA_FRAME_MIN_DATA_LEN);
    gate_count = (uint16_t)(energy_bytes / 4);
    if (gate_count > LD2402_MAX_GATES) {
        gate_count = LD2402_MAX_GATES;
    }

    for (idx = 0; idx < gate_count; idx++) {
        frame.move_energy[idx] = (int32_t)ReadUint32_LE(&h->rx_buffer[9 + idx * 4]);
    }

    h->on_data_received(&frame);
}

static int LD2402_WaitAck(LD2402_Handle_t *h)
{
    uint32_t start_ms;

    if ((h == NULL) || (h->hal.get_tick_ms == NULL) || (h->hal.delay_ms == NULL)) {
        return -1;
    }

    start_ms = h->hal.get_tick_ms();
    while (!h->cmd_done) {
        if ((uint32_t)(h->hal.get_tick_ms() - start_ms) > LD2402_CMD_TIMEOUT_MS) {
            h->cmd_waiting = false;
            return -1;
        }
        h->hal.delay_ms(1);
    }

    h->cmd_waiting = false;
    return (h->cmd_wait_status == LD2402_ACK_STATUS_OK) ? 0 : -1;
}

void LD2402_InputByte(LD2402_Handle_t *h, uint8_t byte)
{
    uint8_t tail_idx;

    if (h == NULL) {
        return;
    }

    switch (h->state) {
        case STATE_IDLE:
            if ((byte == g_ld2402_cmd_header[0]) || (byte == g_ld2402_data_header[0])) {
                h->frame_type = (byte == g_ld2402_cmd_header[0]) ? LD2402_FRAME_TYPE_CMD : LD2402_FRAME_TYPE_DATA;
                h->rx_buffer[0] = byte;
                h->rx_index = 1;
                h->state = STATE_HEADER;
            }
            break;

        case STATE_HEADER:
            if (h->rx_index >= LD2402_FRAME_BUFFER_SIZE) {
                LD2402_ResetRxState(h);
                break;
            }

            h->rx_buffer[h->rx_index++] = byte;
            if (h->rx_index == LD2402_HEADER_LEN) {
                if (!LD2402_CheckHeader(h->rx_buffer, h->frame_type)) {
                    LD2402_ResetRxState(h);
                    break;
                }
                h->state = STATE_LEN_L;
            }
            break;

        case STATE_LEN_L:
            if (h->rx_index >= LD2402_FRAME_BUFFER_SIZE) {
                LD2402_ResetRxState(h);
                break;
            }

            h->rx_buffer[h->rx_index++] = byte;
            h->state = STATE_LEN_H;
            break;

        case STATE_LEN_H:
            if (h->rx_index >= LD2402_FRAME_BUFFER_SIZE) {
                LD2402_ResetRxState(h);
                break;
            }

            h->rx_buffer[h->rx_index++] = byte;
            h->frame_len = ReadUint16_LE(&h->rx_buffer[4]);

            if ((uint32_t)h->frame_len + LD2402_FIXED_OVERHEAD > LD2402_FRAME_BUFFER_SIZE) {
                LD2402_ResetRxState(h);
                break;
            }

            if (h->frame_len == 0) {
                h->state = STATE_TAIL;
            } else {
                h->state = STATE_PAYLOAD;
            }
            break;

        case STATE_PAYLOAD:
            if (h->rx_index >= LD2402_FRAME_BUFFER_SIZE) {
                LD2402_ResetRxState(h);
                break;
            }

            h->rx_buffer[h->rx_index++] = byte;
            if (h->rx_index >= (uint16_t)(LD2402_HEADER_LEN + LD2402_LEN_FIELD_LEN + h->frame_len)) {
                h->state = STATE_TAIL;
            }
            break;

        case STATE_TAIL:
            if (h->rx_index >= LD2402_FRAME_BUFFER_SIZE) {
                LD2402_ResetRxState(h);
                break;
            }

            h->rx_buffer[h->rx_index++] = byte;
            tail_idx = (uint8_t)(h->rx_index - (LD2402_HEADER_LEN + LD2402_LEN_FIELD_LEN + h->frame_len) - 1);
            if (!LD2402_CheckTailByte(h->frame_type, tail_idx, byte)) {
                LD2402_ResetRxState(h);
                break;
            }

            if (tail_idx == (LD2402_TAIL_LEN - 1)) {
                if (h->frame_type == LD2402_FRAME_TYPE_CMD) {
                    LD2402_ParseAckFrame(h);
                } else {
                    LD2402_ParseDataFrame(h);
                }
                LD2402_ResetRxState(h);
            }
            break;

        default:
            LD2402_ResetRxState(h);
            break;
    }
}

void LD2402_Init(LD2402_Handle_t *h, LD2402_HAL_t *hal)
{
    if ((h == NULL) || (hal == NULL)) {
        return;
    }

    if (memset_s(h, sizeof(LD2402_Handle_t), 0, sizeof(LD2402_Handle_t)) != EOK) {
        return;
    }

    h->hal = *hal;
    h->state = STATE_IDLE;
    h->is_in_config_mode = false;
}

int LD2402_SendCommand(LD2402_Handle_t *h, uint16_t cmd, uint8_t *val, uint16_t v_len)
{
    if (h == NULL) {
        return -1;
    }

    h->cmd_waiting = true;
    h->cmd_done = false;
    h->cmd_wait_status = 0xFFFF;
    h->cmd_wait_expect_ack = LD2402_GetExpectAckCmd(cmd);
    h->cmd_last_ack_cmd = 0;
    h->cmd_last_payload_len = 0;

    if (LD2402_SendFrame(h, cmd, val, v_len) != 0) {
        h->cmd_waiting = false;
        return -1;
    }

    return LD2402_WaitAck(h);
}

void LD2402_EnterConfigMode(LD2402_Handle_t *h)
{
    uint8_t val[2] = {0x01, 0x00};

    if ((h == NULL) || h->is_in_config_mode) {
        return;
    }

    if (LD2402_SendCommand(h, LD2402_CMD_ENABLE_CONFIG, val, sizeof(val)) == 0) {
        h->is_in_config_mode = true;
    }
}

void LD2402_ExitConfigMode(LD2402_Handle_t *h)
{
    if ((h == NULL) || (!h->is_in_config_mode)) {
        return;
    }

    if (LD2402_SendCommand(h, LD2402_CMD_END_CONFIG, NULL, 0) == 0) {
        h->is_in_config_mode = false;
    }
}

static bool LD2402_IsAutoThresholdCoefValid(uint16_t coef_10x)
{
    return ((coef_10x >= LD2402_AUTO_THRESHOLD_COEF_MIN) &&
        (coef_10x <= LD2402_AUTO_THRESHOLD_COEF_MAX));
}

int LD2402_SetParam(LD2402_Handle_t *h, uint16_t param_id, uint32_t value)
{
    uint8_t cmd_data[6] = {0};
    int ret;

    if (h == NULL) {
        return -1;
    }

    WriteUint16_LE(&cmd_data[0], param_id);
    WriteUint32_LE(&cmd_data[2], value);

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_SET_PARAM, cmd_data, sizeof(cmd_data));
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_ReadParam(LD2402_Handle_t *h, uint16_t param_id, uint32_t *value)
{
    uint8_t cmd_data[2] = {0};
    int ret;

    if ((h == NULL) || (value == NULL)) {
        return -1;
    }

    WriteUint16_LE(&cmd_data[0], param_id);

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_GET_PARAM, cmd_data, sizeof(cmd_data));
    if ((ret == 0) && (h->cmd_last_payload_len >= 4)) {
        *value = ReadUint32_LE(h->cmd_last_payload);
    } else {
        ret = -1;
    }

    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_SetMaxDistance(LD2402_Handle_t *h, float distance_m)
{
    uint16_t value;

    value = (uint16_t)(distance_m * 10.0f);
    if (value < 7) {
        value = 7;
    }
    if (value > 100) {
        value = 100;
    }

    return LD2402_SetParam(h, LD2402_PARAM_MAX_DIST, value);
}

int LD2402_SetDisappearDelay(LD2402_Handle_t *h, uint16_t seconds)
{
    return LD2402_SetParam(h, LD2402_PARAM_DELAY_TIME, (uint32_t)seconds);
}

int LD2402_SetEngineeringMode(LD2402_Handle_t *h)
{
    uint8_t cmd_data[6] = {0};
    int ret;

    if (h == NULL) {
        return -1;
    }

    WriteUint16_LE(&cmd_data[0], 0x0000);
    WriteUint32_LE(&cmd_data[2], LD2402_MODE_ENGINEERING);

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_OUTPUT_MODE, cmd_data, sizeof(cmd_data));
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_SetNormalMode(LD2402_Handle_t *h)
{
    uint8_t cmd_data[6] = {0};
    int ret;

    if (h == NULL) {
        return -1;
    }

    WriteUint16_LE(&cmd_data[0], 0x0000);
    WriteUint32_LE(&cmd_data[2], LD2402_MODE_NORMAL);

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_OUTPUT_MODE, cmd_data, sizeof(cmd_data));
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_SaveParams(LD2402_Handle_t *h)
{
    int ret;

    if (h == NULL) {
        return -1;
    }

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_SAVE_PARAM, NULL, 0);
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_AutoGainAdjust(LD2402_Handle_t *h)
{
    int ret;

    if (h == NULL) {
        return -1;
    }

    /*
     * 手册要求除使能配置外的命令均在配置模式下执行，
     * 这里统一走 Enter/Exit，避免不同固件版本行为差异。
     */
    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_AUTO_GAIN, NULL, 0);
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_StartAutoThreshold(LD2402_Handle_t *h, uint16_t trig_coef_10x,
    uint16_t hold_coef_10x, uint16_t static_coef_10x)
{
    uint8_t cmd_data[6] = {0};
    int ret;

    if ((h == NULL) ||
        (!LD2402_IsAutoThresholdCoefValid(trig_coef_10x)) ||
        (!LD2402_IsAutoThresholdCoefValid(hold_coef_10x)) ||
        (!LD2402_IsAutoThresholdCoefValid(static_coef_10x))) {
        return -1;
    }

    WriteUint16_LE(&cmd_data[0], trig_coef_10x);
    WriteUint16_LE(&cmd_data[2], hold_coef_10x);
    WriteUint16_LE(&cmd_data[4], static_coef_10x);

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_AUTO_THRESHOLD, cmd_data, sizeof(cmd_data));
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_GetAutoThresholdProgress(LD2402_Handle_t *h, uint16_t *progress_percent)
{
    int ret;

    if ((h == NULL) || (progress_percent == NULL)) {
        return -1;
    }

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_AUTO_THRESHOLD_PROGRESS, NULL, 0);
    if ((ret == 0) && (h->cmd_last_payload_len >= 2)) {
        *progress_percent = ReadUint16_LE(h->cmd_last_payload);
    } else {
        ret = -1;
    }

    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_GetAutoThresholdAlarm(LD2402_Handle_t *h, LD2402_AutoThresholdAlarm_t *alarm)
{
    int ret;

    if ((h == NULL) || (alarm == NULL)) {
        return -1;
    }

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    ret = LD2402_SendCommand(h, LD2402_CMD_AUTO_THRESHOLD_ALARM, NULL, 0);
    if ((ret == 0) && (h->cmd_last_payload_len >= 4)) {
        alarm->alarm_status = ReadUint16_LE(&h->cmd_last_payload[0]);
        alarm->gate_bitmap = ReadUint16_LE(&h->cmd_last_payload[2]);
    } else {
        ret = -1;
    }

    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_GetPowerInterference(LD2402_Handle_t *h, uint32_t *value)
{
    return LD2402_ReadParam(h, LD2402_PARAM_POWER_INTER, value);
}

int LD2402_RefreshSaveFlag(LD2402_Handle_t *h)
{
    uint32_t save_flag = 0;

    if (LD2402_ReadParam(h, LD2402_PARAM_SAVE_FLAG, &save_flag) != 0) {
        return -1;
    }

    return LD2402_SetParam(h, LD2402_PARAM_SAVE_FLAG, save_flag);
}

int LD2402_GetVersion(LD2402_Handle_t *h, char *buf, uint16_t buf_len)
{
    uint16_t ver_len;
    int ret = -1;

    if ((h == NULL) || (buf == NULL) || (buf_len < 2)) {
        return -1;
    }

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    if (LD2402_SendCommand(h, LD2402_CMD_VERSION, NULL, 0) != 0) {
        goto exit;
    }

    if (h->cmd_last_payload_len < 2) {
        goto exit;
    }

    ver_len = ReadUint16_LE(h->cmd_last_payload);
    if ((ver_len == 0) || ((uint16_t)(2 + ver_len) > h->cmd_last_payload_len) || (ver_len >= buf_len)) {
        goto exit;
    }

    if (memcpy_s(buf, buf_len, &h->cmd_last_payload[2], ver_len) != EOK) {
        goto exit;
    }

    buf[ver_len] = '\0';
    ret = 0;

exit:
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_GetSN_Hex(LD2402_Handle_t *h, uint8_t *buf, uint16_t buf_len)
{
    uint16_t sn_len;
    int ret = -1;

    if ((h == NULL) || (buf == NULL) || (buf_len == 0)) {
        return -1;
    }

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    if (LD2402_SendCommand(h, LD2402_CMD_READ_SN_HEX, NULL, 0) != 0) {
        goto exit;
    }

    if (h->cmd_last_payload_len < 2) {
        goto exit;
    }

    sn_len = ReadUint16_LE(h->cmd_last_payload);
    if ((sn_len == 0) || ((uint16_t)(2 + sn_len) > h->cmd_last_payload_len) || (sn_len > buf_len)) {
        goto exit;
    }

    if (memcpy_s(buf, buf_len, &h->cmd_last_payload[2], sn_len) != EOK) {
        goto exit;
    }

    ret = (int)sn_len;

exit:
    LD2402_ExitConfigMode(h);
    return ret;
}

int LD2402_GetSN_Char(LD2402_Handle_t *h, char *buf, uint16_t buf_len)
{
    uint16_t sn_len;
    int ret = -1;

    if ((h == NULL) || (buf == NULL) || (buf_len < 2)) {
        return -1;
    }

    LD2402_EnterConfigMode(h);
    if (!h->is_in_config_mode) {
        return -1;
    }

    if (LD2402_SendCommand(h, LD2402_CMD_READ_SN_CHAR, NULL, 0) != 0) {
        goto exit;
    }

    if (h->cmd_last_payload_len < 2) {
        goto exit;
    }

    sn_len = ReadUint16_LE(h->cmd_last_payload);
    if ((sn_len == 0) || ((uint16_t)(2 + sn_len) > h->cmd_last_payload_len) || (sn_len >= buf_len)) {
        goto exit;
    }

    if (memcpy_s(buf, buf_len, &h->cmd_last_payload[2], sn_len) != EOK) {
        goto exit;
    }

    buf[sn_len] = '\0';
    ret = 0;

exit:
    LD2402_ExitConfigMode(h);
    return ret;
}