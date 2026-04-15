/**
 * @file zw101.h
 * @brief ZW101 Ö¸ÎÆÄ£×éÇı¶¯½Ó¿Ú£¨ws63_final ÖØ¹¹°æ£©¡£
 *
 * Éè¼ÆËµÃ÷£º
 * 1) ½ö±£ÁôÃÅËøÒµÎñ±ØĞèÄÜÁ¦£ºENROLL/VERIFY/ECHO/LIST/DEL/CLEAR/CANCEL£»
 * 2) Ğ­ÒéÖ¡Ï¸½ÚÓë ACK ½âÎöÈ«²¿·â×°ÔÚ Driver ²ã£¬Task ²ãÖ»Ê¹ÓÃÓïÒå½Ó¿Ú£»
 * 3) VERIFY Ä¬ÈÏ²ÎÊıÓÉÉÏ²ã´«Èë£¬±ãÓÚÓë sle_uart_slave ĞĞÎª±£³ÖÒ»ÖÂ¡£
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
 * @brief ZW101 ACK ½á¹û¡£
 *
 * payload[0] ¹Ì¶¨Îª ACK Âë£¬ºóĞø×Ö½ÚÎªÃüÁîË½ÓĞ²ÎÊı¡£
 */
typedef struct {
    uint8_t ack_code;
    uint8_t payload[64];
    uint16_t payload_len;
} zw101_ack_result_t;

/**
 * @brief ³õÊ¼»¯ ZW101 Çı¶¯²¢Íê³ÉÎÕÊÖÌ½²â¡£
 *
 * @param sub_port WK2114 ×Ó´®¿ÚºÅ¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_init(uint8_t sub_port);

/**
 * @brief ²éÑ¯Çı¶¯ÊÇ·ñÒÑ¾ÍĞ÷¡£
 *
 * @return uint8_t 1=¾ÍĞ÷£¬0=Î´¾ÍĞ÷¡£
 */
uint8_t zw101_is_ready(void);

/**
 * @brief Î¹Èë×Ó´®¿Ú½ÓÊÕÊı¾İ£¨ÓÉ WK2114 ÂÖÑ¯»Øµ÷µ÷ÓÃ£©¡£
 *
 * @param sub_port ×Ó´®¿ÚºÅ¡£
 * @param data ½ÓÊÕ»º³åÇø¡£
 * @param len ½ÓÊÕ³¤¶È¡£
 */
void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len);

/**
 * @brief Ö´ĞĞ ECHO ÃüÁî£¨GetEcho 0x53£©¡£
 *
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ±íÊ¾Á´Â·¿É´ï£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_echo(uint8_t *ack_out);

/**
 * @brief ²éÑ¯µ±Ç°ÊÇ·ñÓĞÊÖÖ¸°´Ñ¹ÔÚ ZW101 ´«¸ĞÆ÷ÉÏ£¨PS_GetImageInfo 0x3D£©¡£
 *
 * ACK ÓïÒå£º0x00=ÓĞÊÖÖ¸£¬0x02=ÎŞÊÖÖ¸¡£
 *
 * @param finger_present_out Êä³ö°´Ñ¹×´Ì¬£º1=ÓĞÊÖÖ¸£¬0=ÎŞÊÖÖ¸¡£
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ±íÊ¾³É¹¦»ñÈ¡×´Ì¬£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_check_finger_present(uint8_t *finger_present_out, uint8_t *ack_out);

/**
 * @brief Ö´ĞĞ×Ô¶¯×¢²á£¨AutoEnroll 0x31£©¡£
 *
 * @param page_id Ä¿±êÄ£°å ID¡£
 * @param enroll_times ²ÉÑù´ÎÊı£¨2~6£©¡£
 * @param param_flags ²ÎÊıÎ»¡£
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ±íÊ¾×¢²á³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_enroll(uint16_t page_id, uint8_t enroll_times, uint16_t param_flags, uint8_t *ack_out);

/**
 * @brief Ö´ĞĞ×Ô¶¯ÑéÖ¤£¨AutoIdentify 0x32£©¡£
 *
 * @param score_level °²È«µÈ¼¶£¨1~5£©¡£
 * @param target_id Ä¿±ê ID£»0xFFFF ±íÊ¾ 1:N¡£
 * @param param_flags ²ÎÊıÎ»¡£
 * @param match_id_out Êä³öÆ¥Åä ID£¬¿ÉÎª NULL¡£
 * @param score_out Êä³öÆ¥Åä·ÖÊı£¬¿ÉÎª NULL¡£
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ±íÊ¾ÑéÖ¤Í¨¹ı£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_verify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out,
    uint8_t *ack_out);

/**
 * @brief ²éÑ¯ÓĞĞ§Ä£°åÊıÁ¿£¨0x1D£©¡£
 *
 * @param valid_num_out Êä³öÄ£°åÊıÁ¿¡£
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_list(uint16_t *valid_num_out, uint8_t *ack_out);

/**
 * @brief è¯»å–æœ‰æ•ˆæŒ‡çº¹ç´¢å¼•è¡¨ï¼ˆ0x1Få‘½ä»¤ï¼‰
 *
 * @param page ç´¢å¼•é¡µï¼Œ0è¡¨ç¤º0-255
 * @param index_buf_out å€Ÿç”¨çš„32å­—èŠ‚(256ä½)buffer
 * @param ack_out è¾“å‡º ACKï¼Œå¯ä¸º NULLã€‚
 * @return errcode_t ERRCODE_SUCC æˆåŠŸï¼Œå¦åˆ™å¤±è´¥ã€‚
 */
errcode_t zw101_read_index_table(uint8_t page, uint8_t *index_buf_out, uint8_t *ack_out);

/**
 * @brief É¾³ıÄ£°å£¨0x0C£©¡£
 *
 * @param page_id ÆğÊ¼Ä£°å ID¡£
 * @param count É¾³ıÊıÁ¿¡£
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_delete(uint16_t page_id, uint16_t count, uint8_t *ack_out);

/**
 * @brief Çå¿ÕÄ£°å¿â£¨0x0D£©¡£
 *
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_clear(uint8_t *ack_out);

/**
 * @brief È¡Ïûµ±Ç°Á÷³Ì£¨0x30£©¡£
 *
 * @param ack_out Êä³ö ACK£¬¿ÉÎª NULL¡£
 * @return errcode_t ERRCODE_SUCC ³É¹¦£¬ÆäËûÊ§°Ü¡£
 */
errcode_t zw101_cancel(uint8_t *ack_out);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
