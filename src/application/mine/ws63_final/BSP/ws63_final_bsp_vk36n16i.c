/**
 * @file ws63_final_bsp_vk36n16i.c
 * @brief WS63_final VK36N16I BSP 子模块实现。
 *
 * 说明：
 * 1) 本文件是 VK36N16I 唯一允许直接访问 GPIO/I2C API 的层；
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
 * @brief 初始化 VK36N16I I2C 相关引脚与主机控制器。
 *
 * 说明：板级已经提供外部上拉，这里只做 I2C 复用和主机初始化，避免
 * 额外的内部上拉配置和外部上拉互相干扰。
 */
errcode_t ws63_bsp_vk36n16i_init(void)
{
    errcode_t ret;

    uapi_pin_init();

    /* I2C 走复用功能脚，先把 SCL/SDA 切到外设模式并上拉，避免空闲态漂浮。 */
    ret = uapi_pin_set_mode(WS63_VK36N16I_SCL_PIN, (pin_mode_t)WS63_VK36N16I_I2C_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_VK36N16I_SDA_PIN, (pin_mode_t)WS63_VK36N16I_I2C_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 按规格书以 7bit 从机地址 0x65 进行标准 I2C 读取。 */
    return uapi_i2c_master_init(WS63_VK36N16I_I2C_BUS, WS63_VK36N16I_I2C_SPEED, 0U);
}

/**
 * @brief 通过 I2C 读取 VK36N16I 的 2 字节键值。
 */
errcode_t ws63_bsp_vk36n16i_read_bytes(uint8_t *data, uint16_t len)
{
    i2c_data_t i2c_data = {0};

    if ((data == NULL) || (len < WS63_VK36N16I_I2C_READ_LEN)) {
        return ERRCODE_INVALID_PARAM;
    }

    i2c_data.receive_buf = data;
    i2c_data.receive_len = len;

    return uapi_i2c_master_read(WS63_VK36N16I_I2C_BUS, WS63_VK36N16I_I2C_ADDR, &i2c_data);
}

/**
 * @brief VK36N16I 毫秒级延时。
 */
void ws63_bsp_vk36n16i_delay_ms(uint32_t ms)
{
    if (ms == 0U) {
        return;
    }

    (void)osal_msleep(ms);
}
