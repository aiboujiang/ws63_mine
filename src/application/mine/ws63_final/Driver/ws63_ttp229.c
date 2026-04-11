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
 * 1) 先按移植代码时序给 SDO 输出起始脉冲；
 * 2) 切回输入后，按 SCL 脉冲逐位采样；
 * 3) 对原始位图做语义反转，得到“位1=按下”的统一表示。
 */
errcode_t ws63_ttp229_read_sample(ws63_ttp229_sample_t *sample)
{
    uint16_t raw_code = 0U;
    uint16_t pressed_mask;
    uint8_t i;

    if (sample == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    if (ws63_bsp_ttp229_set_sdo_output(1U) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }
    ws63_bsp_ttp229_delay_us(WS63_TTP229_START_PULSE_HIGH_US);

    if (ws63_bsp_ttp229_set_sdo_output(0U) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }
    ws63_bsp_ttp229_delay_us(WS63_TTP229_START_PULSE_LOW_US);

    if (ws63_bsp_ttp229_set_sdo_input() != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    for (i = 0U; i < 16U; i++) {
        if (ws63_bsp_ttp229_set_scl(1U) != ERRCODE_SUCC) {
            return ERRCODE_FAIL;
        }
        ws63_bsp_ttp229_delay_us(WS63_TTP229_SCL_PULSE_HIGH_US);

        if (ws63_bsp_ttp229_set_scl(0U) != ERRCODE_SUCC) {
            return ERRCODE_FAIL;
        }
        ws63_bsp_ttp229_delay_us(WS63_TTP229_SCL_PULSE_LOW_US);

        raw_code |= (uint16_t)(ws63_bsp_ttp229_read_sdo_level() << i);
    }

    ws63_bsp_ttp229_delay_ms(WS63_TTP229_READ_GAP_MS);

    /* 原始协议中 0 表示按下，这里反转为 1 表示按下，便于上层判断。 */
    pressed_mask = (uint16_t)(~raw_code);

    sample->raw_code = raw_code;
    sample->pressed_mask = pressed_mask;
    sample->pressed_count = ws63_ttp229_count_pressed(pressed_mask);
    sample->multi_key = (sample->pressed_count >= 2U) ? 1U : 0U;
    return ERRCODE_SUCC;
}
