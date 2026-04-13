/**
 * @file ws63_final_task_sensor_bridge.c
 * @brief Task 层传感器桥接子模块（LD2402/ZW101）。
 */

#include "ws63_final_task_internal.h"

#include <stddef.h>

#include "osal_debug.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_final_sle.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"

/* ZW101 自动识别任务请求状态：独立于门锁主状态机，避免阻塞其他认证链路。 */
static uint8_t g_ws63_zw101_task_started = 0U;
static uint8_t g_ws63_zw101_auto_identify_requested = 0U;
static uint8_t g_ws63_zw101_auto_identify_cancelled = 0U;

/* 仅供本文件内部使用，避免把 ZW101 任务就绪查询扩散到上层接口。 */
static uint8_t ws63_task_zw101_is_ready(void);

/**
 * @brief ZW101 自动识别任务状态锁。
 */
static unsigned int ws63_zw101_lock(void)
{
    return ws63_os_irq_lock();
}

/**
 * @brief ZW101 自动识别任务状态解锁。
 */
static void ws63_zw101_unlock(unsigned int irq_status)
{
    ws63_os_irq_unlock(irq_status);
}

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
 * @brief 查询 LD2402 是否已完成初始化。
 */
uint8_t ws63_task_ld2402_is_ready(void)
{
    return ld2402_is_ready();
}

/**
 * @brief 查询 LD2402 当前是否处于配置模式。
 */
uint8_t ws63_task_ld2402_is_in_config_mode(void)
{
    return ld2402_is_in_config_mode();
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
 * @brief 读取 LD2402 固件版本。
 */
errcode_t ws63_task_ld2402_get_version(char *buf, uint16_t buf_len)
{
    return ld2402_get_version(buf, buf_len);
}

/**
 * @brief 读取 LD2402 字符形式序列号。
 */
errcode_t ws63_task_ld2402_get_sn_char(char *buf, uint16_t buf_len)
{
    return ld2402_get_sn_char(buf, buf_len);
}

/**
 * @brief 读取 LD2402 十六进制形式序列号。
 */
int32_t ws63_task_ld2402_get_sn_hex(uint8_t *buf, uint16_t buf_len)
{
    return ld2402_get_sn_hex(buf, buf_len);
}

/**
 * @brief 读取 LD2402 单个参数值。
 */
errcode_t ws63_task_ld2402_read_param(uint16_t param_id, uint32_t *value)
{
    return ld2402_read_param(param_id, value);
}

/**
 * @brief 写入 LD2402 单个参数值。
 */
errcode_t ws63_task_ld2402_set_param(uint16_t param_id, uint32_t value)
{
    return ld2402_set_param(param_id, value);
}

/**
 * @brief 设置 LD2402 最大距离。
 */
errcode_t ws63_task_ld2402_set_max_distance(float distance_m)
{
    return ld2402_set_max_distance(distance_m);
}

/**
 * @brief 设置 LD2402 目标消失延迟。
 */
errcode_t ws63_task_ld2402_set_disappear_delay(uint16_t seconds)
{
    return ld2402_set_disappear_delay(seconds);
}

/**
 * @brief 切换 LD2402 到正常模式。
 */
errcode_t ws63_task_ld2402_set_normal_mode(void)
{
    return ld2402_set_normal_mode();
}

/**
 * @brief 切换 LD2402 到工程模式。
 */
errcode_t ws63_task_ld2402_set_engineering_mode(void)
{
    return ld2402_set_engineering_mode();
}

/**
 * @brief 保存 LD2402 当前参数。
 */
errcode_t ws63_task_ld2402_save_params(void)
{
    return ld2402_save_params();
}

/**
 * @brief 触发 LD2402 自动增益调节。
 */
errcode_t ws63_task_ld2402_auto_gain_adjust(void)
{
    return ld2402_auto_gain_adjust();
}

/**
 * @brief 开始 LD2402 自动门限生成。
 */
errcode_t ws63_task_ld2402_start_auto_threshold(uint16_t trig_coef_10x,
    uint16_t hold_coef_10x, uint16_t static_coef_10x)
{
    return ld2402_start_auto_threshold(trig_coef_10x, hold_coef_10x, static_coef_10x);
}

/**
 * @brief 查询 LD2402 自动门限生成进度。
 */
errcode_t ws63_task_ld2402_get_auto_threshold_progress(uint16_t *progress_percent)
{
    return ld2402_get_auto_threshold_progress(progress_percent);
}

/**
 * @brief 查询 LD2402 自动门限干扰状态。
 */
errcode_t ws63_task_ld2402_get_auto_threshold_alarm(uint16_t *alarm_status, uint16_t *gate_bitmap)
{
    return ld2402_get_auto_threshold_alarm(alarm_status, gate_bitmap);
}

/**
 * @brief 读取 LD2402 电源干扰参数。
 */
errcode_t ws63_task_ld2402_get_power_interference(uint32_t *value)
{
    return ld2402_get_power_interference(value);
}

/**
 * @brief 执行 LD2402 0x003F 读后回写流程。
 */
errcode_t ws63_task_ld2402_refresh_save_flag(void)
{
    return ld2402_refresh_save_flag();
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
 * @brief 设置 LD2402 子口通道使能状态。
 */
errcode_t ws63_task_ld2402_set_channel_enable(uint8_t enable)
{
    errcode_t ret;

    if (ws63_is_subport_config_enabled(LD2402_SUBPORT) == 0U) {
        return ERRCODE_FAIL;
    }

    ret = wk2114_subport_set_sleep(LD2402_SUBPORT, (enable != 0U) ? 0U : 1U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ws63_set_subport_runtime_enable(LD2402_SUBPORT, (enable != 0U) ? 1U : 0U);
}

/**
 * @brief 查询 LD2402 子口通道是否启用。
 */
uint8_t ws63_task_ld2402_is_channel_enabled(void)
{
    return ws63_is_subport_enabled(LD2402_SUBPORT);
}

/**
 * @brief 查询 ZW101 是否已完成初始化。
 */
static uint8_t ws63_task_zw101_is_ready(void)
{
    return zw101_is_ready();
}

/**
 * @brief 请求 ZW101 自动识别任务执行一次识别。
 */
errcode_t ws63_task_zw101_request_auto_identify(void)
{
    unsigned int irq_status;

    if (g_ws63_zw101_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = ws63_zw101_lock();
    g_ws63_zw101_auto_identify_requested = 1U;
    g_ws63_zw101_auto_identify_cancelled = 0U;
    ws63_zw101_unlock(irq_status);

    /* 指纹链路一旦被拉起，就把接近窗口往后推，给后续识别留足时间。 */
    (void)ws63_lock_mgr_refresh_auth_window();
    return ERRCODE_SUCC;
}

/**
 * @brief 取消尚未开始的 ZW101 自动识别请求。
 */
void ws63_task_zw101_cancel_auto_identify_request(void)
{
    unsigned int irq_status;

    irq_status = ws63_zw101_lock();
    g_ws63_zw101_auto_identify_requested = 0U;
    g_ws63_zw101_auto_identify_cancelled = 1U;
    ws63_zw101_unlock(irq_status);
}

/**
 * @brief 向门锁结果链路上报一次 ZW101 自动识别结果。
 */
static void ws63_zw101_report_auto_identify_result(errcode_t ret, uint16_t match_id, uint16_t score)
{
    uint8_t passed;

    passed = (ret == ERRCODE_SUCC) ? 1U : 0U;
    (void)ws63_lock_mgr_refresh_auth_window();
    if (passed != 0U) {
        osal_printk("[zw101] auto identify pass id=%u score=%u\r\n",
            (unsigned int)match_id,
            (unsigned int)score);
    } else {
        osal_printk("[zw101] auto identify fail, ret=0x%x\r\n", (unsigned int)ret);
    }

    (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_ZW101, passed);
}

/**
 * @brief ZW101 自动识别任务入口。
 */
static void *ws63_zw101_task_entry(const char *arg)
{
    (void)arg;

    while (1) {
        uint8_t request;
        uint8_t cancelled;

        {
            unsigned int irq_status = ws63_zw101_lock();
            request = g_ws63_zw101_auto_identify_requested;
            cancelled = g_ws63_zw101_auto_identify_cancelled;
            ws63_zw101_unlock(irq_status);
        }

        if ((request == 0U) || (cancelled != 0U)) {
            ws63_os_sleep_ms(20U);
            continue;
        }

        if ((ws63_task_zw101_is_ready() == 0U) || (ws63_lock_mgr_is_armed() == 0U)) {
            ws63_os_sleep_ms(20U);
            continue;
        }

        {
            uint16_t match_id = 0U;
            uint16_t score = 0U;
            errcode_t ret;

            /* 先清标志，再进入阻塞式识别，避免同一轮接近窗口重复触发。 */
            ws63_task_zw101_cancel_auto_identify_request();

            /* 识别开始时刷新一次窗口，防止长耗时识别把唤醒态提前打断。 */
            (void)ws63_lock_mgr_refresh_auth_window();

            osal_printk("[zw101] auto identify start\r\n");
            ret = zw101_business_auto_identify(0U, 0U, 0U, &match_id, &score);
            ws63_zw101_report_auto_identify_result(ret, match_id, score);
        }
    }

    return NULL;
}

/**
 * @brief 启动 ZW101 自动识别任务。
 */
errcode_t ws63_zw101_task_start(void)
{
    errcode_t ret;

    if (g_ws63_zw101_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_start_task("ws63_zw101_task",
        ws63_zw101_task_entry,
        0U,
        WS63_TASK_STACK_SIZE,
        WS63_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[zw101] task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_zw101_task_started = 1U;
    return ERRCODE_SUCC;
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
