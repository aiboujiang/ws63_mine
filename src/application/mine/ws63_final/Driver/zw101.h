/**
 * @file zw101.h
 * @brief ZW101 指纹模组驱动层接口（基于《指纹模组产品用户手册_V1.5.1》）。
 *
 * 说明：
 * 1) 本文件仅定义 Driver 层协议语义接口；
 * 2) App/Task 层通过封装接口调用，不直接处理协议帧细节；
 * 3) 本次重点实现 ZA 协议兼容命令用于在线调试，业务/维护/定制命令先提供函数实现。
 */

#ifndef ZW101_H
#define ZW101_H

#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief ZW101 应答结果。
 */
typedef struct {
    uint8_t ack_code;
    uint8_t payload[64];
    uint16_t payload_len;
} zw101_ack_result_t;

/**
 * @brief 初始化 ZW101 驱动并执行通信探测。
 *
 * @param sub_port 对应的 WK2114 子串口号。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_init(uint8_t sub_port);

/**
 * @brief 查询 ZW101 驱动是否已完成初始化。
 *
 * @return uint8_t 1=就绪，0=未就绪。
 */
uint8_t zw101_is_ready(void);

/**
 * @brief 喂入 ZW101 接收数据流。
 *
 * @param sub_port 子串口号。
 * @param data 接收缓冲区。
 * @param len 接收长度。
 */
void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief 发送 ZW101 原始命令帧（十六进制字节流）。
 *
 * @param data 命令帧数据。
 * @param len  命令帧长度。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_send_raw(const uint8_t *data, uint16_t len);

/* ------------------------- ZA 协议兼容命令（调试重点） ------------------------- */

/**
 * @brief ZA 兼容握手 GetEcho（0x53）。
 *
 * @param ack_out 输出确认码，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_za_get_echo(uint8_t *ack_out);

/**
 * @brief ZA 自动登记 AutoLogin（0x54）。
 *
 * @param wait_time 待指时长（1~255）。
 * @param sample_interval_code 采样间隔编码（0~15，对应高 4bit）。
 * @param press_times 按指次数（2 或 3，对应低 4bit）。
 * @param page_id 存储序号。
 * @param allow_dup 重复登记标志（0=不允许，1=允许）。
 * @param ack_out 输出确认码，可为 NULL。
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t zw101_za_auto_login(uint8_t wait_time,
    uint8_t sample_interval_code,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out);

/**
 * @brief ZA 自动搜索 AutoSearch（0x55）。
 */
errcode_t zw101_za_auto_search(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    zw101_ack_result_t *result_out);

/**
 * @brief ZA 搜索指纹（带残留判断）SearchResBack（0x56）。
 */
errcode_t zw101_za_search_res_back(uint8_t buffer_id,
    uint16_t start_page,
    uint16_t page_num,
    zw101_ack_result_t *result_out);

/**
 * @brief ZA 自动登记（灯常亮）AutoLoginStabLight（0x57）。
 */
errcode_t zw101_za_auto_login_stab_light(uint8_t wait_time,
    uint8_t press_times,
    uint16_t page_id,
    uint8_t allow_dup,
    uint8_t *ack_out);

/**
 * @brief ZA 自动搜索（搜前提示）AutoSearchWithEcho（0x58）。
 */
errcode_t zw101_za_auto_search_with_echo(uint8_t wait_time,
    uint16_t start_page,
    uint16_t page_num,
    zw101_ack_result_t *result_out);

/**
 * @brief ZA 过程终止 ProcessTerminateCmd（0xAA）。
 */
errcode_t zw101_za_process_terminate(uint8_t *ack_out);

/* ------------------------- 业务类指令集（先实现，暂不主动调用） ------------------------- */

errcode_t zw101_business_get_image(void);
errcode_t zw101_business_gen_char(uint8_t buffer_id);
errcode_t zw101_business_match(uint16_t *score_out);
errcode_t zw101_business_search(uint8_t buffer_id,
    uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out);
errcode_t zw101_business_reg_model(void);
errcode_t zw101_business_store_char(uint8_t buffer_id, uint16_t page_id);
errcode_t zw101_business_load_char(uint8_t buffer_id, uint16_t page_id);
errcode_t zw101_business_up_char(uint8_t buffer_id);
errcode_t zw101_business_down_char(uint8_t buffer_id);
errcode_t zw101_business_delete_char(uint16_t page_id, uint16_t count);
errcode_t zw101_business_empty(void);
errcode_t zw101_business_write_reg(uint8_t reg_index, uint8_t reg_value);
errcode_t zw101_business_read_syspara(uint8_t *syspara_out, uint16_t *out_len);
errcode_t zw101_business_read_infpage(void);
errcode_t zw101_business_read_valid_template_num(uint16_t *valid_num_out);
errcode_t zw101_business_read_index_table(uint8_t table_index);
errcode_t zw101_business_get_enroll_image(void);
errcode_t zw101_business_read_add_para(uint8_t *add_para_out, uint16_t *out_len);
errcode_t zw101_business_sleep(void);
errcode_t zw101_business_write_empara(uint16_t em_para);
errcode_t zw101_business_cancel(void);
errcode_t zw101_business_auto_enroll(uint16_t page_id, uint8_t enroll_times, uint16_t param_flags);
errcode_t zw101_business_auto_identify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out);

/* ------------------------- 维护类指令集（先实现，暂不主动调用） ------------------------- */

errcode_t zw101_maint_up_image_4bit(void);
errcode_t zw101_maint_up_image_8bit(void);
errcode_t zw101_maint_down_image_4bit(void);
errcode_t zw101_maint_down_image_8bit(void);
errcode_t zw101_maint_get_chip_sn(uint8_t *chip_sn_out, uint16_t *out_len);
errcode_t zw101_maint_handshake(uint8_t *ack_out);
errcode_t zw101_maint_check_sensor(uint8_t *ack_out);
errcode_t zw101_maint_reset_setting(void);

/* ------------------------- 定制类指令集（先实现，暂不主动调用） ------------------------- */

errcode_t zw101_custom_set_pwd(uint32_t pwd);
errcode_t zw101_custom_verify_pwd(uint32_t pwd);
errcode_t zw101_custom_set_chip_addr(uint32_t chip_addr);
errcode_t zw101_custom_write_notepad(uint8_t page_id, const uint8_t *data, uint8_t data_len);
errcode_t zw101_custom_read_notepad(uint8_t page_id, uint8_t *data_out, uint16_t *out_len);
errcode_t zw101_custom_bln_auto_manual_switch(uint8_t mode);
errcode_t zw101_custom_control_bln(uint8_t func_code,
    uint8_t start_color,
    uint8_t end_color_or_duty,
    uint8_t loop_times,
    uint8_t cycle);
errcode_t zw101_custom_get_image_info(uint8_t *area_out, uint8_t *quality_out);
errcode_t zw101_custom_search_now(uint16_t start_page,
    uint16_t page_num,
    uint16_t *page_id_out,
    uint16_t *score_out);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
