/**
 * @file ws63_final_task_internal.h
 * @brief Task 层内部模块共享接口（仅供 App/Task 内部 C 文件使用）。
 */

#ifndef WS63_FINAL_TASK_INTERNAL_H
#define WS63_FINAL_TASK_INTERNAL_H

#include <stdint.h>

#include "ws63_final_config.h"
#include "ws63_final_task.h"

#define WS63_SUBPORT_MAX 4U

/* WK2114 发送队列消息：统一封装“目标子口 + 数据”请求。 */
typedef struct {
	uint8_t sub_port;
	uint16_t len;
	uint8_t data[WS63_TASK_QUEUE_PAYLOAD_MAX];
} ws63_wk2114_tx_msg_t;

/* WK2114 上行到 SLE 的队列消息：带子口标识以便 SLE 打标签转发。 */
typedef struct {
	uint8_t sub_port;
	uint16_t len;
	uint8_t data[WS63_TASK_QUEUE_PAYLOAD_MAX];
} ws63_sle_uplink_msg_t;

/* RGB 控制命令：由调试命令/API 投递，RGB 任务串行执行。 */
typedef enum {
	WS63_RGB_CMD_REINIT = 0,
	WS63_RGB_CMD_SET_COLOR,
	WS63_RGB_CMD_SET_DEMO,
	WS63_RGB_CMD_OFF
} ws63_rgb_cmd_type_t;

typedef struct {
	ws63_rgb_cmd_type_t type;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t enable;
} ws63_rgb_ctrl_msg_t;

/* 蜂鸣器控制命令：通过消息队列串行下发到硬件任务。 */
typedef enum {
	WS63_BEEP_CMD_ON = 0,
	WS63_BEEP_CMD_OFF,
	WS63_BEEP_CMD_SET_VOLUME
} ws63_beep_cmd_type_t;

typedef struct {
	ws63_beep_cmd_type_t type;
	uint16_t freq_hz;
	uint8_t volume_percent;
} ws63_beep_ctrl_msg_t;

/* 门锁认证源：用于区分不同输入通道的认证结果。 */
typedef enum {
	WS63_LOCK_AUTH_SOURCE_CAMERA = 0,
	WS63_LOCK_AUTH_SOURCE_ZW101,
	WS63_LOCK_AUTH_SOURCE_TTP229,
	WS63_LOCK_AUTH_SOURCE_MANUAL
} ws63_lock_auth_source_t;

/**
 * @brief 初始化 RGB 演示链路。
 */
void ws63_rgb_demo_init(void);

/**
 * @brief 周期驱动 RGB 演示。
 *
 * @param now_ms 当前系统毫秒 Tick。
 */
void ws63_rgb_demo_process(uint32_t now_ms);

/**
 * @brief 启动 RGB 独立任务。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_rgb_task_start(void);

/**
 * @brief 初始化电机与编码器能力。
 */
void ws63_motor_encoder_init(void);

/**
 * @brief 查询电机/编码器能力是否可用。
 *
 * @return uint8_t 1=就绪，0=未就绪。
 */
uint8_t ws63_task_motor_encoder_is_ready(void);

/**
 * @brief 初始化蜂鸣器能力。
 */
void ws63_task_buzzer_init(void);

/**
 * @brief 启动蜂鸣器独立任务。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_beep_task_start(void);

/**
 * @brief 播放一次短促提示音。
 *
 * @param freq_hz 目标频率（Hz）。
 * @param volume_percent 目标音量（占空比百分比）。
 * @param duration_ms 持续时间（毫秒）。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_buzzer_beep_tone(uint16_t freq_hz, uint8_t volume_percent, uint32_t duration_ms);

/**
 * @brief 启动 TTP229 独立任务。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_ttp229_task_start(void);

/**
 * @brief 启动 camera 独立任务。
 */
errcode_t ws63_camera_task_start(void);

/**
 * @brief 启动门锁编排任务。
 */
errcode_t ws63_lock_mgr_task_start(void);

/**
 * @brief 向 WK2114 发送队列投递消息。
 */
errcode_t ws63_task_post_wk2114_tx(const ws63_wk2114_tx_msg_t *msg, uint32_t timeout);

/**
 * @brief 向 SLE 上行队列投递消息。
 */
errcode_t ws63_task_post_sle_uplink(const ws63_sle_uplink_msg_t *msg, uint32_t timeout);

/**
 * @brief 获取 WK2114 发送队列中的一条消息。
 */
errcode_t ws63_task_recv_wk2114_tx(ws63_wk2114_tx_msg_t *msg, uint32_t timeout);

/**
 * @brief 获取 SLE 上行队列中的一条消息。
 */
errcode_t ws63_task_recv_sle_uplink(ws63_sle_uplink_msg_t *msg, uint32_t timeout);

/**
 * @brief 查询 WK2114 驱动链路是否就绪。
 */
uint8_t ws63_task_wk2114_is_ready(void);

/**
 * @brief 门锁认证结果上报接口。
 *
 * @param source 认证来源。
 * @param passed 1=认证通过，0=认证失败。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_lock_mgr_report_auth_result(ws63_lock_auth_source_t source, uint8_t passed);

/**
 * @brief 查询门锁当前是否处于接近唤醒窗口。
 *
 * @return uint8_t 1=正在接近唤醒，0=未唤醒。
 */
uint8_t ws63_lock_mgr_is_armed(void);

/**
 * @brief camera 任务发送文本消息。
 *
 * @param payload 不含前缀的业务文本，例如 `action` 或 `Die`。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_camera_send_message(const char *payload);

/**
 * @brief camera 子口接收回调。
 */
void ws63_task_camera_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

#endif