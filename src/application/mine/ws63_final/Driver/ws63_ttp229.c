/**
 * @file ws63_ttp229.c
 * @brief WS63 TTP229 驱动层实现。
 */

#include "ws63_ttp229.h"

#include "ws63_final_bsp.h"
#include "ws63_final_config.h"

/**
 * @brief 初始化 TTP229 底层引脚。
 */
errcode_t ws63_ttp229_init(void)
{
    return ws63_bsp_ttp229_init();
}

/**
 * @brief 统计按下按键数量（位为1表示按下）。
 */
uint8_t ws63_ttp229_count_pressed(uint16_t pressed_mask)
{
    uint8_t count = 0U;
    uint8_t i;

    for (i = 0U; i < 16U; i++) {
        if ((pressed_mask & (uint16_t)(1U << i)) != 0U) {
            count++;
        }
    }

    return count;
}

/**
 * @brief 读取一次 TTP229 16 位按键数据。
 *
 * 关键流程：
 * 1) 通过 I2C 直接读取 2 字节键值，不再手工驱动 SCL/SDO 时序；
 * 2) 对读取到的两个字节做小端拼接，得到 16 位键值；
 * 3) 按规格书语义直接输出“位1=按下”的统一表示。
 */
errcode_t ws63_ttp229_read_sample(ws63_ttp229_sample_t *sample)
{
    uint8_t key_bytes[WS63_TTP229_I2C_READ_LEN] = {0};
    uint16_t raw_code;
    uint8_t attempt;
    errcode_t ret;

    if (sample == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = ERRCODE_FAIL;
    for (attempt = 0U; attempt < WS63_TTP229_READ_RETRY_MAX; attempt++) {
        ret = ws63_bsp_ttp229_read_bytes(key_bytes, WS63_TTP229_I2C_READ_LEN);
        if (ret == ERRCODE_SUCC) {
            break;
        }

        /* I2C 读失败后短暂退避，优先覆盖上电抖动和偶发总线繁忙。 */
        if (attempt + 1U < WS63_TTP229_READ_RETRY_MAX) {
            ws63_bsp_ttp229_delay_ms(WS63_TTP229_READ_RETRY_GAP_MS);
        }
    }

    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 手册定义第 1 个字节对应 TP0~TP7，第 2 个字节对应 TP8~TP15。 */
    raw_code = (uint16_t)key_bytes[0] | (uint16_t)((uint16_t)key_bytes[1] << 8);

    sample->raw_code = raw_code;
    sample->pressed_mask = raw_code;
    sample->pressed_count = ws63_ttp229_count_pressed(raw_code);
    sample->multi_key = (sample->pressed_count >= 2U) ? 1U : 0U;
    return ERRCODE_SUCC;
}
