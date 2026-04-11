/**
 * @file ws63_final_bsp_ttp229.c
 * @brief WS63_final TTP229 BSP 子模块实现。
 *
 * 说明：
 * 1) 本文件是 TTP229 唯一允许直接访问 GPIO/I2C API 的层；
 * 2) Driver 层仅通过本文件导出的接口完成初始化与读取。
 */

#include "ws63_final_bsp.h"

#include "i2c.h"
#include "gpio.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "pinctrl.h"

#include "ws63_final_config.h"

/**
 * @brief 初始化 TTP229 I2C 相关引脚与主机控制器。
 */
errcode_t ws63_bsp_ttp229_init(void)
{
    errcode_t ret;

    uapi_pin_init();

    /* I2C 走复用功能脚，先把 SCL/SDA 切到外设模式并上拉，避免空闲态漂浮。 */
    ret = uapi_pin_set_mode(WS63_TTP229_SCL_PIN, (pin_mode_t)WS63_TTP229_I2C_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_TTP229_SDA_PIN, (pin_mode_t)WS63_TTP229_I2C_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_pull(WS63_TTP229_SCL_PIN, PIN_PULL_TYPE_UP);
    if (ret != ERRCODE_SUCC) {
        /* GPIO15/16 在不同板级上不一定都支持内部上拉，这里保留告警但不阻断 I2C 初始化。 */
        osal_printk("[wk2114 final bsp] TTP229 SCL pull config skip, ret=0x%x\r\n", (unsigned int)ret);
    }

    ret = uapi_pin_set_pull(WS63_TTP229_SDA_PIN, PIN_PULL_TYPE_UP);
    if (ret != ERRCODE_SUCC) {
        /* 触摸键盘总线只依赖可用的 SDA/SCL 复用，外部上拉存在时可继续工作。 */
        osal_printk("[wk2114 final bsp] TTP229 SDA pull config skip, ret=0x%x\r\n", (unsigned int)ret);
    }

    /* 按规格书以 7bit 从机地址 0x65 进行标准 I2C 读取。 */
    return uapi_i2c_master_init(WS63_TTP229_I2C_BUS, WS63_TTP229_I2C_SPEED, 0U);
}

/**
 * @brief 通过 I2C 读取 TTP229 的 2 字节键值。
 */
errcode_t ws63_bsp_ttp229_read_bytes(uint8_t *data, uint16_t len)
{
    i2c_data_t i2c_data = {0};

    if ((data == NULL) || (len < WS63_TTP229_I2C_READ_LEN)) {
        return ERRCODE_INVALID_PARAM;
    }

    i2c_data.receive_buf = data;
    i2c_data.receive_len = len;

    return uapi_i2c_master_read(WS63_TTP229_I2C_BUS, WS63_TTP229_I2C_ADDR, &i2c_data);
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
