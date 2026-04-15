/**
 * @file ws63_final_task.h
 * @brief WK2114 ×îÖÕ°æÓ¦ÓÃÈÎÎñ²ã½Ó¿Ú¡£
 */

#ifndef WS63_TASK_H
#define WS63_TASK_H


/**
 * @brief È¡Ïûµ±Ç°¿ÉÄÜÕıÔÚÖ´ĞĞµÄ ZW101 VERIFY Á÷³Ì¡£
 *
 * ËµÃ÷£ºÏÈÇåÀíÈÎÎñ²ã¹ÒÆğÇëÇó£¬ÔÙÏÂ·¢Ğ­Òé²ã CANCEL£¬¾¡Á¿Í¬Ê±¸²¸Ç¡°ÉĞÎ´¿ªÊ¼¡±ºÍ¡°ÒÑ¾­½øÈëÉè±¸²à¡±µÄÁ½ÖÖÇé¿ö¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_cancel_active_request(void);
#include <stdint.h>

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief ×Ó´®¿Ú½ÓÊÕ»Øµ÷¡£
 *
 * @param sub_port ×Ó´®¿ÚºÅ£¨1~4£©¡£
 * @param data     Êı¾İ»º³åÇø¡£
 * @param len      Êı¾İ³¤¶È¡£
 */
typedef void (*ws63_rx_callback_t)(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief ×¢²á×Ó´®¿Ú½ÓÊÕ»Øµ÷¡£
 *
 * @param sub_port ×Ó´®¿ÚºÅ£¨1~4£©¡£
 * @param callback »Øµ÷º¯Êı£¬¿ÉÎª NULL£¨±íÊ¾×¢Ïú£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_register_rx_callback(uint8_t sub_port,
    ws63_rx_callback_t callback);

/**
 * @brief Í¨¹ı WK2114 ×Ó´®¿Ú·¢ËÍÊı¾İ¡£
 *
 * @param sub_port ×Ó´®¿ÚºÅ£¨1~4£©¡£
 * @param data     Êı¾İ»º³åÇø¡£
 * @param len      Êı¾İ³¤¶È¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_send(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief ¿ØÖÆµç»úÕı×ª£¨IA=0£¬IB=PWM£©¡£
 *
 * @param duty_percent Õ¼¿Õ±È°Ù·Ö±È£¨0~100£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_motor_forward(uint8_t duty_percent);

/**
 * @brief ¿ØÖÆµç»ú·´×ª£¨IA=PWM£¬IB=0£©¡£
 *
 * @param duty_percent Õ¼¿Õ±È°Ù·Ö±È£¨0~100£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_motor_reverse(uint8_t duty_percent);

/**
 * @brief µç»úÍ£Ö¹£¨»¬ĞĞ£¬IA=0£¬IB=0£©¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_motor_coast_stop(void);

/**
 * @brief µç»úÉ²³µ£¨¼±Í££¬IA=1£¬IB=1£©¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_motor_brake_stop(void);

/**
 * @brief ¶¯Ì¬µ÷Õûµ±Ç°ÔËĞĞ·½ÏòÕ¼¿Õ±È¡£
 *
 * @param duty_percent Õ¼¿Õ±È°Ù·Ö±È£¨0~100£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_motor_set_duty(uint8_t duty_percent);

/**
 * @brief ´ò¿ª·äÃùÆ÷²¢ÉèÖÃÆµÂÊ¡£
 *
 * @param freq_hz Ä¿±êÆµÂÊ£¨Hz£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_buzzer_on(uint16_t freq_hz);

/**
 * @brief ²¥·ÅÒ»´Î¶Ì´ÙÌáÊ¾Òô¡£
 *
 * @param freq_hz Ä¿±êÆµÂÊ£¨Hz£©¡£
 * @param volume_percent Ä¿±êÒôÁ¿£¨Õ¼¿Õ±È°Ù·Ö±È£©¡£
 * @param duration_ms ³ÖĞøÊ±¼ä£¨ºÁÃë£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_buzzer_beep_tone(uint16_t freq_hz, uint8_t volume_percent, uint32_t duration_ms);

/**
 * @brief ¹Ø±Õ·äÃùÆ÷¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_buzzer_off(void);

/**
 * @brief ²éÑ¯·äÃùÆ÷ÊÇ·ñÕıÔÚ·¢Éù¡£
 *
 * @return uint8_t 1=ÕıÔÚ·¢Éù£¬0=¾²Òô¡£
 */
uint8_t ws63_task_buzzer_is_on(void);

/**
 * @brief »ñÈ¡·äÃùÆ÷µ±Ç°ÆµÂÊ¡£
 *
 * @return uint16_t µ±Ç°ÆµÂÊ£¨Hz£©¡£
 */
uint16_t ws63_task_buzzer_get_freq_hz(void);

/**
 * @brief ÉèÖÃ·äÃùÆ÷ÒôÁ¿¡£
 *
 * @param volume_percent ÒôÁ¿°Ù·Ö±È£¨0~100£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_buzzer_set_volume(uint8_t volume_percent);

/**
 * @brief »ñÈ¡·äÃùÆ÷µ±Ç°ÒôÁ¿¡£
 *
 * @return uint8_t µ±Ç°ÒôÁ¿°Ù·Ö±È¡£
 */
uint8_t ws63_task_buzzer_get_volume(void);

/**
 * @brief ÖØĞÂ³õÊ¼»¯ RGB Çı¶¯²¢»Ö¸´ÑİÊ¾Ä£Ê½¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_rgb_reinit(void);

/**
 * @brief ÉèÖÃ RGB ÑÕÉ«£¨8bit Í¨µÀ£©¡£
 *
 * @param r ºìÉ«·ÖÁ¿£¨0~255£©¡£
 * @param g ÂÌÉ«·ÖÁ¿£¨0~255£©¡£
 * @param b À¶É«·ÖÁ¿£¨0~255£©¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_rgb_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief ¹Ø±Õ RGB£¨Êä³öºÚÉ«£©¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_rgb_off(void);

/**
 * @brief ÉèÖÃ RGB ÑİÊ¾Ä£Ê½¿ª¹Ø¡£
 *
 * @param enable 1=¿ªÆôÑİÊ¾£¬0=¹Ø±ÕÑİÊ¾¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_rgb_set_demo_enable(uint8_t enable);

/**
 * @brief ²éÑ¯ RGB Çı¶¯ÊÇ·ñÒÑ¾ÍĞ÷¡£
 *
 * @return uint8_t 1=¾ÍĞ÷£¬0=Î´¾ÍĞ÷¡£
 */
uint8_t ws63_task_rgb_is_ready(void);

/**
 * @brief ²éÑ¯ RGB ÑİÊ¾Ä£Ê½ÊÇ·ñ¿ªÆô¡£
 *
 * @return uint8_t 1=¿ªÆô£¬0=¹Ø±Õ¡£
 */
uint8_t ws63_task_rgb_is_demo_enable(void);

/**
 * @brief ÖØĞÂ³õÊ¼»¯ LD2402¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_ld2402_reinit(void);

/**
 * @brief ²éÑ¯ LD2402 ÊÇ·ñÒÑÍê³É³õÊ¼»¯¡£
 *
 * @return uint8_t 1=ÒÑ¾ÍĞ÷£¬0=Î´¾ÍĞ÷¡£
 */
uint8_t ws63_task_ld2402_is_ready(void);

/**
 * @brief ²éÑ¯ LD2402 µ±Ç°ÊÇ·ñ´¦ÓÚÅäÖÃÄ£Ê½¡£
 *
 * @return uint8_t 1=ÊÇ£¬0=·ñ¡£
 */
uint8_t ws63_task_ld2402_is_in_config_mode(void);

/**
 * @brief Ïò LD2402 ·¢ËÍÔ­Ê¼ÃüÁîÖ¡¡£
 *
 * @param data ÃüÁî»º³åÇø¡£
 * @param len  ÃüÁî³¤¶È¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_ld2402_send_raw(const uint8_t *data, uint16_t len);

/**
 * @brief ¶ÁÈ¡ LD2402 ¹Ì¼ş°æ±¾¡£
 */
errcode_t ws63_task_ld2402_get_version(char *buf, uint16_t buf_len);

/**
 * @brief ¶ÁÈ¡ LD2402 ×Ö·ûĞÎÊ½ĞòÁĞºÅ¡£
 */
errcode_t ws63_task_ld2402_get_sn_char(char *buf, uint16_t buf_len);

/**
 * @brief ¶ÁÈ¡ LD2402 Ê®Áù½øÖÆĞÎÊ½ĞòÁĞºÅ¡£
 */
int32_t ws63_task_ld2402_get_sn_hex(uint8_t *buf, uint16_t buf_len);

/**
 * @brief ¶ÁÈ¡ LD2402 µ¥¸ö²ÎÊıÖµ¡£
 */
errcode_t ws63_task_ld2402_read_param(uint16_t param_id, uint32_t *value);

/**
 * @brief Ğ´Èë LD2402 µ¥¸ö²ÎÊıÖµ¡£
 */
errcode_t ws63_task_ld2402_set_param(uint16_t param_id, uint32_t value);

/**
 * @brief ÉèÖÃ LD2402 ×î´ó¾àÀë¡£
 */
errcode_t ws63_task_ld2402_set_max_distance(float distance_m);

/**
 * @brief ÉèÖÃ LD2402 Ä¿±êÏûÊ§ÑÓ³Ù¡£
 */
errcode_t ws63_task_ld2402_set_disappear_delay(uint16_t seconds);

/**
 * @brief ÇĞ»» LD2402 µ½Õı³£Ä£Ê½¡£
 */
errcode_t ws63_task_ld2402_set_normal_mode(void);

/**
 * @brief ÇĞ»» LD2402 µ½¹¤³ÌÄ£Ê½¡£
 */
errcode_t ws63_task_ld2402_set_engineering_mode(void);

/**
 * @brief ±£´æ LD2402 µ±Ç°²ÎÊı¡£
 */
errcode_t ws63_task_ld2402_save_params(void);

/**
 * @brief ´¥·¢ LD2402 ×Ô¶¯ÔöÒæµ÷½Ú¡£
 */
errcode_t ws63_task_ld2402_auto_gain_adjust(void);

/**
 * @brief ¿ªÊ¼ LD2402 ×Ô¶¯ÃÅÏŞÉú³É¡£
 */
errcode_t ws63_task_ld2402_start_auto_threshold(uint16_t trig_coef_10x,
    uint16_t hold_coef_10x, uint16_t static_coef_10x);

/**
 * @brief ²éÑ¯ LD2402 ×Ô¶¯ÃÅÏŞÉú³É½ø¶È¡£
 */
errcode_t ws63_task_ld2402_get_auto_threshold_progress(uint16_t *progress_percent);

/**
 * @brief ²éÑ¯ LD2402 ×Ô¶¯ÃÅÏŞ¸ÉÈÅ×´Ì¬¡£
 */
errcode_t ws63_task_ld2402_get_auto_threshold_alarm(uint16_t *alarm_status, uint16_t *gate_bitmap);

/**
 * @brief ¶ÁÈ¡ LD2402 µçÔ´¸ÉÈÅ²ÎÊı¡£
 */
errcode_t ws63_task_ld2402_get_power_interference(uint32_t *value);

/**
 * @brief Ö´ĞĞ LD2402 0x003F ¶Áºó»ØĞ´Á÷³Ì¡£
 */
errcode_t ws63_task_ld2402_refresh_save_flag(void);

/**
 * @brief ÉèÖÃ LD2402 ÔËĞĞÌ¬ÈÕÖ¾¿ª¹Ø¡£
 *
 * @param enable 1=¿ªÆô£¬0=¹Ø±Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_ld2402_set_log_enable(uint8_t enable);

/**
 * @brief »ñÈ¡ LD2402 ÔËĞĞÌ¬ÈÕÖ¾¿ª¹Ø¡£
 *
 * @return uint8_t 1=¿ªÆô£¬0=¹Ø±Õ¡£
 */
uint8_t ws63_task_ld2402_get_log_enable(void);

/**
 * @brief ÉèÖÃ LD2402 ÔËĞĞÌ¬ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 *
 * @param gap_ms ¼ä¸ôºÁÃë£¬0 ±íÊ¾Ã¿°ü¶¼Êä³ö¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_ld2402_set_log_gap_ms(uint32_t gap_ms);

/**
 * @brief »ñÈ¡ LD2402 ÔËĞĞÌ¬ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 *
 * @return uint32_t ¼ä¸ôºÁÃë¡£
 */
uint32_t ws63_task_ld2402_get_log_gap_ms(void);

/**
 * @brief »ñÈ¡×î½üÒ»´Î½âÎöµ½µÄ LD2402 ¾àÀëÖµ¡£
 *
 * @return int32_t ×î½üÒ»´Î¾àÀëÖµ£»ÈôÉĞÎ´½âÎöµ½ÓĞĞ§Êı¾İ£¬Ôò·µ»Ø -1¡£
 */
int32_t ws63_task_ld2402_get_distance_mm(void);

/**
 * @brief »ñÈ¡×î½üÒ»´ÎÓĞĞ§¾àÀëÖµµÄ¸üĞÂÊ±¼ä¡£
 *
 * @return uint32_t ×î½üÒ»´Î¸üĞÂÊ±µÄÏµÍ³ Tick ºÁÃëÖµ¡£
 */
uint32_t ws63_task_ld2402_get_distance_tick_ms(void);

/**
 * @brief ÉèÖÃ LD2402 ×Ó¿ÚÍ¨µÀÊ¹ÄÜ×´Ì¬¡£
 *
 * @param enable 1=ÆôÓÃÍ¨µÀ£¬0=¹Ø±ÕÍ¨µÀ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_ld2402_set_channel_enable(uint8_t enable);

/**
 * @brief ²éÑ¯ LD2402 ×Ó¿ÚÍ¨µÀÊÇ·ñÆôÓÃ¡£
 *
 * @return uint8_t 1=ÆôÓÃ£¬0=¹Ø±Õ¡£
 */
uint8_t ws63_task_ld2402_is_channel_enabled(void);

/**
 * @brief ÉèÖÃ SLE ÉÏĞĞ success ÈÕÖ¾¿ª¹Ø¡£
 *
 * @param enable 1=¿ªÆô£¬0=¹Ø±Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_sle_uplink_log_set_enable(uint8_t enable);

/**
 * @brief »ñÈ¡ SLE ÉÏĞĞ success ÈÕÖ¾¿ª¹Ø¡£
 *
 * @return uint8_t 1=¿ªÆô£¬0=¹Ø±Õ¡£
 */
uint8_t ws63_task_sle_uplink_log_get_enable(void);

/**
 * @brief ÉèÖÃ SLE ÉÏĞĞ success ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 *
 * @param gap_ms ¼ä¸ôºÁÃë£¬0 ±íÊ¾Ã¿´Î success ¶¼´òÓ¡¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_sle_uplink_log_set_gap_ms(uint32_t gap_ms);

/**
 * @brief »ñÈ¡ SLE ÉÏĞĞ success ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 *
 * @return uint32_t ¼ä¸ôºÁÃë¡£
 */
uint32_t ws63_task_sle_uplink_log_get_gap_ms(void);

/**
 * @brief Í¨¹ı SLE ÏòÖ÷»ú²à·¢ËÍµ÷ÊÔÈÕÖ¾ÎÄ±¾¡£
 *
 * @param data ÈÕÖ¾ÎÄ±¾Êı¾İ¡£
 * @param len  ÈÕÖ¾³¤¶È¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_send_debug_log_to_host(const uint8_t *data, uint16_t len);

/**
 * @brief ÖØĞÂ³õÊ¼»¯ ZW101£¨´¥·¢ÎÕÊÖ¼ì²â£©¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_reinit(void);

/**
 * @brief Ö´ĞĞ ZW101 ECHO ÃüÁî¡£
 *
 * @param ack_out Êä³ö ACK Âë£¬¿ÉÎª¿Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_echo(uint8_t *ack_out);

/**
 * @brief Ö´ĞĞ ZW101 ×Ô¶¯ÑéÖ¤£¨VERIFY£©¡£
 *
 * @param score_level °²È«µÈ¼¶£¨1~5£©¡£
 * @param target_id Ä¿±ê ID£»0xFFFF ±íÊ¾ 1:N¡£
 * @param param_flags ²ÎÊıÎ»¡£
 * @param match_id_out Êä³öÆ¥Åä ID£¬¿ÉÎª¿Õ¡£
 * @param score_out Êä³öÆ¥Åä·ÖÊı£¬¿ÉÎª¿Õ¡£
 * @param ack_out Êä³ö ACK Âë£¬¿ÉÎª¿Õ¡£
 * @return errcode_t ERRCODE_SUCC=ÈÏÖ¤Í¨¹ı£¬ÆäËû=ÈÏÖ¤Ê§°Ü¡£
 */
errcode_t ws63_task_zw101_verify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out,
    uint8_t *ack_out);

/**
 * @brief Ö´ĞĞ ZW101 ×Ô¶¯×¢²á£¨ENROLL£©¡£
 *
 * @param page_id Ä£°å ID¡£
 * @param enroll_times ²ÉÑù´ÎÊı£¨2~6£©¡£
 * @param param_flags ²ÎÊıÎ»¡£
 * @param ack_out Êä³ö ACK Âë£¬¿ÉÎª¿Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_enroll(uint16_t page_id,
    uint8_t enroll_times,
    uint16_t param_flags,
    uint8_t *ack_out);

/**
 * @brief ²éÑ¯ ZW101 ÓĞĞ§Ä£°åÊıÁ¿£¨LIST£©¡£
 *
 * @param valid_num_out Êä³öÓĞĞ§Ä£°åÊıÁ¿¡£
 * @param ack_out Êä³ö ACK Âë£¬¿ÉÎª¿Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_list(uint16_t *valid_num_out, uint8_t *ack_out);

/**
 * @brief è¯»å– ZW101 æœ‰æ•ˆæŒ‡çº¹ç´¢å¼•è¡¨ï¼ˆREAD_INDEX_TABLEï¼‰
 *
 * @param page ç´¢å¼•é¡µï¼Œ0è¡¨ç¤º0-255
 * @param index_buf_out è¾“å‡ºçš„32å­—èŠ‚buffer
 * @param ack_out è¾“å‡º ACKï¼Œå¯ä¸º NULLã€‚
 * @return errcode_t ERRCODE_SUCC æˆåŠŸï¼Œå¦åˆ™å¤±è´¥ã€‚
 */
errcode_t ws63_task_zw101_read_index_table(uint8_t page, uint8_t *index_buf_out, uint8_t *ack_out);

/**
 * @brief É¾³ı ZW101 Ä£°å£¨DEL£©¡£
 */
errcode_t ws63_task_zw101_delete(uint16_t page_id,
    uint16_t count,
    uint8_t *ack_out);

/**
 * @brief Çå¿Õ ZW101 Ä£°å¿â£¨CLEAR£©¡£
 *
 * @param ack_out Êä³ö ACK Âë£¬¿ÉÎª¿Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_clear(uint8_t *ack_out);

/**
 * @brief È¡Ïû ZW101 µ±Ç°Á÷³Ì£¨CANCEL£©¡£
 *
 * @param ack_out Êä³ö ACK Âë£¬¿ÉÎª¿Õ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_zw101_cancel(uint8_t *ack_out);

/**
 * @brief ´¥·¢ VK36N16I ×´Ì¬»úÖØ³õÊ¼»¯¡£
 *
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_vk36n16i_reinit(void);

/**
 * @brief ÉèÖÃ VK36N16I ×´Ì¬»úÊ¹ÄÜ¡£
 *
 * @param enable 1=ÆôÓÃ²ÉÑù£¬0=ÔİÍ£²ÉÑù¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_vk36n16i_set_enable(uint8_t enable);

/**
 * @brief ²éÑ¯ VK36N16I Çı¶¯ÊÇ·ñ¾ÍĞ÷¡£
 *
 * @return uint8_t 1=¾ÍĞ÷£¬0=Î´¾ÍĞ÷¡£
 */
uint8_t ws63_task_vk36n16i_is_ready(void);

/**
 * @brief ²éÑ¯ VK36N16I ×´Ì¬»úÊÇ·ñÆôÓÃ¡£
 *
 * @return uint8_t 1=ÆôÓÃ£¬0=¹Ø±Õ¡£
 */
uint8_t ws63_task_vk36n16i_is_enabled(void);

/**
 * @brief ÉèÖÃ VK36N16I ¶à¼ü±¨¾¯¿ª¹Ø¡£
 *
 * @param enable 1=¿ªÆô±¨¾¯£¬0=¹Ø±Õ±¨¾¯¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_vk36n16i_set_multi_key_alarm(uint8_t enable);

/**
 * @brief ²éÑ¯ VK36N16I ¶à¼ü±¨¾¯ÊÇ·ñ¿ªÆô¡£
 *
 * @return uint8_t 1=¿ªÆô£¬0=¹Ø±Õ¡£
 */
uint8_t ws63_task_vk36n16i_is_multi_key_alarm_enable(void);

/**
 * @brief ²éÑ¯µ±Ç°ÊÇ·ñ´¦ÓÚ¶à¼ü±¨¾¯×´Ì¬¡£
 *
 * @return uint8_t 1=´¦ÓÚ±¨¾¯£¬0=Î´±¨¾¯¡£
 */
uint8_t ws63_task_vk36n16i_is_multi_key_active(void);

/**
 * @brief »ñÈ¡ VK36N16I ×î½üÒ»´ÎÔ­Ê¼ 16 Î»Âë¡£
 *
 * @return uint16_t Ô­Ê¼ÂëÖµ¡£
 */
uint16_t ws63_task_vk36n16i_get_raw_code(void);

/**
 * @brief »ñÈ¡ VK36N16I ×î½üÒ»´Î°´ÏÂÑÚÂë£¨Î»1=°´ÏÂ£©¡£
 *
 * @return uint16_t °´ÏÂÑÚÂë¡£
 */
uint16_t ws63_task_vk36n16i_get_pressed_mask(void);

/**
 * @brief »ñÈ¡ VK36N16I ×î½üÒ»´Î°´ÏÂ°´¼üÊıÁ¿¡£
 *
 * @return uint8_t °´¼üÊıÁ¿£¨0~16£©¡£
 */
uint8_t ws63_task_vk36n16i_get_pressed_count(void);

/**
 * @brief Get the latest VK36N16I key label text.
 *
 * The text follows the measured board mapping and keeps multi-key presses as
 * additive labels, for example A+B when both keys are pressed.
 *
 * @param text Output buffer.
 * @param text_len Output buffer length.
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t ws63_task_vk36n16i_get_pressed_text(char *text, uint16_t text_len);

/**
 * @brief »ñÈ¡±àÂëÆ÷×îĞÂ RPM¡£
 *
 * @return int32_t ÓĞ·ûºÅ RPM¡£
 */
int32_t ws63_task_get_motor_rpm(void);

/**
 * @brief »ñÈ¡±àÂëÆ÷ÀÛ¼Æ¼ÆÊıÖµ¡£
 *
 * @return int32_t ÓĞ·ûºÅÀÛ¼ÆÂö³å¼ÆÊı¡£
 */
int32_t ws63_task_get_encoder_total_count(void);

/**
 * @brief WK2114 ×îÖÕ°æÒµÎñÈÎÎñÈë¿Ú¡£
 *
 * @param arg ÈÎÎñ²ÎÊı¡£
 * @return void* ¹Ì¶¨·µ»Ø NULL¡£
 */
void *ws63_task_entry(const char *arg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
