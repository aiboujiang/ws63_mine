/**
 * @file ws63_final_task.h
 * @brief WK2114 最终版应用任务层接口。
 */

#ifndef WS63_TASK_H
#define WS63_TASK_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 子串口接收回调。
 *
 * @param sub_port 子串口号（1~4）。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 */
typedef void (*ws63_rx_callback_t)(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 注册子串口接收回调。
 *
 * @param sub_port 子串口号（1~4）。
 * @param callback 回调函数，可为 NULL（表示注销）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_register_rx_callback(uint8_t sub_port,
    ws63_rx_callback_t callback);

/**
 * @brief 通过 WK2114 子串口发送数据。
 *
 * @param sub_port 子串口号（1~4）。
 * @param data     数据缓冲区。
 * @param len      数据长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_send(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 控制电机正转（IA=0，IB=PWM）。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_forward(uint8_t duty_percent);

/**
 * @brief 控制电机反转（IA=PWM，IB=0）。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_reverse(uint8_t duty_percent);

/**
 * @brief 电机停止（滑行，IA=0，IB=0）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_coast_stop(void);

/**
 * @brief 电机刹车（急停，IA=1，IB=1）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_brake_stop(void);

/**
 * @brief 动态调整当前运行方向占空比。
 *
 * @param duty_percent 占空比百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_motor_set_duty(uint8_t duty_percent);

/**
 * @brief 打开蜂鸣器并设置频率。
 *
 * @param freq_hz 目标频率（Hz）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_buzzer_on(uint16_t freq_hz);

/**
 * @brief 关闭蜂鸣器。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_buzzer_off(void);

/**
 * @brief 查询蜂鸣器是否正在发声。
 *
 * @return uint8_t 1=正在发声，0=静音。
 */
uint8_t ws63_task_buzzer_is_on(void);

/**
 * @brief 获取蜂鸣器当前频率。
 *
 * @return uint16_t 当前频率（Hz）。
 */
uint16_t ws63_task_buzzer_get_freq_hz(void);

/**
 * @brief 设置蜂鸣器音量。
 *
 * @param volume_percent 音量百分比（0~100）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_buzzer_set_volume(uint8_t volume_percent);

/**
 * @brief 获取蜂鸣器当前音量。
 *
 * @return uint8_t 当前音量百分比。
 */
uint8_t ws63_task_buzzer_get_volume(void);

/**
 * @brief 重新初始化 RGB 驱动并恢复演示模式。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_rgb_reinit(void);

/**
 * @brief 设置 RGB 颜色（8bit 通道）。
 *
 * @param r 红色分量（0~255）。
 * @param g 绿色分量（0~255）。
 * @param b 蓝色分量（0~255）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_rgb_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 关闭 RGB（输出黑色）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_rgb_off(void);

/**
 * @brief 设置 RGB 演示模式开关。
 *
 * @param enable 1=开启演示，0=关闭演示。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_rgb_set_demo_enable(uint8_t enable);

/**
 * @brief 查询 RGB 驱动是否已就绪。
 *
 * @return uint8_t 1=就绪，0=未就绪。
 */
uint8_t ws63_task_rgb_is_ready(void);

/**
 * @brief 查询 RGB 演示模式是否开启。
 *
 * @return uint8_t 1=开启，0=关闭。
 */
uint8_t ws63_task_rgb_is_demo_enable(void);

/**
 * @brief 重新初始化 LD2402（兼容命令别名 LD2401）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_ld2402_reinit(void);

/**
 * @brief 向 LD2402 发送原始命令帧。
 *
 * @param data 命令缓冲区。
 * @param len  命令长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_ld2402_send_raw(const uint8_t *data, uint16_t len);

/**
 * @brief 重新初始化 ZW101（触发握手检测）。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_zw101_reinit(void);

/**
 * @brief 执行 ZW101 标准握手命令（0x35）。
 *
 * @param ack_out 输出 ACK 码，可为空。
 * @return errcode_t ERRCODE_SUCC=握手成功，其他=失败。
 */
errcode_t ws63_task_zw101_handshake(uint8_t *ack_out);

/**
 * @brief 执行 ZW101 传感器检测命令（0x36）。
 *
 * @param ack_out 输出 ACK 码，可为空。
 * @return errcode_t ERRCODE_SUCC=检测成功，其他=失败。
 */
errcode_t ws63_task_zw101_check_sensor(uint8_t *ack_out);

/**
 * @brief 向 ZW101 发送原始命令帧。
 *
 * @param data 命令缓冲区。
 * @param len  命令长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_zw101_send_raw(const uint8_t *data, uint16_t len);

/**
 * @brief 执行 ZW101 ZA 握手（GetEcho）。
 *
 * @param ack_out 输出确认码，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_zw101_za_get_echo(uint8_t *ack_out);

/**
 * @brief 执行 ZW101 ZA 自动登记（AutoLogin）。
 */
errcode_t ws63_task_zw101_za_auto_login(uint8_t wait_time,
    uint8_t sample_interval_code,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out);

/**
 * @brief 执行 ZW101 ZA 自动搜索（AutoSearch）。
 *
 * @param wait_time 待指时长。
 * @param start_page 起始页。
 * @param page_num 搜索页数。
 * @param page_id_out 输出匹配页码，可为 NULL。
 * @param score_out 输出匹配得分，可为 NULL。
 * @param ack_out 输出确认码，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_zw101_za_auto_search(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out,
    uint8_t *ack_out);

/**
 * @brief 执行 ZW101 ZA 搜索指纹（带残留判断，SearchResBack）。
 */
errcode_t ws63_task_zw101_za_search_res_back(uint8_t buffer_id,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out,
    uint8_t *ack_out);

/**
 * @brief 执行 ZW101 ZA 自动登记（灯常亮，AutoLoginStabLight）。
 */
errcode_t ws63_task_zw101_za_auto_login_stab(uint8_t wait_time,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out);

/**
 * @brief 执行 ZW101 ZA 自动搜索（搜前提示，AutoSearchWithEcho）。
 */
errcode_t ws63_task_zw101_za_auto_search_echo(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out,
    uint8_t *ack_out);

/**
 * @brief 执行 ZW101 ZA 过程终止（ProcessTerminateCmd）。
 */
errcode_t ws63_task_zw101_za_terminate(uint8_t *ack_out);

/**
 * @brief 获取编码器最新 RPM。
 *
 * @return int32_t 有符号 RPM。
 */
int32_t ws63_task_get_motor_rpm(void);

/**
 * @brief 获取编码器累计计数值。
 *
 * @return int32_t 有符号累计脉冲计数。
 */
int32_t ws63_task_get_encoder_total_count(void);

/**
 * @brief WK2114 最终版业务任务入口。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
void *ws63_task_entry(const char *arg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
