/**
 * @file ld2402.h
 * @brief LD2402 毫米波雷达驱动。
 */
#ifndef LD2402_H
#define LD2402_H

#include "errcode.h"
#include <stdint.h>

/**
 * @brief 初始化LD2402雷达模块并进行通信测试
 *
 * @param sub_port 对应的串口号
 * @return errcode_t 成功返回ERRCODE_SUCC，否则返回失败代码
 */
errcode_t ld2402_init(uint8_t sub_port);

/**
 * @brief 获取 LD2402 当前是否已完成初始化。
 *
 * @return uint8_t 1=已就绪，0=未就绪。
 */
uint8_t ld2402_is_ready(void);

/**
 * @brief 获取 LD2402 当前是否处于配置模式。
 *
 * @return uint8_t 1=是，0=否。
 */
uint8_t ld2402_is_in_config_mode(void);

/**
 * @brief 处理LD2402雷达接收到的数据
 *
 * @param sub_port 对应串口号
 * @param data 接收到的数据缓冲区
 * @param len 接收到的数据长度
 */
void ld2402_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 向 LD2402 发送原始十六进制命令帧。
 *
 * @param data 命令缓冲区。
 * @param len  命令长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_send_raw(const uint8_t *data, uint16_t len);

/**
 * @brief 读取固件版本号。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_get_version(char *buf, uint16_t buf_len);

/**
 * @brief 读取字符形式序列号。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_get_sn_char(char *buf, uint16_t buf_len);

/**
 * @brief 读取十六进制形式序列号。
 *
 * @param buf     输出缓冲区。
 * @param buf_len 缓冲区长度。
 * @return int32_t 读取到的字节数，失败返回负值。
 */
int32_t ld2402_get_sn_hex(uint8_t *buf, uint16_t buf_len);

/**
 * @brief 读取单个参数值。
 *
 * @param param_id 参数 ID。
 * @param value    输出值。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_read_param(uint16_t param_id, uint32_t *value);

/**
 * @brief 写入单个参数值。
 *
 * @param param_id 参数 ID。
 * @param value    参数值。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_set_param(uint16_t param_id, uint32_t value);

/**
 * @brief 设置最远探测距离。
 *
 * @param distance_m 距离，单位米。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_set_max_distance(float distance_m);

/**
 * @brief 设置目标消失延迟时间。
 *
 * @param seconds 延迟秒数。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_set_disappear_delay(uint16_t seconds);

/**
 * @brief 切换到正常输出模式。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_set_normal_mode(void);

/**
 * @brief 切换到工程输出模式。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_set_engineering_mode(void);

/**
 * @brief 保存当前参数到掉电区。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_save_params(void);

/**
 * @brief 触发自动增益调节。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_auto_gain_adjust(void);

/**
 * @brief 开始自动门限生成。
 *
 * @param trig_coef_10x   触发门限系数（10 倍放大）。
 * @param hold_coef_10x   保持门限系数（10 倍放大）。
 * @param static_coef_10x 微动门限系数（10 倍放大）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_start_auto_threshold(uint16_t trig_coef_10x,
	uint16_t hold_coef_10x, uint16_t static_coef_10x);

/**
 * @brief 查询自动门限生成进度。
 *
 * @param progress_percent 输出进度百分比。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_get_auto_threshold_progress(uint16_t *progress_percent);

/**
 * @brief 查询自动门限干扰状态。
 *
 * @param alarm_status 输出干扰状态码。
 * @param gate_bitmap  输出受影响距离门位图。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_get_auto_threshold_alarm(uint16_t *alarm_status, uint16_t *gate_bitmap);

/**
 * @brief 读取电源干扰参数。
 *
 * @param value 输出值。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_get_power_interference(uint32_t *value);

/**
 * @brief 执行 0x003F 读后回写流程。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ld2402_refresh_save_flag(void);

/**
 * @brief 设置 LD2402 运行态日志开关。
 *
 * @param enable 1=开启，0=关闭。
 * @return errcode_t ERRCODE_SUCC 成功。
 */
errcode_t ld2402_set_data_log_enable(uint8_t enable);

/**
 * @brief 获取 LD2402 运行态日志开关状态。
 *
 * @return uint8_t 1=开启，0=关闭。
 */
uint8_t ld2402_get_data_log_enable(void);

/**
 * @brief 设置 LD2402 运行态日志最小输出间隔。
 *
 * @param gap_ms 间隔毫秒，0 表示不做时间节流。
 * @return errcode_t ERRCODE_SUCC 成功。
 */
errcode_t ld2402_set_data_log_gap_ms(uint32_t gap_ms);

/**
 * @brief 获取 LD2402 运行态日志最小输出间隔。
 *
 * @return uint32_t 间隔毫秒。
 */
uint32_t ld2402_get_data_log_gap_ms(void);

/**
 * @brief 获取最近一次解析到的 LD2402 距离值。
 *
 * @return int32_t 最近一次距离值；若尚未解析到有效数据，则返回 -1。
 */
int32_t ld2402_get_last_distance_mm(void);

/**
 * @brief 获取最近一次有效距离值的时间戳（毫秒）。
 *
 * @return uint32_t 最近一次更新时的系统 Tick 毫秒值。
 */
uint32_t ld2402_get_last_distance_tick_ms(void);

#endif
