/**
 * @file ld2402.c
 * @brief LD2402 雷达模块驱动逻辑。
 *
 * 根据《HLK-LD2402用户手册 V1.08》:
 * - 波特率 115200
 * - 使能配置命令 (0x00FF), 结束配置命令 (0x00FE) 来确认通信正常。
 */
#include "ld2402.h"
#include "wk2114.h"
#include "ws63_final_bsp.h"
#include "ws63_final_config.h"
#include "osal_debug.h"
#include "securec.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* LD2402 命令/数据帧公共格式参数。 */
#define LD2402_HEADER_LEN                 4U
#define LD2402_LEN_FIELD_LEN              2U
#define LD2402_TAIL_LEN                   4U
#define LD2402_FRAME_FIXED_OVERHEAD       (LD2402_HEADER_LEN + LD2402_LEN_FIELD_LEN + LD2402_TAIL_LEN)
#define LD2402_FRAME_BUFFER_SIZE          256U
#define LD2402_CMD_TIMEOUT_MS             1000U
#define LD2402_ACK_CMD_OFFSET             0x0100U
#define LD2402_ACK_STATUS_OK              0x0000U
#define LD2402_CMD_VERSION                0x0000U
#define LD2402_CMD_SET_PARAM              0x0007U
#define LD2402_CMD_GET_PARAM              0x0008U
#define LD2402_CMD_AUTO_THRESHOLD         0x0009U
#define LD2402_CMD_AUTO_THRESHOLD_PROGRESS 0x000AU
#define LD2402_CMD_READ_SN_CHAR           0x0011U
#define LD2402_CMD_OUTPUT_MODE            0x0012U
#define LD2402_CMD_AUTO_THRESHOLD_ALARM   0x0014U
#define LD2402_CMD_READ_SN_HEX            0x0016U
#define LD2402_CMD_AUTO_GAIN              0x00EEU
#define LD2402_CMD_SAVE_PARAM             0x00FDU
#define LD2402_CMD_END_CONFIG             0x00FEU
#define LD2402_CMD_ENABLE_CONFIG          0x00FFU
#define LD2402_PARAM_MAX_DIST             0x0001U
#define LD2402_PARAM_DELAY_TIME           0x0004U
#define LD2402_PARAM_POWER_INTER          0x0005U
#define LD2402_PARAM_SAVE_FLAG            0x003FU

/* 手册定义的固定帧头/帧尾。 */
static const uint8_t g_ld2402_cmd_header[LD2402_HEADER_LEN] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t g_ld2402_data_header[LD2402_HEADER_LEN] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t g_ld2402_cmd_tail[LD2402_TAIL_LEN] = {0x04, 0x03, 0x02, 0x01};
static const uint8_t g_ld2402_data_tail[LD2402_TAIL_LEN] = {0xF8, 0xF7, 0xF6, 0xF5};
#define LD2402_ACK_EXPECT_CMD_ENABLE      0x01FFU
#define LD2402_ACK_EXPECT_STATUS_OK       0x0000U

/* 初始化阶段的接收窗口参数：分段轮询，兼容“ACK 与数据帧交织/分段到达”的场景。 */
#define LD2402_INIT_RETRY_MAX             3U
#define LD2402_INIT_POLL_ROUND_MAX        8U
#define LD2402_INIT_POLL_GAP_MS           20U
/* 清空接收缓存时的最大轮询次数，防止 RFCNT 读回异常导致死循环。 */
#define LD2402_DRAIN_MAX_ROUND            32U

// 使能配置命令: 帧头(4)+帧内长(2)+字(2)+值(2)+帧尾(4)
static const uint8_t g_ld2402_cmd_enable[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 
    0x04, 0x00, 
    0xFF, 0x00, 
    0x01, 0x00, 
    0x04, 0x03, 0x02, 0x01
};

// 结束配置命令: 帧头(4)+帧内长(2)+字(2)+帧尾(4)
static const uint8_t g_ld2402_cmd_disable[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 
    0x02, 0x00, 
    0xFE, 0x00, 
    0x04, 0x03, 0x02, 0x01
};

/* 运行态日志控制：默认“开启但按间隔节流”，避免上电后高频刷屏。 */
static uint8_t g_ld2402_data_log_enable = WS63_LD2402_DATA_LOG_ENABLE_DEFAULT;
static uint32_t g_ld2402_data_log_gap_ms = WS63_LD2402_DATA_LOG_GAP_MS_DEFAULT;
static uint32_t g_ld2402_data_last_log_ms = 0U;
/* 最近一次解析到的 distance:xxx 输出，供门锁编排层轮询使用。 */
static int32_t g_ld2402_last_distance_mm = -1;
static uint32_t g_ld2402_last_distance_tick_ms = 0U;
/* 当前绑定的子串口与协议模式状态，供调试命令与运行态共用。 */
static uint8_t g_ld2402_sub_port = 0U;
static uint8_t g_ld2402_ready = 0U;
static uint8_t g_ld2402_in_config_mode = 0U;
static int32_t g_ld2402_last_status = -1;

/* 协议帧与重试时序参数。 */
#define LD2402_PROTOCOL_FRAME_MAX        LD2402_FRAME_BUFFER_SIZE
#define LD2402_PROTOCOL_INIT_RETRY_MAX   3U
#define LD2402_PROTOCOL_POLL_ROUND_MAX   50U
#define LD2402_PROTOCOL_POLL_GAP_MS      20U
#define LD2402_PROTOCOL_DRAIN_ROUND_MAX  32U
#define LD2402_PROTOCOL_DRAIN_GAP_MS     2U

/* 5.2.8 输出模式参数值。 */
#define LD2402_PROTOCOL_MODE_ENGINEERING 0x00000004U
#define LD2402_PROTOCOL_MODE_NORMAL      0x00000064U

/* 5.2.9 自动门限生成系数范围。 */
#define LD2402_PROTOCOL_AUTO_COEF_MIN    10U
#define LD2402_PROTOCOL_AUTO_COEF_MAX    200U

/* 工程模式下的能量门个数。 */
#define LD2402_PROTOCOL_ENGINEERING_MOVE_GATES   16U
#define LD2402_PROTOCOL_ENGINEERING_STATIC_GATES 16U

/**
 * @brief 将 16 位数值按小端写入缓冲区。
 */
static void ld2402_write_uint16_le(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
}

/**
 * @brief 将 32 位数值按小端写入缓冲区。
 */
static void ld2402_write_uint32_le(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value & 0xFFU);
    buf[1] = (uint8_t)((value >> 8) & 0xFFU);
    buf[2] = (uint8_t)((value >> 16) & 0xFFU);
    buf[3] = (uint8_t)((value >> 24) & 0xFFU);
}

/**
 * @brief 从小端字节流读取 16 位数值。
 */
static uint16_t ld2402_read_uint16_le(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * @brief 从小端字节流读取 32 位数值。
 */
static uint32_t ld2402_read_uint32_le(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/**
 * @brief 判断一段文本中是否包含 OFF 关键字。
 */
static bool ld2402_parse_off_text(const uint8_t *data, uint16_t len)
{
    const char *off_ptr;
    char text_buf[32] = {0};
    uint16_t copy_len;

    if ((data == NULL) || (len < 3U)) {
        return false;
    }

    copy_len = (len < (uint16_t)(sizeof(text_buf) - 1U)) ? len : (uint16_t)(sizeof(text_buf) - 1U);
    (void)memcpy(text_buf, data, copy_len);
    text_buf[copy_len] = '\0';
    off_ptr = strstr(text_buf, "OFF");
    return (off_ptr != NULL);
}

/**
 * @brief 判断缓冲区前 4 字节是否为命令帧头。
 */
static bool ld2402_is_cmd_header(const uint8_t *data)
{
    return (memcmp(data, g_ld2402_cmd_header, LD2402_HEADER_LEN) == 0);
}

/**
 * @brief 判断缓冲区前 4 字节是否为数据帧头。
 */
static bool ld2402_is_data_header(const uint8_t *data)
{
    return (memcmp(data, g_ld2402_data_header, LD2402_HEADER_LEN) == 0);
}

/**
 * @brief 判断帧尾是否匹配。
 */
static bool ld2402_tail_match(bool is_cmd, const uint8_t *data, uint16_t tail_pos)
{
    const uint8_t *expect_tail = is_cmd ? g_ld2402_cmd_tail : g_ld2402_data_tail;

    return (data[tail_pos] == expect_tail[0]) && (data[tail_pos + 1U] == expect_tail[1]) &&
        (data[tail_pos + 2U] == expect_tail[2]) && (data[tail_pos + 3U] == expect_tail[3]);
}

/**
 * @brief 检查一段数据里是否包含完整数据帧，并据此刷新最近距离值。
 *
 * 说明：运行态可能收到文本帧，也可能收到工程模式二进制帧；这里优先把
 * 可确认的二进制距离值折算为毫米，保持门锁侧的距离语义稳定。
 */
static bool ld2402_update_from_data_frames(const uint8_t *data, uint16_t len)
{
    uint16_t pos;
    bool updated = false;

    if ((data == NULL) || (len < LD2402_FRAME_FIXED_OVERHEAD)) {
        return false;
    }

    for (pos = 0U; pos + LD2402_FRAME_FIXED_OVERHEAD <= len; pos++) {
        uint16_t payload_len;
        uint16_t frame_len;
        uint16_t tail_pos;

        if (!ld2402_is_data_header(&data[pos])) {
            continue;
        }

        payload_len = ld2402_read_uint16_le(&data[pos + 4U]);
        frame_len = (uint16_t)(LD2402_FRAME_FIXED_OVERHEAD + payload_len);
        if ((frame_len < LD2402_FRAME_FIXED_OVERHEAD) || ((uint32_t)pos + frame_len > (uint32_t)len)) {
            continue;
        }

        tail_pos = (uint16_t)(pos + frame_len - LD2402_TAIL_LEN);
        if (!ld2402_tail_match(false, data, tail_pos)) {
            continue;
        }

        /* 数据帧至少包含：状态 + 距离；状态为 0 时视为无人。 */
        if (payload_len < 3U) {
            continue;
        }

        g_ld2402_last_status = (int32_t)data[pos + 6U];
        if (data[pos + 6U] == 0U) {
            g_ld2402_last_distance_mm = -1;
        } else {
            g_ld2402_last_distance_mm = (int32_t)ld2402_read_uint16_le(&data[pos + 7U]) * 10;
        }
        g_ld2402_last_distance_tick_ms = ws63_bsp_get_tick_ms();
        updated = true;
    }

    return updated;
}

/**
 * @brief 在累计缓存中查找目标 ACK 并回填 payload。
 */
static bool ld2402_find_ack_in_buffer(const uint8_t *data, uint16_t len, uint16_t expect_ack_cmd,
    uint16_t *ack_status_out, const uint8_t **payload_out, uint16_t *payload_len_out)
{
    uint16_t pos;

    if ((data == NULL) || (len < LD2402_FRAME_FIXED_OVERHEAD) || (ack_status_out == NULL) ||
        (payload_out == NULL) || (payload_len_out == NULL)) {
        return false;
    }

    for (pos = 0U; pos + LD2402_FRAME_FIXED_OVERHEAD <= len; pos++) {
        bool is_cmd;
        bool is_data;
        uint16_t payload_len;
        uint16_t frame_len;
        uint16_t tail_pos;

        is_cmd = ld2402_is_cmd_header(&data[pos]);
        is_data = ld2402_is_data_header(&data[pos]);
        if ((!is_cmd) && (!is_data)) {
            continue;
        }

        payload_len = ld2402_read_uint16_le(&data[pos + 4U]);
        frame_len = (uint16_t)(LD2402_FRAME_FIXED_OVERHEAD + payload_len);
        if ((frame_len < LD2402_FRAME_FIXED_OVERHEAD) || ((uint32_t)pos + frame_len > (uint32_t)len)) {
            continue;
        }

        tail_pos = (uint16_t)(pos + frame_len - LD2402_TAIL_LEN);
        if (!ld2402_tail_match(is_cmd, data, tail_pos)) {
            continue;
        }

        if (is_data) {
            continue;
        }

        if (payload_len < 4U) {
            continue;
        }

        if (ld2402_read_uint16_le(&data[pos + 6U]) != expect_ack_cmd) {
            continue;
        }

        *ack_status_out = ld2402_read_uint16_le(&data[pos + 8U]);
        *payload_out = &data[pos + 10U];
        *payload_len_out = (uint16_t)(payload_len - 4U);
        return true;
    }

    return false;
}

/**
 * @brief 清空子串口接收缓存，避免历史帧干扰当前协议命令。
 */
static void ld2402_drain_rx(uint8_t sub_port)
{
    uint8_t rx_buf[LD2402_PROTOCOL_FRAME_MAX] = {0};
    uint8_t round;
    uint8_t read_max_len;

    read_max_len = (uint8_t)((sizeof(rx_buf) > 0xFFU) ? 0xFFU : sizeof(rx_buf));

    for (round = 0U; round < LD2402_PROTOCOL_DRAIN_ROUND_MAX; round++) {
        if (wk2114_subport_read(sub_port, rx_buf, read_max_len) == 0U) {
            break;
        }
        ws63_bsp_sleep_ms(LD2402_PROTOCOL_DRAIN_GAP_MS);
    }
}

/**
 * @brief 发送命令并同步等待 ACK。
 *
 * @param cmd         命令字。
 * @param value       命令参数。
 * @param value_len   命令参数长度。
 * @param ack_payload ACK 返回值缓存，可为空。
 * @param ack_len_out ACK 返回值长度，可为空。
 * @param ack_status_out ACK 状态码，可为空。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
static errcode_t ld2402_send_command_wait_ack(uint16_t cmd, const uint8_t *value, uint16_t value_len,
    uint8_t *ack_payload, uint16_t ack_payload_len, uint16_t *ack_len_out, uint16_t *ack_status_out)
{
    uint8_t frame[LD2402_PROTOCOL_FRAME_MAX] = {0};
    uint8_t rx_buf[LD2402_PROTOCOL_FRAME_MAX] = {0};
    uint16_t frame_len = 0U;
    uint16_t rx_total = 0U;
    uint32_t start_ms;
    uint16_t expect_ack_cmd;
    uint16_t ack_status = LD2402_ACK_EXPECT_STATUS_OK;
    const uint8_t *payload_ptr = NULL;
    uint16_t payload_len = 0U;
    uint16_t remaining;
    uint8_t read_len;
    uint8_t round;
    bool ack_found = false;

    if (g_ld2402_sub_port == 0U) {
        return ERRCODE_FAIL;
    }

    if (value_len > 0U) {
        if ((value == NULL) || ((uint32_t)value_len + LD2402_FRAME_FIXED_OVERHEAD > sizeof(frame))) {
            return ERRCODE_INVALID_PARAM;
        }
    }

    frame[0] = g_ld2402_cmd_header[0];
    frame[1] = g_ld2402_cmd_header[1];
    frame[2] = g_ld2402_cmd_header[2];
    frame[3] = g_ld2402_cmd_header[3];
    ld2402_write_uint16_le(&frame[4], (uint16_t)(2U + value_len));
    ld2402_write_uint16_le(&frame[6], cmd);
    if ((value != NULL) && (value_len > 0U)) {
        (void)memcpy_s(&frame[8], sizeof(frame) - 8U, value, value_len);
    }
    frame_len = (uint16_t)(8U + value_len);
    frame[frame_len++] = g_ld2402_cmd_tail[0];
    frame[frame_len++] = g_ld2402_cmd_tail[1];
    frame[frame_len++] = g_ld2402_cmd_tail[2];
    frame[frame_len++] = g_ld2402_cmd_tail[3];

    ld2402_drain_rx(g_ld2402_sub_port);
    if (wk2114_subport_write(g_ld2402_sub_port, frame, frame_len) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    expect_ack_cmd = (uint16_t)(cmd + LD2402_ACK_CMD_OFFSET);
    start_ms = ws63_bsp_get_tick_ms();
    for (round = 0U; round < LD2402_PROTOCOL_POLL_ROUND_MAX; round++) {
        remaining = (uint16_t)(sizeof(rx_buf) - rx_total);
        if (remaining > 0U) {
            read_len = wk2114_subport_read(g_ld2402_sub_port, &rx_buf[rx_total], (uint8_t)((remaining > 0xFFU) ? 0xFFU : remaining));
            if (read_len > 0U) {
                rx_total = (uint16_t)(rx_total + read_len);
                ld2402_update_from_data_frames(rx_buf, rx_total);
                ack_found = ld2402_find_ack_in_buffer(rx_buf, rx_total, expect_ack_cmd,
                    &ack_status, &payload_ptr, &payload_len);
                if (ack_found) {
                    if ((ack_payload != NULL) && (ack_payload_len > 0U) && (payload_len > 0U)) {
                        uint16_t copy_len = (payload_len < ack_payload_len) ? payload_len : ack_payload_len;
                        (void)memcpy_s(ack_payload, ack_payload_len, payload_ptr, copy_len);
                        if (ack_len_out != NULL) {
                            *ack_len_out = copy_len;
                        }
                    } else if (ack_len_out != NULL) {
                        *ack_len_out = 0U;
                    }
                    if (ack_status_out != NULL) {
                        *ack_status_out = ack_status;
                    }
                    return (ack_status == LD2402_ACK_STATUS_OK) ? ERRCODE_SUCC : ERRCODE_FAIL;
                }
            }
        }

        if ((uint32_t)(ws63_bsp_get_tick_ms() - start_ms) > LD2402_CMD_TIMEOUT_MS) {
            break;
        }

        ws63_bsp_sleep_ms(LD2402_PROTOCOL_POLL_GAP_MS);
    }

    if (ack_len_out != NULL) {
        *ack_len_out = 0U;
    }
    if (ack_status_out != NULL) {
        *ack_status_out = 0xFFFFU;
    }
    return ERRCODE_FAIL;
}

/**
 * @brief 进入配置模式，失败则保持原状态。
 */
static errcode_t ld2402_enter_config_mode(void)
{
    uint8_t enable_value[2] = {0x01U, 0x00U};
    uint16_t ack_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    errcode_t ret;

    if (g_ld2402_in_config_mode != 0U) {
        return ERRCODE_SUCC;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_ENABLE_CONFIG, enable_value, sizeof(enable_value),
        NULL, 0U, &ack_len, &ack_status);
    if (ret == ERRCODE_SUCC) {
        g_ld2402_in_config_mode = 1U;
    }
    return ret;
}

/**
 * @brief 尝试退出配置模式。
 */
static void ld2402_exit_config_mode(void)
{
    uint16_t ack_len = 0U;
    uint16_t ack_status = 0xFFFFU;

    if (g_ld2402_in_config_mode == 0U) {
        return;
    }

    if (ld2402_send_command_wait_ack(LD2402_CMD_END_CONFIG, NULL, 0U, NULL, 0U, &ack_len, &ack_status) == ERRCODE_SUCC) {
        g_ld2402_in_config_mode = 0U;
    }
}

/**
 * @brief 校验自动门限系数是否符合手册范围。
 */
static bool ld2402_auto_coef_valid(uint16_t coef_10x)
{
    return ((coef_10x >= LD2402_PROTOCOL_AUTO_COEF_MIN) && (coef_10x <= LD2402_PROTOCOL_AUTO_COEF_MAX));
}

/**
 * @brief 判断当前是否允许输出一条运行态观测日志。
 *
 * 说明：距离文本、OFF 文本和普通数据包都复用同一节流窗，避免不同分支
 * 各自打印后把串口重新刷满。
 */
static bool ld2402_can_print_processing_log(uint32_t now_ms)
{
    if (g_ld2402_data_log_enable == 0U) {
        return false;
    }

    if ((g_ld2402_data_log_gap_ms > 0U) &&
        ((uint32_t)(now_ms - g_ld2402_data_last_log_ms) < g_ld2402_data_log_gap_ms)) {
        return false;
    }

    g_ld2402_data_last_log_ms = now_ms;
    return true;
}

/**
 * @brief 从串口文本中提取 `distance:xxx` 数值。
 *
 * LD2402 这一路在门锁场景下只接受明确的距离字段，避免把其他调试文本
 * 误识别为有效接近事件。
 */
static bool ld2402_parse_distance_text(const uint8_t *data, uint16_t len, int32_t *distance_mm_out)
{
    const char prefix[] = "distance:";
    char text_buf[128] = {0};
    const char *value_ptr;
    char value_buf[16] = {0};
    size_t copy_len;
    size_t i;
    long distance_value;
    char *end_ptr = NULL;

    if ((data == NULL) || (distance_mm_out == NULL) || (len == 0U)) {
        return false;
    }

    copy_len = (len < (uint16_t)(sizeof(text_buf) - 1U)) ? (size_t)len : (sizeof(text_buf) - 1U);
    (void)memcpy(text_buf, data, copy_len);
    text_buf[copy_len] = '\0';

    value_ptr = strstr(text_buf, prefix);
    if (value_ptr == NULL) {
        return false;
    }

    value_ptr += (sizeof(prefix) - 1U);
    for (i = 0U; (i + 1U < sizeof(value_buf)) && (value_ptr[i] != '\0'); i++) {
        char ch = value_ptr[i];

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '\t')) {
            break;
        }
        if ((i == 0U) && ((ch == '+') || (ch == '-'))) {
            value_buf[i] = ch;
            continue;
        }
        if ((ch < '0') || (ch > '9')) {
            break;
        }
        value_buf[i] = ch;
    }

    value_buf[i] = '\0';

    if (value_buf[0] == '\0') {
        return false;
    }

    distance_value = strtol(value_buf, &end_ptr, 10);
    if ((end_ptr == value_buf) || (end_ptr == NULL) || (*end_ptr != '\0')) {
        return false;
    }

    *distance_mm_out = (int32_t)distance_value;
    return true;
}

/**
 * @brief 检查缓冲区中的某个完整 LD2402 帧。
 *
 * @param data 缓冲区。
 * @param len 缓冲区有效长度。
 * @param has_enable_ack_out 输出：是否命中“使能配置 ACK 成功”。
 * @return true  发现至少一个完整帧（命令帧或数据帧）。
 * @return false 未发现完整帧。
 */
static bool ld2402_find_valid_frame(const uint8_t *data, uint16_t len,
    bool *has_enable_ack_out)
{
    uint16_t pos;

    if ((data == NULL) || (len < LD2402_FRAME_FIXED_OVERHEAD) || (has_enable_ack_out == NULL)) {
        return false;
    }

    *has_enable_ack_out = false;

    for (pos = 0U; pos + LD2402_FRAME_FIXED_OVERHEAD <= len; pos++) {
        bool is_cmd_header;
        bool is_data_header;
        uint16_t payload_len;
        uint16_t frame_total_len;
        uint16_t tail_pos;
        const uint8_t *expect_tail;

        is_cmd_header = (data[pos] == 0xFDU) && (data[pos + 1U] == 0xFCU) &&
            (data[pos + 2U] == 0xFBU) && (data[pos + 3U] == 0xFAU);
        is_data_header = (data[pos] == 0xF4U) && (data[pos + 1U] == 0xF3U) &&
            (data[pos + 2U] == 0xF2U) && (data[pos + 3U] == 0xF1U);
        if ((!is_cmd_header) && (!is_data_header)) {
            continue;
        }

        payload_len = (uint16_t)((uint16_t)data[pos + 4U] |
            ((uint16_t)data[pos + 5U] << 8));
        frame_total_len = (uint16_t)(LD2402_FRAME_FIXED_OVERHEAD + payload_len);
        if ((frame_total_len < LD2402_FRAME_FIXED_OVERHEAD) ||
            ((uint32_t)pos + frame_total_len > (uint32_t)len)) {
            continue;
        }

        tail_pos = (uint16_t)(pos + frame_total_len - LD2402_TAIL_LEN);
        expect_tail = is_cmd_header ? g_ld2402_cmd_tail : g_ld2402_data_tail;
        if ((data[tail_pos] != expect_tail[0]) ||
            (data[tail_pos + 1U] != expect_tail[1]) ||
            (data[tail_pos + 2U] != expect_tail[2]) ||
            (data[tail_pos + 3U] != expect_tail[3])) {
            continue;
        }

        /* 命中“使能配置 ACK + 成功状态”则直接标记。 */
        if (is_cmd_header && (payload_len >= 4U)) {
            uint16_t ack_cmd;
            uint16_t ack_status;

            ack_cmd = (uint16_t)((uint16_t)data[pos + 6U] |
                ((uint16_t)data[pos + 7U] << 8));
            ack_status = (uint16_t)((uint16_t)data[pos + 8U] |
                ((uint16_t)data[pos + 9U] << 8));
            if ((ack_cmd == LD2402_ACK_EXPECT_CMD_ENABLE) &&
                (ack_status == LD2402_ACK_EXPECT_STATUS_OK)) {
                *has_enable_ack_out = true;
            }
        }

        return true;
    }

    return false;
}

/**
 * @brief 初始化 ld2402 模块并校验通信。
 * 通过发送"使能配置命令"并读取响应来测试握手。
 */
errcode_t ld2402_init(uint8_t sub_port)
{
    uint8_t rx_buf[128] = {0};
    uint16_t rx_total = 0U;
    uint8_t len = 0;
    uint8_t retry = LD2402_INIT_RETRY_MAX;
    errcode_t ret = ERRCODE_FAIL;

    osal_printk("[ld2402] init on port %u\r\n", sub_port);
    g_ld2402_sub_port = sub_port;
    g_ld2402_ready = 0U;
    g_ld2402_in_config_mode = 0U;
    g_ld2402_last_status = -1;

    while (retry-- > 0) {
        uint8_t round;
        uint8_t drain_round;
        uint8_t retry_index;
        bool has_valid_frame = false;
        bool has_enable_ack = false;

        retry_index = (uint8_t)(LD2402_INIT_RETRY_MAX - retry);

        // 清空接收缓存（带上限，避免读回异常时卡死）
        for (drain_round = 0U; drain_round < LD2402_DRAIN_MAX_ROUND; drain_round++) {
            if (wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf)) == 0U) {
                break;
            }
            ws63_bsp_sleep_ms(2);
        }

        if (drain_round >= LD2402_DRAIN_MAX_ROUND) {
            osal_printk("[ld2402] drain rx hit limit, continue init.\r\n");
        }

        // 发送使能配置
        wk2114_subport_write(sub_port, g_ld2402_cmd_enable, sizeof(g_ld2402_cmd_enable));

        /*
         * 分段轮询接收窗口：
         * 1) ACK 可能不是首字节对齐；
         * 2) ACK 与数据上报可能交织到达。
         */
        rx_total = 0U;
        for (round = 0U; round < LD2402_INIT_POLL_ROUND_MAX; round++) {
            len = wk2114_subport_read(sub_port, &rx_buf[rx_total],
                (uint8_t)(sizeof(rx_buf) - rx_total));
            if (len > 0U) {
                rx_total = (uint16_t)(rx_total + len);
                has_valid_frame = ld2402_find_valid_frame(rx_buf, rx_total, &has_enable_ack);
                if (has_valid_frame) {
                    break;
                }
                if (rx_total >= sizeof(rx_buf)) {
                    break;
                }
            }

            ws63_bsp_sleep_ms(LD2402_INIT_POLL_GAP_MS);
        }

        osal_printk("[ld2402] init try%u rx_total=%u valid_frame=%u enable_ack=%u\r\n",
            (unsigned int)retry_index,
            (unsigned int)rx_total,
            (unsigned int)has_valid_frame,
            (unsigned int)has_enable_ack);

        if (has_valid_frame) {
            if (has_enable_ack) {
                osal_printk("[ld2402] Enable Config ACK received, communication OK.\r\n");
            } else {
                osal_printk("[ld2402] valid frame received, communication OK.\r\n");
            }

            // 通信正常后发送结束配置，恢复工作模式
            wk2114_subport_write(sub_port, g_ld2402_cmd_disable, sizeof(g_ld2402_cmd_disable));
            ws63_bsp_sleep_ms(20);

            g_ld2402_ready = 1U;
            g_ld2402_in_config_mode = 0U;
            ret = ERRCODE_SUCC;
            break;
        }

        osal_printk("[ld2402] init retry...\r\n");
        ws63_bsp_sleep_ms(100);
    }

    if (ret != ERRCODE_SUCC) {
        osal_printk("[ld2402] init test failed.\r\n");
    }

    return ret;
}

/**
 * @brief 处理 ld2402 雷达上报数据
 */
void ld2402_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint32_t now_ms;
    int32_t distance_mm;
    bool is_off;

    (void)sub_port;

    if (len == 0U) {
        return;
    }

    now_ms = ws63_bsp_get_tick_ms();

    /* 正常模式下优先识别 OFF 文本，避免无人状态沿用旧距离。 */
    is_off = ld2402_parse_off_text(data, len);
    if (is_off) {
        g_ld2402_last_distance_mm = -1;
        g_ld2402_last_distance_tick_ms = ws63_bsp_get_tick_ms();
        if (ld2402_can_print_processing_log(now_ms)) {
            osal_printk("LD2402 processing OFF\r\n");
        }
        return;
    }

    /* 门锁编排层只关心明确的距离值，调试打印交给统一节流逻辑控制。 */
    if (ld2402_parse_distance_text(data, len, &distance_mm)) {
        g_ld2402_last_distance_mm = distance_mm;
        g_ld2402_last_distance_tick_ms = ws63_bsp_get_tick_ms();
        if (ld2402_can_print_processing_log(now_ms)) {
            osal_printk("LD2402 processing distance:%ld\r\n", (long)distance_mm);
        }
        return;
    }

    /* 工程模式帧会直接上报状态+距离，这里顺手刷新最后一次距离值。 */
    if (ld2402_update_from_data_frames(data, len)) {
        return;
    }

    if (!ld2402_can_print_processing_log(now_ms)) {
        return;
    }

    /* 仅限制日志输出频率，不改变日志文案语义，便于兼容现场既有关键字检索。 */
    osal_printk("LD2402 processing %u bytes.\r\n", (unsigned int)len);
    /* logic to parse ld2402 radar packet */
}

/**
 * @brief 设置 LD2402 运行态日志开关。
 */
errcode_t ld2402_set_data_log_enable(uint8_t enable)
{
    uint8_t prev_enable;

    prev_enable = g_ld2402_data_log_enable;
    g_ld2402_data_log_enable = (enable != 0U) ? 1U : 0U;
    if ((g_ld2402_data_log_enable != 0U) && (prev_enable == 0U)) {
        /*
         * 退出静默模式后直接清零节流游标，确保下一条 distance:xxx 立刻可见，
         * 避免刚从 Die 之后恢复日志时还被上一轮静默窗口挡住。
         */
        g_ld2402_data_last_log_ms = 0U;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief 获取 LD2402 运行态日志开关状态。
 */
uint8_t ld2402_get_data_log_enable(void)
{
    return g_ld2402_data_log_enable;
}

/**
 * @brief 设置 LD2402 运行态日志最小输出间隔。
 */
errcode_t ld2402_set_data_log_gap_ms(uint32_t gap_ms)
{
    g_ld2402_data_log_gap_ms = gap_ms;
    return ERRCODE_SUCC;
}

/**
 * @brief 获取 LD2402 运行态日志最小输出间隔。
 */
uint32_t ld2402_get_data_log_gap_ms(void)
{
    return g_ld2402_data_log_gap_ms;
}

/**
 * @brief 获取最近一次解析到的距离值。
 */
int32_t ld2402_get_last_distance_mm(void)
{
    return g_ld2402_last_distance_mm;
}

/**
 * @brief 获取最近一次有效距离值的更新时间。
 */
uint32_t ld2402_get_last_distance_tick_ms(void)
{
    return g_ld2402_last_distance_tick_ms;
}

/**
 * @brief 获取 LD2402 当前是否已完成初始化。
 */
uint8_t ld2402_is_ready(void)
{
    return g_ld2402_ready;
}

/**
 * @brief 获取 LD2402 当前是否处于配置模式。
 */
uint8_t ld2402_is_in_config_mode(void)
{
    return g_ld2402_in_config_mode;
}

/**
 * @brief 向 LD2402 发送原始命令帧。
 */
errcode_t ld2402_send_raw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U) || (g_ld2402_sub_port == 0U) || (g_ld2402_ready == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    return wk2114_subport_write(g_ld2402_sub_port, data, len);
}

/**
 * @brief 读取固件版本号。
 */
errcode_t ld2402_get_version(char *buf, uint16_t buf_len)
{
    uint8_t payload[64] = {0};
    uint16_t payload_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    uint16_t version_len;
    errcode_t ret;

    if ((buf == NULL) || (buf_len < 2U) || (g_ld2402_ready == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_VERSION, NULL, 0U, payload, sizeof(payload), &payload_len, &ack_status);
    ld2402_exit_config_mode();
    if ((ret != ERRCODE_SUCC) || (payload_len < 2U)) {
        return ERRCODE_FAIL;
    }

    version_len = ld2402_read_uint16_le(payload);
    if ((version_len == 0U) || ((uint32_t)version_len + 2U > payload_len) || (version_len >= buf_len)) {
        return ERRCODE_FAIL;
    }

    if (memcpy_s(buf, buf_len, &payload[2U], version_len) != EOK) {
        return ERRCODE_FAIL;
    }
    buf[version_len] = '\0';
    return ERRCODE_SUCC;
}

/**
 * @brief 读取字符形式序列号。
 */
errcode_t ld2402_get_sn_char(char *buf, uint16_t buf_len)
{
    uint8_t payload[64] = {0};
    uint16_t payload_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    uint16_t sn_len;
    errcode_t ret;

    if ((buf == NULL) || (buf_len < 2U) || (g_ld2402_ready == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_READ_SN_CHAR, NULL, 0U,
        payload, sizeof(payload), &payload_len, &ack_status);
    ld2402_exit_config_mode();
    if ((ret != ERRCODE_SUCC) || (payload_len < 2U)) {
        return ERRCODE_FAIL;
    }

    sn_len = ld2402_read_uint16_le(payload);
    if ((sn_len == 0U) || ((uint32_t)sn_len + 2U > payload_len) || (sn_len >= buf_len)) {
        return ERRCODE_FAIL;
    }

    if (memcpy_s(buf, buf_len, &payload[2U], sn_len) != EOK) {
        return ERRCODE_FAIL;
    }
    buf[sn_len] = '\0';
    return ERRCODE_SUCC;
}

/**
 * @brief 读取十六进制形式序列号。
 */
int32_t ld2402_get_sn_hex(uint8_t *buf, uint16_t buf_len)
{
    uint8_t payload[64] = {0};
    uint16_t payload_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    uint16_t sn_len;
    errcode_t ret;

    if ((buf == NULL) || (buf_len == 0U) || (g_ld2402_ready == 0U)) {
        return -1;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return -1;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_READ_SN_HEX, NULL, 0U,
        payload, sizeof(payload), &payload_len, &ack_status);
    ld2402_exit_config_mode();
    if ((ret != ERRCODE_SUCC) || (payload_len < 2U)) {
        return -1;
    }

    sn_len = ld2402_read_uint16_le(payload);
    if ((sn_len == 0U) || ((uint32_t)sn_len + 2U > payload_len) || (sn_len > buf_len)) {
        return -1;
    }

    if (memcpy_s(buf, buf_len, &payload[2U], sn_len) != EOK) {
        return -1;
    }

    return (int32_t)sn_len;
}

/**
 * @brief 读取单个参数值。
 */
errcode_t ld2402_read_param(uint16_t param_id, uint32_t *value)
{
    uint8_t cmd_data[2] = {0};
    uint8_t payload[64] = {0};
    uint16_t payload_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    errcode_t ret;

    if ((value == NULL) || (g_ld2402_ready == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ld2402_write_uint16_le(cmd_data, param_id);

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_GET_PARAM, cmd_data, sizeof(cmd_data),
        payload, sizeof(payload), &payload_len, &ack_status);
    ld2402_exit_config_mode();
    if ((ret != ERRCODE_SUCC) || (payload_len < 4U)) {
        return ERRCODE_FAIL;
    }

    *value = ld2402_read_uint32_le(payload);
    return ERRCODE_SUCC;
}

/**
 * @brief 写入单个参数值。
 */
errcode_t ld2402_set_param(uint16_t param_id, uint32_t value)
{
    uint8_t cmd_data[6] = {0};
    errcode_t ret;

    if (g_ld2402_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ld2402_write_uint16_le(cmd_data, param_id);
    ld2402_write_uint32_le(&cmd_data[2U], value);

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_SET_PARAM, cmd_data, sizeof(cmd_data),
        NULL, 0U, NULL, NULL);
    ld2402_exit_config_mode();
    return ret;
}

/**
 * @brief 设置最远探测距离。
 */
errcode_t ld2402_set_max_distance(float distance_m)
{
    uint16_t value;

    value = (uint16_t)(distance_m * 10.0f);
    if (value < 7U) {
        value = 7U;
    }
    if (value > 100U) {
        value = 100U;
    }

    return ld2402_set_param(LD2402_PARAM_MAX_DIST, value);
}

/**
 * @brief 设置目标消失延迟时间。
 */
errcode_t ld2402_set_disappear_delay(uint16_t seconds)
{
    return ld2402_set_param(LD2402_PARAM_DELAY_TIME, seconds);
}

/**
 * @brief 切换到正常输出模式。
 */
errcode_t ld2402_set_normal_mode(void)
{
    uint8_t cmd_data[6] = {0};
    errcode_t ret;

    if (g_ld2402_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ld2402_write_uint16_le(cmd_data, 0x0000U);
    ld2402_write_uint32_le(&cmd_data[2U], LD2402_PROTOCOL_MODE_NORMAL);

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_OUTPUT_MODE, cmd_data, sizeof(cmd_data),
        NULL, 0U, NULL, NULL);
    ld2402_exit_config_mode();
    return ret;
}

/**
 * @brief 切换到工程输出模式。
 */
errcode_t ld2402_set_engineering_mode(void)
{
    uint8_t cmd_data[6] = {0};
    errcode_t ret;

    if (g_ld2402_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ld2402_write_uint16_le(cmd_data, 0x0000U);
    ld2402_write_uint32_le(&cmd_data[2U], LD2402_PROTOCOL_MODE_ENGINEERING);

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_OUTPUT_MODE, cmd_data, sizeof(cmd_data),
        NULL, 0U, NULL, NULL);
    ld2402_exit_config_mode();
    return ret;
}

/**
 * @brief 保存当前参数到掉电区。
 */
errcode_t ld2402_save_params(void)
{
    errcode_t ret;

    if (g_ld2402_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_SAVE_PARAM, NULL, 0U, NULL, 0U, NULL, NULL);
    ld2402_exit_config_mode();
    return ret;
}

/**
 * @brief 触发自动增益调节。
 */
errcode_t ld2402_auto_gain_adjust(void)
{
    errcode_t ret;

    if (g_ld2402_ready == 0U) {
        return ERRCODE_FAIL;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_AUTO_GAIN, NULL, 0U, NULL, 0U, NULL, NULL);
    ld2402_exit_config_mode();
    return ret;
}

/**
 * @brief 开始自动门限生成。
 */
errcode_t ld2402_start_auto_threshold(uint16_t trig_coef_10x,
    uint16_t hold_coef_10x, uint16_t static_coef_10x)
{
    uint8_t cmd_data[6] = {0};
    errcode_t ret;

    if ((g_ld2402_ready == 0U) || (!ld2402_auto_coef_valid(trig_coef_10x)) ||
        (!ld2402_auto_coef_valid(hold_coef_10x)) || (!ld2402_auto_coef_valid(static_coef_10x))) {
        return ERRCODE_INVALID_PARAM;
    }

    ld2402_write_uint16_le(cmd_data, trig_coef_10x);
    ld2402_write_uint16_le(&cmd_data[2U], hold_coef_10x);
    ld2402_write_uint16_le(&cmd_data[4U], static_coef_10x);

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_AUTO_THRESHOLD, cmd_data, sizeof(cmd_data),
        NULL, 0U, NULL, NULL);
    ld2402_exit_config_mode();
    return ret;
}

/**
 * @brief 查询自动门限生成进度。
 */
errcode_t ld2402_get_auto_threshold_progress(uint16_t *progress_percent)
{
    uint8_t payload[16] = {0};
    uint16_t payload_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    errcode_t ret;

    if ((progress_percent == NULL) || (g_ld2402_ready == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_AUTO_THRESHOLD_PROGRESS, NULL, 0U,
        payload, sizeof(payload), &payload_len, &ack_status);
    ld2402_exit_config_mode();
    if ((ret != ERRCODE_SUCC) || (payload_len < 2U)) {
        return ERRCODE_FAIL;
    }

    *progress_percent = ld2402_read_uint16_le(payload);
    return ERRCODE_SUCC;
}

/**
 * @brief 查询自动门限干扰状态。
 */
errcode_t ld2402_get_auto_threshold_alarm(uint16_t *alarm_status, uint16_t *gate_bitmap)
{
    uint8_t payload[16] = {0};
    uint16_t payload_len = 0U;
    uint16_t ack_status = 0xFFFFU;
    errcode_t ret;

    if ((alarm_status == NULL) || (gate_bitmap == NULL) || (g_ld2402_ready == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = ld2402_enter_config_mode();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ld2402_send_command_wait_ack(LD2402_CMD_AUTO_THRESHOLD_ALARM, NULL, 0U,
        payload, sizeof(payload), &payload_len, &ack_status);
    ld2402_exit_config_mode();
    if ((ret != ERRCODE_SUCC) || (payload_len < 4U)) {
        return ERRCODE_FAIL;
    }

    *alarm_status = ld2402_read_uint16_le(payload);
    *gate_bitmap = ld2402_read_uint16_le(&payload[2U]);
    return ERRCODE_SUCC;
}

/**
 * @brief 读取电源干扰参数。
 */
errcode_t ld2402_get_power_interference(uint32_t *value)
{
    return ld2402_read_param(LD2402_PARAM_POWER_INTER, value);
}

/**
 * @brief 执行 0x003F 参数的读后回写流程。
 */
errcode_t ld2402_refresh_save_flag(void)
{
    uint32_t save_flag = 0U;

    if (ld2402_read_param(LD2402_PARAM_SAVE_FLAG, &save_flag) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return ld2402_set_param(LD2402_PARAM_SAVE_FLAG, save_flag);
}
