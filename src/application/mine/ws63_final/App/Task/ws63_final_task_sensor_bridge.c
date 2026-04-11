/**
 * @file ws63_final_task_sensor_bridge.c
 * @brief Task 层传感器桥接子模块（LD2402/ZW101）。
 */

#include "ws63_final_task.h"

#include <stddef.h>

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "ws63_final_sle.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"

/**
 * @brief 重新初始化 LD2402。
 */
errcode_t ws63_task_ld2402_reinit(void)
{
    if (!ws63_is_subport_enabled(LD2402_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return ld2402_init(LD2402_SUBPORT);
}

/**
 * @brief 向 LD2402 发送原始命令帧。
 */
errcode_t ws63_task_ld2402_send_raw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (!ws63_is_subport_enabled(LD2402_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return wk2114_subport_write(LD2402_SUBPORT, data, len);
}

/**
 * @brief 设置 LD2402 运行态日志开关。
 */
errcode_t ws63_task_ld2402_set_log_enable(uint8_t enable)
{
    return ld2402_set_data_log_enable(enable);
}

/**
 * @brief 获取 LD2402 运行态日志开关。
 */
uint8_t ws63_task_ld2402_get_log_enable(void)
{
    return ld2402_get_data_log_enable();
}

/**
 * @brief 设置 LD2402 运行态日志最小输出间隔。
 */
errcode_t ws63_task_ld2402_set_log_gap_ms(uint32_t gap_ms)
{
    return ld2402_set_data_log_gap_ms(gap_ms);
}

/**
 * @brief 获取 LD2402 运行态日志最小输出间隔。
 */
uint32_t ws63_task_ld2402_get_log_gap_ms(void)
{
    return ld2402_get_data_log_gap_ms();
}

/**
 * @brief 获取 LD2402 最近一次解析到的距离值。
 */
int32_t ws63_task_ld2402_get_distance_mm(void)
{
    return ld2402_get_last_distance_mm();
}

/**
 * @brief 获取 LD2402 最近一次有效距离值的更新时间。
 */
uint32_t ws63_task_ld2402_get_distance_tick_ms(void)
{
    return ld2402_get_last_distance_tick_ms();
}

/**
 * @brief 设置 SLE 上行 success 日志开关。
 */
errcode_t ws63_task_sle_uplink_log_set_enable(uint8_t enable)
{
    return ws63_sle_set_uplink_success_log_enable(enable);
}

/**
 * @brief 获取 SLE 上行 success 日志开关。
 */
uint8_t ws63_task_sle_uplink_log_get_enable(void)
{
    return ws63_sle_get_uplink_success_log_enable();
}

/**
 * @brief 设置 SLE 上行 success 日志最小输出间隔。
 */
errcode_t ws63_task_sle_uplink_log_set_gap_ms(uint32_t gap_ms)
{
    return ws63_sle_set_uplink_success_log_gap_ms(gap_ms);
}

/**
 * @brief 获取 SLE 上行 success 日志最小输出间隔。
 */
uint32_t ws63_task_sle_uplink_log_get_gap_ms(void)
{
    return ws63_sle_get_uplink_success_log_gap_ms();
}

/**
 * @brief 重新初始化 ZW101（触发握手检测）。
 */
errcode_t ws63_task_zw101_reinit(void)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return zw101_init(ZW101_SUBPORT);
}

/**
 * @brief 执行 ZW101 标准握手命令（0x35）。
 */
errcode_t ws63_task_zw101_handshake(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_maint_handshake(ack_out);
}

/**
 * @brief 执行 ZW101 传感器检测命令（0x36）。
 */
errcode_t ws63_task_zw101_check_sensor(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_maint_check_sensor(ack_out);
}

/**
 * @brief 向 ZW101 发送原始命令帧。
 */
errcode_t ws63_task_zw101_send_raw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return zw101_send_raw(data, len);
}

/**
 * @brief 执行 ZW101 ZA 握手（GetEcho）。
 */
errcode_t ws63_task_zw101_za_get_echo(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return zw101_za_get_echo(ack_out);
}

/**
 * @brief 执行 ZW101 ZA 自动登记（AutoLogin）。
 */
errcode_t ws63_task_zw101_za_auto_login(uint8_t wait_time,
    uint8_t sample_interval_code,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return zw101_za_auto_login(wait_time,
        sample_interval_code,
        press_times,
        page_id,
        allow_dup,
        ack_out);
}

/**
 * @brief 执行 ZW101 ZA 自动搜索（AutoSearch）。
 */
errcode_t ws63_task_zw101_za_auto_search(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out,
    uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t result;

    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_za_auto_search(wait_time, start_page, page_num, &result);
    if (ack_out != NULL) {
        *ack_out = result.ack_code;
    }
    if ((page_id_out != NULL) && (result.payload_len >= 5U)) {
        *page_id_out = (uint16_t)(((uint16_t)result.payload[1] << 8) | result.payload[2]);
    }
    if ((score_out != NULL) && (result.payload_len >= 5U)) {
        *score_out = (uint16_t)(((uint16_t)result.payload[3] << 8) | result.payload[4]);
    }

    return ret;
}

/**
 * @brief 执行 ZW101 ZA 搜索指纹（带残留判断）。
 */
errcode_t ws63_task_zw101_za_search_res_back(uint8_t buffer_id,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out,
    uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t result;

    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_za_search_res_back(buffer_id, start_page, page_num, &result);
    if (ack_out != NULL) {
        *ack_out = result.ack_code;
    }
    if ((page_id_out != NULL) && (result.payload_len >= 5U)) {
        *page_id_out = (uint16_t)(((uint16_t)result.payload[1] << 8) | result.payload[2]);
    }
    if ((score_out != NULL) && (result.payload_len >= 5U)) {
        *score_out = (uint16_t)(((uint16_t)result.payload[3] << 8) | result.payload[4]);
    }

    return ret;
}

/**
 * @brief 执行 ZW101 ZA 自动登记（灯常亮）。
 */
errcode_t ws63_task_zw101_za_auto_login_stab(uint8_t wait_time,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return zw101_za_auto_login_stab_light(wait_time,
        press_times,
        page_id,
        allow_dup,
        ack_out);
}

/**
 * @brief 执行 ZW101 ZA 自动搜索（搜前提示）。
 */
errcode_t ws63_task_zw101_za_auto_search_echo(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out,
    uint8_t *ack_out)
{
    errcode_t ret;
    zw101_ack_result_t result;

    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_za_auto_search_with_echo(wait_time, start_page, page_num, &result);
    if (ack_out != NULL) {
        *ack_out = result.ack_code;
    }
    if ((page_id_out != NULL) && (result.payload_len >= 5U)) {
        *page_id_out = (uint16_t)(((uint16_t)result.payload[1] << 8) | result.payload[2]);
    }
    if ((score_out != NULL) && (result.payload_len >= 5U)) {
        *score_out = (uint16_t)(((uint16_t)result.payload[3] << 8) | result.payload[4]);
    }

    return ret;
}

/**
 * @brief 执行 ZW101 ZA 过程终止。
 */
errcode_t ws63_task_zw101_za_terminate(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return zw101_za_process_terminate(ack_out);
}
