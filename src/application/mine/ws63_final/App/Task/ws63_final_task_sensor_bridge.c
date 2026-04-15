/**
 * @file ws63_final_task_sensor_bridge.c
 * @brief Task ²ã´«¸ĞÆ÷ÇÅ½Ó×ÓÄ£¿é£¨LD2402/ZW101£©¡£
 */

#include "ws63_final_task_internal.h"

#include <stddef.h>
#include <string.h>

#include "osal_debug.h"
#include "securec.h"

#include "ws63_final_common.h"
#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_final_sle.h"
#include "wk2114.h"
#include "ld2402.h"
#include "zw101.h"

/* ZW101 VERIFY ÈÎÎñÇëÇó×´Ì¬£º¶ÀÁ¢ÓÚÃÅËøÖ÷×´Ì¬»ú£¬±ÜÃâ×èÈûÆäËûÈÏÖ¤Á´Â·¡£ */
static uint8_t g_ws63_zw101_task_started = 0U;
static uint8_t g_ws63_zw101_verify_requested = 0U;
static uint8_t g_ws63_zw101_verify_cancelled = 0U;
static uint8_t g_ws63_zw101_verify_wait_release = 0U;
static uint8_t g_ws63_zw101_verify_disabled = 0U;
static uint8_t g_ws63_zw101_verify_fail_streak = 0U;
/* ACK_TIMEOUT ¶ÀÁ¢ÖØÊÔ¼ÆÊı£ºÖ»Í³¼ÆÉúÃüÖÜÆÚÄÚµÄÍ¨ĞÅ³¬Ê±£¬²»ºÍÊ¶±ğÊ§°Ü»ìÓÃ¡£ */
static uint8_t g_ws63_zw101_verify_timeout_streak = 0U;
/* ¼ÇÂ¼×î½üÒ»´Î VERIFY µÄ ACK£¬ÓÃÓÚÖØÊÔ²ßÂÔ·ÖÖ§ÅĞ¶Ï¡£ */
static uint8_t g_ws63_zw101_last_verify_ack = 0xFFU;

/* Ä¬ÈÏ VERIFY ²ÎÊı£ºÓë sle_uart_slave ÒÑÑéÖ¤¿Ú¾¶±£³ÖÒ»ÖÂ¡£ */
#define WS63_ZW101_VERIFY_LEVEL_DEFAULT 3U
#define WS63_ZW101_VERIFY_ID_DEFAULT 0xFFFFU
#define WS63_ZW101_VERIFY_PARAM_DEFAULT 0x0000U

/* Á¬ĞøÊ§°Ü±£»¤£º´ïµ½ãĞÖµºó½ûÓÃ VERIFY ²¢ÉÏ±¨±¨¾¯ÏûÏ¢¡£ */
#define WS63_ZW101_VERIFY_FAIL_DISABLE_THRESHOLD 5U
/* ACK_TIMEOUT ÔÊĞíµÄÉúÃüÖÜÆÚÄÚ×Ô¶¯ÖØÀ­´ÎÊı£º±ÜÃâÁ´Â·Ë²Ì¬°ÑÒ»´ÎÈÏÖ¤Ö±½Ó´ò³ÉÖÕÌ¬Ê§°Ü¡£ */
#define WS63_ZW101_VERIFY_TIMEOUT_RETRY_THRESHOLD 3U
#define WS63_ZW101_ALARM_TEXT "ZW101 VERIFY DISABLED"
#define WS63_ZW101_READY_RETRY_GAP_MS 1000U
/* ÀëÊÖ¼ì²â²»ĞèÒª¸ßÆµÂÖÑ¯£¬ÊÊµ±·Å´ó¼ä¸ô¿ÉÏÔÖø½µµÍ´®¿ÚÓëÈÕÖ¾¸ºÔØ¡£ */
#define WS63_ZW101_RELEASE_CHECK_GAP_MS 500U

/* ÏêÏ¸×·×ÙÄ¬ÈÏ¹Ø±Õ£º±ÜÃâ VERIFY ÖØÊÔÁ´Â·ÔÚÕı³£³¡¾°Ë¢ÆÁ£¬ÅÅÕÏÊ±ÔÙÁÙÊ±´ò¿ª¡£ */
#define WS63_ZW101_TRACE_DETAIL_ENABLE 0U

/* ACK ÓïÒå³£Á¿£ºÓÃÓÚ±ÜÃâÖØÊÔ·ÖÖ§ÖĞµÄÄ§·¨Öµ¡£ */
#define WS63_ZW101_ACK_OK 0x00U
#define WS63_ZW101_ACK_NOT_PRESSED 0x09U
#define WS63_ZW101_ACK_TIMEOUT 0x26U
#define WS63_ZW101_ACK_UNKNOWN 0xFFU

/* ½ö¹©±¾ÎÄ¼şÄÚ²¿Ê¹ÓÃ£¬±ÜÃâ°Ñ ZW101 ÈÎÎñ¾ÍĞ÷²éÑ¯À©É¢µ½ÉÏ²ã½Ó¿Ú¡£ */
static uint8_t ws63_task_zw101_is_ready(void);


static void ws63_zw101_reset_timeout_retry_state(void);
static uint8_t ws63_zw101_try_retry_verify_after_timeout(void);

/**
 * @brief °Ñ³£¼û ACK Âë×ª»»Îª¿É¶ÁÎÄ±¾£¬±ãÓÚ´®¿ÚÈÕÖ¾¿ìËÙÅĞ¶ÏÊ§°ÜÀàĞÍ¡£
 */
static const char *ws63_zw101_ack_to_text(uint8_t ack_code)
{
    switch (ack_code) {
        case 0x00U:
            return "OK";
        case 0x08U:
            return "NO_MATCH";
        case 0x09U:
            return "NOT_PRESSED";
        case 0x24U:
            return "BAD_IMAGE";
        case 0x26U:
            return "ACK_TIMEOUT";
        default:
            return "OTHER";
    }
}

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
/**
 * @brief °´Ñ¹×´Ì¬ÎÄ±¾¡£
 */
static const char *ws63_zw101_finger_text(uint8_t finger_present)
{
    return (finger_present != 0U) ? "PRESSED" : "RELEASED";
}
#endif

/**
 * @brief ´òÓ¡ VERIFY ×´Ì¬¿ìÕÕ£¬Í³Ò»¹Û²ìÇëÇóÎ»/È¡ÏûÎ»/½ûÓÃÎ»±ä»¯¡£
 *
 * ËµÃ÷£º¶îÍâ´øÉÏ timeout ÖØÊÔ¼ÆÊı£¬±ãÓÚÏÖ³¡Çø·Ö¡°Ê¶±ğÊ§°Ü¡±ºÍ¡°Í¨ĞÅ³¬Ê±ÖØÀ­¡±¡£
 */
static void ws63_zw101_trace_verify_state(const char *tag)
{
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    unsigned int irq_status;
    uint8_t request;
    uint8_t cancelled;
    uint8_t disabled;
    uint8_t wait_release;
    uint8_t fail_streak;
    uint8_t timeout_streak;
    uint8_t armed;
    uint8_t ready;

    if (tag == NULL) {
        return;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    request = g_ws63_zw101_verify_requested;
    cancelled = g_ws63_zw101_verify_cancelled;
    disabled = g_ws63_zw101_verify_disabled;
    wait_release = g_ws63_zw101_verify_wait_release;
    fail_streak = g_ws63_zw101_verify_fail_streak;
    timeout_streak = g_ws63_zw101_verify_timeout_streak;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    armed = ws63_lock_mgr_is_armed();
    ready = ws63_task_zw101_is_ready();

    osal_printk("[zw101 trace] %s req=%u cancel=%u wait_release=%u disabled=%u fail_streak=%u timeout_retry=%u armed=%u ready=%u\r\n",
        tag,
        (unsigned int)request,
        (unsigned int)cancelled,
        (unsigned int)wait_release,
        (unsigned int)disabled,
        (unsigned int)fail_streak,
        (unsigned int)timeout_streak,
        (unsigned int)armed,
        (unsigned int)ready);
#else
    (void)tag;
#endif
}

/**
 * @brief ZW101 VERIFY ÈÎÎñ×´Ì¬Ëø¡£
 */


/**
 * @brief ZW101 VERIFY ÈÎÎñ×´Ì¬½âËø¡£
 */


/**
 * @brief Çå¿Õ ACK_TIMEOUT µÄ¶ÀÁ¢ÖØÊÔ¼ÆÊı¡£
 *
 * ËµÃ÷£º¸Ã¼ÆÊıÖ»·şÎñÓÚ¡°µ±Ç°ÉúÃüÖÜÆÚÄÚµÄÍ¨ĞÅ³¬Ê±ÖØÀ­¡±£¬Òò´ËÔÚĞÂ ARMED ´°¿Ú¡¢
 *       ÈÏÖ¤³É¹¦»ò³¬Ê±²ßÂÔ³¹µ×·ÅÆúÊ±¶¼ĞèÒªÍ¬²½ÇåÁã£¬±ÜÃâ°ÑÉÏÒ»ÂÖÀúÊ·´ø½øÏÂÒ»ÂÖ¡£
 */
static void ws63_zw101_reset_timeout_retry_state(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_verify_timeout_streak = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
}

/**
 * @brief ÔÚ ACK_TIMEOUT ÇÒÉúÃüÖÜÆÚÈÔÓĞĞ§Ê±£¬Á¢¼´ÖØÀ­Ò»´Î VERIFY¡£
 *
 * @return uint8_t 1=±¾´Î³¬Ê±ÒÑ×ª³ÉÄÚ²¿ÖØÊÔ£¬²»ÔÙÉÏ±¨ÃÅËøÊ§°Ü£»0=Ó¦¼ÌĞø×ßÊ§°ÜÉÏ±¨¡£
 */
static uint8_t ws63_zw101_try_retry_verify_after_timeout(void)
{
    unsigned int irq_status;
    uint8_t timeout_retry_streak;

    if (ws63_lock_mgr_is_armed() == 0U) {
        return 0U;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_zw101_verify_timeout_streak < 0xFFU) {
        g_ws63_zw101_verify_timeout_streak++;
    }
    timeout_retry_streak = g_ws63_zw101_verify_timeout_streak;

    if (timeout_retry_streak < WS63_ZW101_VERIFY_TIMEOUT_RETRY_THRESHOLD) {
        /*
         * Ö»ÓĞ¡°ÉúÃüÖÜÆÚÈÔÈ»»î×Å¡±Ê±²ÅÖØÀ­ VERIFY£ºÕâÑù ACK_TIMEOUT ²»»á°ÑÃÅËø
         * ÎóÍÆ½ø Die/lockout£¬¶øÊÇµ±³ÉÒ»´Î¿É»Ö¸´µÄÍ¨ĞÅÒì³£¼ÌĞø³¢ÊÔ¡£
         */
        g_ws63_zw101_verify_requested = 1U;
        g_ws63_zw101_verify_cancelled = 0U;
        g_ws63_zw101_verify_wait_release = 0U;
        g_ws63_zw101_last_verify_ack = WS63_ZW101_ACK_TIMEOUT;
        WS63_FINAL_IRQ_UNLOCK(irq_status);

        osal_printk("[zw101] VERIFY timeout, retry queued timeout_retry=%u/%u\r\n",
            (unsigned int)timeout_retry_streak,
            (unsigned int)WS63_ZW101_VERIFY_TIMEOUT_RETRY_THRESHOLD);
        ws63_zw101_trace_verify_state("verify_timeout_retry");
        return 1U;
    }

    /* ³¬Ê±ÖØÊÔºÄ¾¡ºó»ØÂäµ½ÆÕÍ¨Ê§°ÜÉÏ±¨£¬±£ÁôÏÖÓĞ lockout / ¸æ¾¯ÓïÒå¡£ */
    g_ws63_zw101_verify_timeout_streak = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    osal_printk("[zw101] VERIFY timeout retry exhausted, fall back to fail\r\n");
    return 0U;
}

/**
 * @brief ÖØĞÂ³õÊ¼»¯ LD2402¡£
 */
errcode_t ws63_task_ld2402_reinit(void)
{
    if (!ws63_is_subport_enabled(LD2402_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    return ld2402_init(LD2402_SUBPORT);
}

/**
 * @brief ²éÑ¯ LD2402 ÊÇ·ñÒÑÍê³É³õÊ¼»¯¡£
 */
uint8_t ws63_task_ld2402_is_ready(void)
{
    return ld2402_is_ready();
}

/**
 * @brief ²éÑ¯ LD2402 µ±Ç°ÊÇ·ñ´¦ÓÚÅäÖÃÄ£Ê½¡£
 */
uint8_t ws63_task_ld2402_is_in_config_mode(void)
{
    return ld2402_is_in_config_mode();
}

/**
 * @brief Ïò LD2402 ·¢ËÍÔ­Ê¼ÃüÁîÖ¡¡£
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
 * @brief ¶ÁÈ¡ LD2402 ¹Ì¼ş°æ±¾¡£
 */
errcode_t ws63_task_ld2402_get_version(char *buf, uint16_t buf_len)
{
    return ld2402_get_version(buf, buf_len);
}

/**
 * @brief ¶ÁÈ¡ LD2402 ×Ö·ûĞÎÊ½ĞòÁĞºÅ¡£
 */
errcode_t ws63_task_ld2402_get_sn_char(char *buf, uint16_t buf_len)
{
    return ld2402_get_sn_char(buf, buf_len);
}

/**
 * @brief ¶ÁÈ¡ LD2402 Ê®Áù½øÖÆĞÎÊ½ĞòÁĞºÅ¡£
 */
int32_t ws63_task_ld2402_get_sn_hex(uint8_t *buf, uint16_t buf_len)
{
    return ld2402_get_sn_hex(buf, buf_len);
}

/**
 * @brief ¶ÁÈ¡ LD2402 µ¥¸ö²ÎÊıÖµ¡£
 */
errcode_t ws63_task_ld2402_read_param(uint16_t param_id, uint32_t *value)
{
    return ld2402_read_param(param_id, value);
}

/**
 * @brief Ğ´Èë LD2402 µ¥¸ö²ÎÊıÖµ¡£
 */
errcode_t ws63_task_ld2402_set_param(uint16_t param_id, uint32_t value)
{
    return ld2402_set_param(param_id, value);
}

/**
 * @brief ÉèÖÃ LD2402 ×î´ó¾àÀë¡£
 */
errcode_t ws63_task_ld2402_set_max_distance(float distance_m)
{
    return ld2402_set_max_distance(distance_m);
}

/**
 * @brief ÉèÖÃ LD2402 Ä¿±êÏûÊ§ÑÓ³Ù¡£
 */
errcode_t ws63_task_ld2402_set_disappear_delay(uint16_t seconds)
{
    return ld2402_set_disappear_delay(seconds);
}

/**
 * @brief ÇĞ»» LD2402 µ½Õı³£Ä£Ê½¡£
 */
errcode_t ws63_task_ld2402_set_normal_mode(void)
{
    return ld2402_set_normal_mode();
}

/**
 * @brief ÇĞ»» LD2402 µ½¹¤³ÌÄ£Ê½¡£
 */
errcode_t ws63_task_ld2402_set_engineering_mode(void)
{
    return ld2402_set_engineering_mode();
}

/**
 * @brief ±£´æ LD2402 µ±Ç°²ÎÊı¡£
 */
errcode_t ws63_task_ld2402_save_params(void)
{
    return ld2402_save_params();
}

/**
 * @brief ´¥·¢ LD2402 ×Ô¶¯ÔöÒæµ÷½Ú¡£
 */
errcode_t ws63_task_ld2402_auto_gain_adjust(void)
{
    return ld2402_auto_gain_adjust();
}

/**
 * @brief ¿ªÊ¼ LD2402 ×Ô¶¯ÃÅÏŞÉú³É¡£
 */
errcode_t ws63_task_ld2402_start_auto_threshold(uint16_t trig_coef_10x,
    uint16_t hold_coef_10x, uint16_t static_coef_10x)
{
    return ld2402_start_auto_threshold(trig_coef_10x, hold_coef_10x, static_coef_10x);
}

/**
 * @brief ²éÑ¯ LD2402 ×Ô¶¯ÃÅÏŞÉú³É½ø¶È¡£
 */
errcode_t ws63_task_ld2402_get_auto_threshold_progress(uint16_t *progress_percent)
{
    return ld2402_get_auto_threshold_progress(progress_percent);
}

/**
 * @brief ²éÑ¯ LD2402 ×Ô¶¯ÃÅÏŞ¸ÉÈÅ×´Ì¬¡£
 */
errcode_t ws63_task_ld2402_get_auto_threshold_alarm(uint16_t *alarm_status, uint16_t *gate_bitmap)
{
    return ld2402_get_auto_threshold_alarm(alarm_status, gate_bitmap);
}

/**
 * @brief ¶ÁÈ¡ LD2402 µçÔ´¸ÉÈÅ²ÎÊı¡£
 */
errcode_t ws63_task_ld2402_get_power_interference(uint32_t *value)
{
    return ld2402_get_power_interference(value);
}

/**
 * @brief Ö´ĞĞ LD2402 0x003F ¶Áºó»ØĞ´Á÷³Ì¡£
 */
errcode_t ws63_task_ld2402_refresh_save_flag(void)
{
    return ld2402_refresh_save_flag();
}

/**
 * @brief ÉèÖÃ LD2402 ÔËĞĞÌ¬ÈÕÖ¾¿ª¹Ø¡£
 */
errcode_t ws63_task_ld2402_set_log_enable(uint8_t enable)
{
    return ld2402_set_data_log_enable(enable);
}

/**
 * @brief »ñÈ¡ LD2402 ÔËĞĞÌ¬ÈÕÖ¾¿ª¹Ø¡£
 */
uint8_t ws63_task_ld2402_get_log_enable(void)
{
    return ld2402_get_data_log_enable();
}

/**
 * @brief ÉèÖÃ LD2402 ÔËĞĞÌ¬ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 */
errcode_t ws63_task_ld2402_set_log_gap_ms(uint32_t gap_ms)
{
    return ld2402_set_data_log_gap_ms(gap_ms);
}

/**
 * @brief »ñÈ¡ LD2402 ÔËĞĞÌ¬ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 */
uint32_t ws63_task_ld2402_get_log_gap_ms(void)
{
    return ld2402_get_data_log_gap_ms();
}

/**
 * @brief »ñÈ¡ LD2402 ×î½üÒ»´Î½âÎöµ½µÄ¾àÀëÖµ¡£
 */
int32_t ws63_task_ld2402_get_distance_mm(void)
{
    return ld2402_get_last_distance_mm();
}

/**
 * @brief »ñÈ¡ LD2402 ×î½üÒ»´ÎÓĞĞ§¾àÀëÖµµÄ¸üĞÂÊ±¼ä¡£
 */
uint32_t ws63_task_ld2402_get_distance_tick_ms(void)
{
    return ld2402_get_last_distance_tick_ms();
}

/**
 * @brief ÉèÖÃ LD2402 ×Ó¿ÚÍ¨µÀÊ¹ÄÜ×´Ì¬¡£
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
 * @brief ²éÑ¯ LD2402 ×Ó¿ÚÍ¨µÀÊÇ·ñÆôÓÃ¡£
 */
uint8_t ws63_task_ld2402_is_channel_enabled(void)
{
    return ws63_is_subport_enabled(LD2402_SUBPORT);
}

/**
 * @brief ²éÑ¯ ZW101 ÊÇ·ñÒÑÍê³É³õÊ¼»¯¡£
 */
static uint8_t ws63_task_zw101_is_ready(void)
{
    return zw101_is_ready();
}

/**
 * @brief Í¨¹ı SLE ÉÏĞĞ¶ÓÁĞ·¢ËÍÒ»Ìõ ZW101 ±¨¾¯ÎÄ±¾¡£
 */
static void ws63_task_zw101_notify_alarm(const char *alarm_text)
{
    ws63_sle_uplink_msg_t msg = {0};
    uint16_t text_len;

    if (alarm_text == NULL) {
        return;
    }

    text_len = (uint16_t)strlen(alarm_text);
    if ((text_len == 0U) || (text_len > WS63_TASK_QUEUE_PAYLOAD_MAX)) {
        return;
    }

    msg.sub_port = ZW101_SUBPORT;
    msg.len = text_len;
    if (memcpy_s(msg.data, sizeof(msg.data), alarm_text, text_len) != EOK) {
        return;
    }

    (void)ws63_task_post_sle_uplink(&msg, WS63_OS_NO_WAIT);
}

/**
 * @brief ÇëÇó ZW101 VERIFY ÈÎÎñÖ´ĞĞÒ»´ÎÈÏÖ¤¡£
 */
errcode_t ws63_task_zw101_request_verify(void)
{
    unsigned int irq_status;

    if (g_ws63_zw101_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_zw101_verify_disabled != 0U) {
        WS63_FINAL_IRQ_UNLOCK(irq_status);
        return ERRCODE_FAIL;
    }

    g_ws63_zw101_verify_requested = 1U;
    g_ws63_zw101_verify_cancelled = 0U;
    g_ws63_zw101_verify_wait_release = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("request_verify");

    /* VERIFY ÇëÇóÒ»µ©Èë¶Ó£¬¾ÍË¢ĞÂÈÏÖ¤´°¿Ú£¬¸øºóĞøµÈ´ı ACK Ô¤ÁôÊ±¼ä¡£ */
    (void)ws63_lock_mgr_refresh_auth_window();
    return ERRCODE_SUCC;
}

errcode_t ws63_task_zw101_request_verify_after_release(void)
{
    unsigned int irq_status;

    if (g_ws63_zw101_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    if (g_ws63_zw101_verify_disabled != 0U) {
        WS63_FINAL_IRQ_UNLOCK(irq_status);
        return ERRCODE_FAIL;
    }

    /*
     * VERIFY Ê§°ÜºóÍ³Ò»½øÈë wait_release£º
     * Ã¿ 0.3s ÂÖÑ¯Ò»´Î PS_GetImageInfo£¬Ö»ÓĞ ACK=0x02£¨ÎŞÊÖÖ¸£©
     * ²ÅÔÊĞíÅÅ¶ÓÏÂÒ»´Î VERIFY£¬±ÜÃâÁ¬Ğø¿Õ¼ì°Ñ×´Ì¬»ú´òÂú¡£
     */
    ws63_zw101_reset_timeout_retry_state();
    g_ws63_zw101_verify_requested = 0U;
    g_ws63_zw101_verify_cancelled = 0U;
    g_ws63_zw101_verify_wait_release = 1U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("request_verify_after_release");
    (void)ws63_lock_mgr_refresh_auth_window();
    return ERRCODE_SUCC;
}

/**
 * @brief ÖØÖÃµ±Ç° ARMED ´°¿ÚÄÚµÄ ZW101 ½ûÓÃ±£»¤×´Ì¬¡£
 */
void ws63_task_zw101_reset_armed_window_guard(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_verify_disabled = 0U;
    g_ws63_zw101_verify_fail_streak = 0U;
    g_ws63_zw101_verify_timeout_streak = 0U;
    g_ws63_zw101_verify_wait_release = 0U;
    g_ws63_zw101_last_verify_ack = 0xFFU;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("reset_armed_window_guard");
}

/**
 * @brief È¡ÏûÉĞÎ´¿ªÊ¼µÄ ZW101 VERIFY ÇëÇó¡£
 */
void ws63_task_zw101_cancel_verify_request(void)
{
    unsigned int irq_status;

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_verify_requested = 0U;
    g_ws63_zw101_verify_cancelled = 1U;
    g_ws63_zw101_verify_wait_release = 0U;
    WS63_FINAL_IRQ_UNLOCK(irq_status);

    ws63_zw101_trace_verify_state("cancel_verify");
}

/**
 * @brief È¡Ïûµ±Ç°¿ÉÄÜÕıÔÚÖ´ĞĞµÄ ZW101 VERIFY Á÷³Ì¡£
 *
 * ËµÃ÷£ºDEBUG INIT »áÓÅÏÈµ÷ÓÃÕâ¸öÈë¿Ú£¬ÏÈÇåµôÈÎÎñ²ã¹ÒÆğÇëÇó£¬ÔÙÏòÄ£×éÏÂ·¢
 * CANCEL ÃüÁî£¬¾¡Á¿°Ñ¡°ÅÅ¶ÓÖĞµÄ VERIFY¡±ºÍ¡°ÕıÔÚÅÜµÄ VERIFY¡±Ò»ÆğÊÕ¿Ú¡£
 */
errcode_t ws63_task_zw101_cancel_active_request(void)
{
    errcode_t ret;

    ws63_task_zw101_cancel_verify_request();

    if (zw101_is_ready() == 0U) {
        return ERRCODE_SUCC;
    }

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    {
        uint8_t ack_code = 0xFFU;

        ret = zw101_cancel(&ack_code);
        osal_printk("[zw101 trace] cancel_active ret=0x%x ack=0x%02x\r\n",
            (unsigned int)ret,
            (unsigned int)ack_code);
    }
#else
    ret = zw101_cancel(NULL);
#endif
    return ret;
}

/**
 * @brief »ñÈ¡×î½üÒ»´Î VERIFY ACK¡£
 *
 * ÓÃÍ¾£º¸ø lock_mgr Ìá¹©Ê§°ÜÀàĞÍÅĞ±ğÒÀ¾İ£¨ÀıÈç NOT_PRESSED ÊÇ·ñ¼ÆÈëËø¶¨Ê§°Ü´ÎÊı£©¡£
 */
uint8_t ws63_task_zw101_get_last_verify_ack(void)
{
    unsigned int irq_status;
    uint8_t ack_code;

    irq_status = WS63_FINAL_IRQ_LOCK();
    ack_code = g_ws63_zw101_last_verify_ack;
    WS63_FINAL_IRQ_UNLOCK(irq_status);
    return ack_code;
}

/**
 * @brief ÅĞ¶Ï ACK ÊÇ·ñÊôÓÚ¡°ÈÏÖ¤Ê§°Ü¿É¼ÌĞøÖØÊÔ¡±µÄÀàĞÍ¡£
 */
static uint8_t ws63_zw101_is_verify_fail_ack(uint8_t ack_code)
{
    /*
    * NOT_PRESSED ÊÓÎª¡°Î´´¥·¢ÓĞĞ§°´Ñ¹¡±µÄ¿É»Ö¸´×´Ì¬£º
    * - ÈÔÔÊĞíÔÚµ±Ç°´°¿Ú¼ÌĞøÖØÊÔ£»
    * - ²»¼ÆÈë ZW101 Á¬ĞøÊ§°Ü½ûÓÃ¼ÆÊı¡£
    *
    * ³¬Ê±ÖØÊÔÒÑ¾­ºÄ¾¡£»·ñÔò»áÏÈ×ß¡°Á¢¼´ÖØÀ­ VERIFY¡±µÄ¿ì½İ·ÖÖ§¡£
     */
    if ((ack_code == 0x08U) || (ack_code == 0x24U) ||  (ack_code == 0x09U)) {
        return 1U;
    }

    return 0U;
}

/**
 * @brief ÏòÃÅËø½á¹ûÁ´Â·ÉÏ±¨Ò»´Î ZW101 VERIFY ½á¹û¡£
 */
static void ws63_zw101_report_verify_result(errcode_t ret, uint8_t ack_code, uint16_t match_id, uint16_t score)
{
    uint8_t passed;
    uint8_t fail_disable = 0U;
    unsigned int irq_status;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    uint8_t fail_streak_after;
    uint8_t disabled_after;
    uint8_t timeout_retry_streak_after;
#endif

    passed = ((ret == ERRCODE_SUCC) && (ack_code == WS63_ZW101_ACK_OK)) ? 1U : 0U;

    /* ³É¹¦ÏÈ°ÑËùÓĞÖØÊÔÌ¬Çåµô£¬±ÜÃâÏÂÒ»ÂÖÉúÃüÖÜÆÚ¼Ì³ĞÉÏÒ»ÂÖµÄ³¬Ê±ÀúÊ·¡£ */
    if (passed != 0U) {
        ws63_zw101_reset_timeout_retry_state();
        (void)ws63_lock_mgr_refresh_auth_window();
    } else if ((ack_code == WS63_ZW101_ACK_TIMEOUT) && (ws63_zw101_try_retry_verify_after_timeout() != 0U)) {
        /*
         * ACK_TIMEOUT ÔÚÉúÃüÖÜÆÚÄÚ±»×ª»»ÎªÄÚ²¿ÖØÊÔÊ±£¬²»ÏòÃÅËøÉÏ±¨Ê§°Ü£¬±ÜÃâ
         * lock_mgr °ÑÒ»´ÎÍ¨ĞÅ³¬Ê±ÎóÅĞ³ÉÖÕÌ¬Ê§°Ü²¢Á¢¿Ì·¢ Die¡£
         */
        return;
    } else {
        ws63_zw101_reset_timeout_retry_state();
        if (ack_code != WS63_ZW101_ACK_TIMEOUT) {
            (void)ws63_lock_mgr_refresh_auth_window();
        }
    }

    irq_status = WS63_FINAL_IRQ_LOCK();
    g_ws63_zw101_last_verify_ack = ack_code;
    if (passed != 0U) {
        g_ws63_zw101_verify_fail_streak = 0U;
    } else {
        if ((ws63_zw101_is_verify_fail_ack(ack_code) != 0U) ||
            ((ret != ERRCODE_SUCC) && (ack_code == WS63_ZW101_ACK_UNKNOWN))) {
            if (g_ws63_zw101_verify_fail_streak < 0xFFU) {
                g_ws63_zw101_verify_fail_streak++;
            }
        }

        if ((g_ws63_zw101_verify_fail_streak >= WS63_ZW101_VERIFY_FAIL_DISABLE_THRESHOLD) &&
            (g_ws63_zw101_verify_disabled == 0U)) {
            g_ws63_zw101_verify_disabled = 1U;
            fail_disable = 1U;
            g_ws63_zw101_verify_requested = 0U;
            g_ws63_zw101_verify_cancelled = 1U;
            g_ws63_zw101_verify_wait_release = 0U;
        }
    }

    WS63_FINAL_IRQ_UNLOCK(irq_status);

    if (passed != 0U) {
        /* Ö¸ÎÆÍ¨¹ıÊ±¸üĞÂ lock_mgr ¸½¼Ó×Ö¶Î£¬¹©¿ªËø³É¹¦ÊÂ¼şÆ´½Ó finger_id/score¡£ */
        ws63_lock_mgr_update_finger_result(match_id, score);
        osal_printk("[zw101] VERIFY SUCCESS id=%u score=%u ack=0x%02x(%s)\r\n",
            (unsigned int)match_id,
            (unsigned int)score,
            (unsigned int)ack_code,
            ws63_zw101_ack_to_text(ack_code));
    } else {
        osal_printk("[zw101] VERIFY FAIL ret=0x%x ack=0x%02x(%s)\r\n",
            (unsigned int)ret,
            (unsigned int)ack_code,
            ws63_zw101_ack_to_text(ack_code));
    }

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    /* ÏêÏ¸½á¹û¿ìÕÕÖ»ÔÚ´ò¿ª×·×ÙÊ±Êä³ö£¬±ÜÃâÕı³£ÈÏÖ¤Â·¾¶ÖØ¸´Ë¢ÆÁ¡£ */
    fail_streak_after = g_ws63_zw101_verify_fail_streak;
    disabled_after = g_ws63_zw101_verify_disabled;
    timeout_retry_streak_after = g_ws63_zw101_verify_timeout_streak;
    osal_printk("[zw101 trace] verify_result ret=0x%x ack=0x%02x id=%u score=%u fail_streak=%u timeout_retry=%u disabled=%u\r\n",
        (unsigned int)ret,
        (unsigned int)ack_code,
        (unsigned int)match_id,
        (unsigned int)score,
        (unsigned int)fail_streak_after,
        (unsigned int)timeout_retry_streak_after,
        (unsigned int)disabled_after);
#endif

    if (fail_disable != 0U) {
        osal_printk("[zw101] VERIFY disabled after %u continuous failures\r\n",
            (unsigned int)WS63_ZW101_VERIFY_FAIL_DISABLE_THRESHOLD);
        ws63_task_zw101_notify_alarm(WS63_ZW101_ALARM_TEXT);
        (void)ws63_task_post_lock_event_text("result=locked;source=finger;reason=zw101_fail_5");
    }

    (void)ws63_lock_mgr_report_auth_result(WS63_LOCK_AUTH_SOURCE_ZW101, passed, ack_code);
}

/**
 * @brief ZW101 VERIFY ÈÎÎñÈë¿Ú¡£
 */
static void *ws63_zw101_task_entry(const char *arg)
{
    (void)arg;

    /* µÈ´ıÔ­ÒòÂëÓÃÓÚÈ¥ÖØÈÕÖ¾£¬±ÜÃâÔÚ¿ÕÏĞÂÖÑ¯Ê±Ë¢ÆÁ¡£ */
    uint8_t last_wait_reason = 0xFFU;
    uint32_t last_ready_retry_ms = 0U;
    uint32_t last_release_check_ms = 0U;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
    uint8_t last_release_finger_present = 0xFFU;
    uint32_t last_release_check_ret = 0xFFFFFFFFU;
#endif

    while (1) {
        uint8_t request;
        uint8_t cancelled;
        uint8_t wait_release;
        uint8_t disabled;
        uint8_t wait_reason = 0U;

        {
            unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
            request = g_ws63_zw101_verify_requested;
            cancelled = g_ws63_zw101_verify_cancelled;
            wait_release = g_ws63_zw101_verify_wait_release;
            disabled = g_ws63_zw101_verify_disabled;
            WS63_FINAL_IRQ_UNLOCK(irq_status);
        }

        if (wait_release != 0U) {
            if (last_wait_reason != 5U) {
                ws63_zw101_trace_verify_state("task_wait_finger_release");
                last_wait_reason = 5U;
                last_release_check_ms = 0U;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
                last_release_finger_present = 0xFFU;
                last_release_check_ret = 0xFFFFFFFFU;
#endif
            }

            if ((disabled == 0U) && (cancelled == 0U) && (ws63_lock_mgr_is_armed() != 0U) &&
                (ws63_task_zw101_is_ready() != 0U)) {
                uint32_t now_ms = ws63_os_tick_ms();
                if ((uint32_t)(now_ms - last_release_check_ms) >= WS63_ZW101_RELEASE_CHECK_GAP_MS) {
                    uint8_t finger_present = 1U;
                    uint8_t ack_code = 0xFFU;
                    errcode_t check_ret;

                    last_release_check_ms = now_ms;
                    check_ret = zw101_check_finger_present(&finger_present, &ack_code);

#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
                    /* ½öÔÚ×´Ì¬±ä»¯Ê±´òÓ¡£¬±ÜÃâ wait_release ÆÚ¼ä¹Ì¶¨½ÚÅÄÈÕÖ¾Ë¢ÆÁ¡£ */
                    if (((uint32_t)check_ret != last_release_check_ret) ||
                        (finger_present != last_release_finger_present)) {
                        osal_printk("[zw101 trace] release_check ret=0x%x ack=0x%02x finger=%s\r\n",
                            (unsigned int)check_ret,
                            (unsigned int)ack_code,
                            ws63_zw101_finger_text(finger_present));
                        last_release_check_ret = (uint32_t)check_ret;
                        last_release_finger_present = finger_present;
                    }
#endif

                    if ((check_ret == ERRCODE_SUCC) && (finger_present == 0U)) {
                        unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
                        g_ws63_zw101_verify_wait_release = 0U;
                        g_ws63_zw101_verify_requested = 1U;
                        g_ws63_zw101_verify_cancelled = 0U;
                        WS63_FINAL_IRQ_UNLOCK(irq_status);

                        osal_printk("[zw101] finger released, retry verify queued\r\n");
                        ws63_zw101_trace_verify_state("retry_verify_after_release");
                        (void)ws63_lock_mgr_refresh_auth_window();
                    }
                }
            }

            ws63_os_sleep_ms(20U);
            continue;
        }

        if (request == 0U) {
            wait_reason = 1U;
        } else if (cancelled != 0U) {
            wait_reason = 2U;
        } else if (disabled != 0U) {
            wait_reason = 3U;
        }

        if (wait_reason != 0U) {
            if (wait_reason != last_wait_reason) {
                ws63_zw101_trace_verify_state("task_wait_request_or_disabled");
                last_wait_reason = wait_reason;
            }
            ws63_os_sleep_ms(20U);
            continue;
        }

        if ((ws63_task_zw101_is_ready() == 0U) || (ws63_lock_mgr_is_armed() == 0U)) {
            if (last_wait_reason != 4U) {
                ws63_zw101_trace_verify_state("task_wait_ready_or_armed");
                last_wait_reason = 4U;
            }

            /*
             * ×ÔÓúÖØÊÔ£ºµ±´¦ÓÚ ARMED ÇÒÒÑÓĞ VERIFY ÇëÇó£¬µ«Éè±¸ÈÔÎ´ ready Ê±£¬
             * ÖÜÆÚµ÷ÓÃ ensure_zw101_ready£¬±ÜÃâµ¥´Î¶èĞÔ³õÊ¼»¯Ê§°ÜºóÕû´°³¬Ê±¡£
             */
            if ((request != 0U) && (cancelled == 0U) && (disabled == 0U) && (ws63_lock_mgr_is_armed() != 0U) &&
                (ws63_task_zw101_is_ready() == 0U)) {
                uint32_t now_ms = ws63_os_tick_ms();
                if ((uint32_t)(now_ms - last_ready_retry_ms) >= WS63_ZW101_READY_RETRY_GAP_MS) {
                    last_ready_retry_ms = now_ms;
#if (WS63_ZW101_TRACE_DETAIL_ENABLE == 1U)
                    {
                        errcode_t recover_ret;

                        recover_ret = ws63_task_ensure_zw101_ready();
                        osal_printk("[zw101 trace] ready_recover ret=0x%x\r\n", (unsigned int)recover_ret);
                    }
#else
                    (void)ws63_task_ensure_zw101_ready();
#endif
                }
            }

            ws63_os_sleep_ms(20U);
            continue;
        }

        last_wait_reason = 0U;

        {
            uint16_t match_id = 0U;
            uint16_t score = 0U;
            uint8_t ack_code = 0xFFU;
            errcode_t ret;

            /* ÏÈÇåÇëÇóÎ»£¬ÔÙ½øÈë×èÈûÊ½ VERIFY£¬±ÜÃâÍ¬Ò»´°¿ÚÄÚÖØ¸´²¢·¢µ÷ÓÃ¡£ */
            ws63_task_zw101_cancel_verify_request();

            /* VERIFY ¿ªÊ¼Ç°Ë¢ĞÂ´°¿Ú£¬·ÀÖ¹³¤ºÄÊ±ÈÏÖ¤°Ñ»½ĞÑÌ¬ÌáÇ°´ò¶Ï¡£ */
            (void)ws63_lock_mgr_refresh_auth_window();

            osal_printk("[zw101] VERIFYING level=%u target=0x%04X param=0x%04X\r\n",
                (unsigned int)WS63_ZW101_VERIFY_LEVEL_DEFAULT,
                (unsigned int)WS63_ZW101_VERIFY_ID_DEFAULT,
                (unsigned int)WS63_ZW101_VERIFY_PARAM_DEFAULT);
            ws63_zw101_trace_verify_state("task_before_verify");
            ret = zw101_verify(WS63_ZW101_VERIFY_LEVEL_DEFAULT,
                WS63_ZW101_VERIFY_ID_DEFAULT,
                WS63_ZW101_VERIFY_PARAM_DEFAULT,
                &match_id,
                &score,
                &ack_code);
            ws63_zw101_report_verify_result(ret, ack_code, match_id, score);
            ws63_zw101_trace_verify_state("task_after_verify");
        }
    }

    return NULL;
}

/**
 * @brief Æô¶¯ ZW101 VERIFY ÈÎÎñ¡£
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
        WS63_ZW101_TASK_STACK_SIZE,
        WS63_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[zw101] task start fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    g_ws63_zw101_task_started = 1U;
    g_ws63_zw101_verify_requested = 0U;
    g_ws63_zw101_verify_cancelled = 0U;
    g_ws63_zw101_verify_wait_release = 0U;
    g_ws63_zw101_verify_disabled = 0U;
    g_ws63_zw101_verify_fail_streak = 0U;
    g_ws63_zw101_verify_timeout_streak = 0U;
    g_ws63_zw101_last_verify_ack = 0xFFU;
    return ERRCODE_SUCC;
}

/**
 * @brief ÉèÖÃ SLE ÉÏĞĞ success ÈÕÖ¾¿ª¹Ø¡£
 */
errcode_t ws63_task_sle_uplink_log_set_enable(uint8_t enable)
{
    return ws63_sle_set_uplink_success_log_enable(enable);
}

/**
 * @brief »ñÈ¡ SLE ÉÏĞĞ success ÈÕÖ¾¿ª¹Ø¡£
 */
uint8_t ws63_task_sle_uplink_log_get_enable(void)
{
    return ws63_sle_get_uplink_success_log_enable();
}

/**
 * @brief ÉèÖÃ SLE ÉÏĞĞ success ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 */
errcode_t ws63_task_sle_uplink_log_set_gap_ms(uint32_t gap_ms)
{
    return ws63_sle_set_uplink_success_log_gap_ms(gap_ms);
}

/**
 * @brief »ñÈ¡ SLE ÉÏĞĞ success ÈÕÖ¾×îĞ¡Êä³ö¼ä¸ô¡£
 */
uint32_t ws63_task_sle_uplink_log_get_gap_ms(void)
{
    return ws63_sle_get_uplink_success_log_gap_ms();
}

/**
 * @brief ÖØĞÂ³õÊ¼»¯ ZW101£¨´¥·¢ÎÕÊÖ¼ì²â£©¡£
 */
errcode_t ws63_task_zw101_reinit(void)
{
    errcode_t ret;

    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        return ERRCODE_FAIL;
    }

    ret = zw101_init(ZW101_SUBPORT);
    if (ret == ERRCODE_SUCC) {
        unsigned int irq_status = WS63_FINAL_IRQ_LOCK();
        g_ws63_zw101_verify_disabled = 0U;
        g_ws63_zw101_verify_fail_streak = 0U;
        g_ws63_zw101_verify_requested = 0U;
        g_ws63_zw101_verify_cancelled = 0U;
        g_ws63_zw101_verify_wait_release = 0U;
        g_ws63_zw101_last_verify_ack = 0xFFU;
        WS63_FINAL_IRQ_UNLOCK(irq_status);
    }

    return ret;
}

/**
 * @brief Ö´ĞĞ ZW101 ECHO ÃüÁî¡£
 */
errcode_t ws63_task_zw101_echo(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_echo(ack_out);
}

/**
 * @brief Ö´ĞĞ ZW101 ×Ô¶¯ÑéÖ¤£¨VERIFY£©¡£
 */
errcode_t ws63_task_zw101_verify(uint8_t score_level,
    uint16_t target_id,
    uint16_t param_flags,
    uint16_t *match_id_out,
    uint16_t *score_out,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_verify(score_level,
        target_id,
        param_flags,
        match_id_out,
        score_out,
        ack_out);
}

/**
 * @brief Ö´ĞĞ ZW101 ×Ô¶¯×¢²á£¨ENROLL£©¡£
 */
errcode_t ws63_task_zw101_enroll(uint16_t page_id,
    uint8_t enroll_times,
    uint16_t param_flags,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_enroll(page_id, enroll_times, param_flags, ack_out);
}

/**
 * @brief ²éÑ¯ ZW101 ÓĞĞ§Ä£°åÊıÁ¿£¨LIST£©¡£
 */
errcode_t ws63_task_zw101_list(uint16_t *valid_num_out, uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_list(valid_num_out, ack_out);
}

/**
 * @brief è¯»å– ZW101 ç´¢å¼•è¡¨
 */
errcode_t ws63_task_zw101_read_index_table(uint8_t page, uint8_t *index_buf_out, uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_read_index_table(page, index_buf_out, ack_out);
}

/**
 * @brief É¾³ı ZW101 Ä£°å£¨DEL£©¡£
 */
errcode_t ws63_task_zw101_delete(uint16_t page_id,
    uint16_t count,
    uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_delete(page_id, count, ack_out);
}

/**
 * @brief Çå¿Õ ZW101 Ä£°å¿â£¨CLEAR£©¡£
 */
errcode_t ws63_task_zw101_clear(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_clear(ack_out);
}

/**
 * @brief È¡Ïû ZW101 µ±Ç°Á÷³Ì£¨CANCEL£©¡£
 */
errcode_t ws63_task_zw101_cancel(uint8_t *ack_out)
{
    if (!ws63_is_subport_enabled(ZW101_SUBPORT)) {
        if (ack_out != NULL) {
            *ack_out = 0xFFU;
        }
        return ERRCODE_FAIL;
    }

    return zw101_cancel(ack_out);
}
