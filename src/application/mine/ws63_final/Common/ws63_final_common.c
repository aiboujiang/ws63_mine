/**
 * @file ws63_final_common.c
 * @brief WK2114 最终版公共层实现。
 */

#include "ws63_final_common.h"

#include "ws63_final_config.h"

/**
 * @brief 计算 WK2114 BAUD1/BAUD0/PRES。
 */
errcode_t ws63_calc_baud_regs(uint32_t baud,
    uint8_t *baud1_out, uint8_t *baud0_out, uint8_t *pres_out)
{
    uint64_t denominator;
    uint64_t reg_x100;
    uint32_t reg_int;
    uint8_t reg_first_decimal;
    uint16_t baud_reg;

    if ((baud == 0U) || (baud1_out == NULL) || (baud0_out == NULL) || (pres_out == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    /*
     * 手册规则：Reg = Fosc / (16 * baud)。
     * 这里先保留两位小数做四舍五入，再拆分整数/小数。
     */
    denominator = (uint64_t)baud * 16U;
    reg_x100 = ((uint64_t)WS63_XTAL_FREQ_HZ * 100U + (denominator / 2U)) / denominator;
    reg_int = (uint32_t)(reg_x100 / 100U);
    if ((reg_int == 0U) || (reg_int > 0x10000U)) {
        return ERRCODE_FAIL;
    }

    reg_first_decimal = (uint8_t)((reg_x100 / 10U) % 10U);
    baud_reg = (uint16_t)(reg_int - 1U);

    *baud1_out = (uint8_t)((baud_reg >> 8) & 0xFFU);
    *baud0_out = (uint8_t)(baud_reg & 0xFFU);
    *pres_out = reg_first_decimal;
    return ERRCODE_SUCC;
}

/**
 * @brief 判断子串口号是否在有效范围（1~4）。
 */
uint8_t ws63_is_subport_valid(uint8_t sub_port)
{
    return (sub_port >= 1U) && (sub_port <= 4U);
}

/**
 * @brief 查询子串口是否启用。
 */
uint8_t ws63_is_subport_enabled(uint8_t sub_port)
{
    switch (sub_port) {
        case 1U:
            return WS63_SUBPORT1_ENABLE;
        case 2U:
            return WS63_SUBPORT2_ENABLE;
        case 3U:
            return WS63_SUBPORT3_ENABLE;
        case 4U:
            return WS63_SUBPORT4_ENABLE;
        default:
            return 0U;
    }
}

/**
 * @brief 读取子串口波特率配置。
 */
uint32_t ws63_get_subport_baud(uint8_t sub_port)
{
    switch (sub_port) {
        case 1U:
            return WS63_SUBPORT1_BAUD;
        case 2U:
            return WS63_SUBPORT2_BAUD;
        case 3U:
            return WS63_SUBPORT3_BAUD;
        case 4U:
            return WS63_SUBPORT4_BAUD;
        default:
            return 0U;
    }
}
