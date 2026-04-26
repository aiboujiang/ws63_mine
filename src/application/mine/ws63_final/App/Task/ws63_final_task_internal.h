/**
 * @file ws63_final_task_internal.h
 * @brief Task 层内部模块共享接口（仅供 App/Task 内部 C 文件使用）。
 */

#ifndef WS63_FINAL_TASK_INTERNAL_H
#define WS63_FINAL_TASK_INTERNAL_H

#include <stdint.h>
#include "ws63_final_config.h"
#include "ws63_final_task.h"
#include "ws63_final_task_debug.h"

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
	WS63_LOCK_AUTH_SOURCE_VK36N16I,
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
 * @brief 启动 VK36N16I 独立任务。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_vk36n16i_task_start(void);

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
 * @brief 向 SLE 发送门锁业务事件文本（[LOCK] 标签）。
 *
 * 说明：门锁结果事件属于业务关键上报，不受 DEBUG INIT 门控。
 *
 * @param event_text 事件文本，建议使用 key=value;key=value 格式。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_post_lock_event_text(const char *event_text);

/**
 * @brief 确保 ZW101 子口已完成惰性初始化。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_ensure_zw101_ready(void);
uint8_t ws63_task_zw101_is_ready(void);

/**
 * @brief 确保 camera 子口已完成惰性初始化。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_ensure_camera_ready(void);
uint8_t ws63_camera_is_hw_ready(void);

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
errcode_t ws63_lock_mgr_report_auth_result(ws63_lock_auth_source_t source, uint8_t passed, uint8_t ack_code);

/**
 * @brief 更新最近一次指纹认证通过详情。
 *
 * @param match_id 指纹匹配 ID。
 * @param score 指纹匹配分数。
 */
void ws63_lock_mgr_update_finger_result(uint16_t match_id, uint16_t score);

/**
 * @brief 更新最近一次 camera 认证通过标签。
 *
 * @param label camera 识别标签，可为空字符串。
 */
void ws63_lock_mgr_update_camera_label(const char *label);

/**
 * @brief 查询门锁当前是否处于接近唤醒窗口。
 *
 * @return uint8_t 1=正在接近唤醒，0=未唤醒。
 */
uint8_t ws63_lock_mgr_is_armed(void);

/**
 * @brief 查询门锁接近唤醒窗口当前截止时间。
 *
 * 说明：用于按键侧输出“续命前/后”调试日志，不改变门锁状态机语义。
 *
 * @return uint32_t 当前截止时间（毫秒 Tick）；未进入窗口时可能为 0。
 */
uint32_t ws63_lock_mgr_get_auth_window_deadline_ms(void);

/**
 * @brief 刷新门锁接近唤醒窗口的超时时间。
 *
 * 说明：仅在门锁处于 ARMED 状态时生效，用于摄像头、按键、指纹和雷达输入续命。
 *
 * @return errcode_t ERRCODE_SUCC 已刷新，其他失败。
 */
errcode_t ws63_lock_mgr_refresh_auth_window(void);

/**
 * @brief 启动 ZW101 VERIFY 任务。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_zw101_task_start(void);

/**
 * @brief 请求 ZW101 VERIFY 任务执行一次认证。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_zw101_request_verify(void);

/**
 * @brief 请求 ZW101 在检测到手指离开后再执行一次 VERIFY。
 *
 * 说明：用于认证失败后的重试节流，避免手指未离开时立即重入认证。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_zw101_request_verify_after_release(void);

/**
 * @brief 重置 ZW101 在当前 ARMED 窗口的失败禁用状态。
 *
 * 说明：用于每次新进入 ARMED 时清空上一个窗口的禁用与失败计数。
 */
void ws63_task_zw101_reset_armed_window_guard(void);

/**
 * @brief 取消尚未开始的 ZW101 VERIFY 请求。
 */
void ws63_task_zw101_cancel_verify_request(void);

/**
 * @brief 获取最近一次 ZW101 VERIFY 返回的 ACK。
 *
 * @return uint8_t 最近 ACK 码，0xFF 表示暂无有效记录。
 */
uint8_t ws63_task_zw101_get_last_verify_ack(void);

/**
 * @brief 查询 VK36N16I 是否已在当前 armed 周期内被失败封禁。
 *
 * @return uint8_t 1=已封禁，0=仍可继续输入。
 */
uint8_t ws63_task_vk36n16i_is_password_disabled(void);

/**
 * @brief camera 任务发送文本消息。
 *
 * @param payload 不含前缀的业务文本，例如 `action` 或 `Die`。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_camera_send_message(const char *payload);

/**
 * @brief 设置 LD2402 子口通道使能状态。
 *
 * @param enable 1=启用，0=关闭。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_task_ld2402_set_channel_enable(uint8_t enable);

/**
 * @brief 查询 LD2402 子口通道是否启用。
 *
 * @return uint8_t 1=启用，0=关闭。
 */
uint8_t ws63_task_ld2402_is_channel_enabled(void);

/**
 * @brief camera 子口接收回调。
 */
void ws63_task_camera_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

#endif