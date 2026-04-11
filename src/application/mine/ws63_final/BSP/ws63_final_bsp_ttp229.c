/**
 * @file ws63_final_bsp_ttp229.c
 * @brief WK2114 最终版 BSP TTP229 子模块实现。
 *
 * 说明：
 * 1) 本文件是 TTP229 唯一允许直接访问 GPIO/延时 API 的层；
 * 2) Driver 层仅通过本文件导出的接口操作引脚与时序。
 */

#include "ws63_final_bsp.h"

#include "gpio.h"
#include "osal_task.h"
#include "pinctrl.h"
#include "tcxo.h"

#include "ws63_final_config.h"

/**
 * @brief 配置 TTP229 SCL 引脚为输出。
 */
static errcode_t ws63_bsp_ttp229_setup_scl_output(void)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(WS63_TTP229_SCL_PIN, (pin_mode_t)WS63_TTP229_SCL_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_dir(WS63_TTP229_SCL_PIN, GPIO_DIRECTION_OUTPUT);
}

/**
 * @brief 初始化 TTP229 相关引脚。
 */
errcode_t ws63_bsp_ttp229_init(void)
{
    errcode_t ret;

    uapi_pin_init();
    uapi_gpio_init();

    ret = ws63_bsp_ttp229_setup_scl_output();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_val(WS63_TTP229_SCL_PIN, GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_TTP229_SDO_PIN, (pin_mode_t)WS63_TTP229_SDO_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_dir(WS63_TTP229_SDO_PIN, GPIO_DIRECTION_INPUT);
}

/**
 * @brief 配置 SDO 为输出并设置电平。
 */
errcode_t ws63_bsp_ttp229_set_sdo_output(uint8_t level_high)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(WS63_TTP229_SDO_PIN, (pin_mode_t)WS63_TTP229_SDO_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(WS63_TTP229_SDO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_val(WS63_TTP229_SDO_PIN,
        (level_high != 0U) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/**
 * @brief 配置 SDO 为输入。
 */
errcode_t ws63_bsp_ttp229_set_sdo_input(void)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(WS63_TTP229_SDO_PIN, (pin_mode_t)WS63_TTP229_SDO_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_dir(WS63_TTP229_SDO_PIN, GPIO_DIRECTION_INPUT);
}

/**
 * @brief 设置 SCL 电平。
 */
errcode_t ws63_bsp_ttp229_set_scl(uint8_t level_high)
{
    return uapi_gpio_set_val(WS63_TTP229_SCL_PIN,
        (level_high != 0U) ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/**
 * @brief 读取 SDO 电平。
 */
uint8_t ws63_bsp_ttp229_read_sdo_level(void)
{
    return (uapi_gpio_get_val(WS63_TTP229_SDO_PIN) == GPIO_LEVEL_HIGH) ? 1U : 0U;
}

/**
 * @brief TTP229 微秒级延时。
 */
void ws63_bsp_ttp229_delay_us(uint32_t us)
{
    if (us == 0U) {
        return;
    }

    (void)uapi_tcxo_delay_us(us);
}

/**
 * @brief TTP229 毫秒级延时。
 */
void ws63_bsp_ttp229_delay_ms(uint32_t ms)
{
    if (ms == 0U) {
        return;
    }

    (void)osal_msleep(ms);
}
