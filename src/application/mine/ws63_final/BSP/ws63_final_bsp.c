/**
 * @file ws63_final_bsp.c
 * @brief WK2114 最终版 BSP/HAL 层实现。
 */

#include "ws63_final_bsp.h"

#include "gpio.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "pinctrl.h"
#include "systick.h"
#include "uart.h"

#include "ws63_final_config.h"

/**
 * @brief IRQ 空实现。
 *
 * 当前阶段仅保留中断链路，实际业务在 Driver/App 层走轮询，
 * 便于后续按模块接入统一事件分发。
 */
static void ws63_irq_stub(pin_t pin, uintptr_t param)
{
    (void)pin;
    (void)param;
}

/**
 * @brief 初始化主口 UART。
 */
errcode_t ws63_bsp_host_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len)
{
    uart_attr_t attr = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };
    uart_pin_config_t pin_cfg = {
        .tx_pin = WS63_HOST_UART_TX_PIN,
        .rx_pin = WS63_HOST_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uart_buffer_config_t rx_cfg;

    if ((rx_buffer == NULL) || (rx_buffer_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    uapi_pin_set_mode(WS63_HOST_UART_TX_PIN, WS63_HOST_UART_PIN_MODE);
    uapi_pin_set_mode(WS63_HOST_UART_RX_PIN, WS63_HOST_UART_PIN_MODE);

    rx_cfg.rx_buffer = rx_buffer;
    rx_cfg.rx_buffer_size = rx_buffer_len;

    (void)uapi_uart_deinit(WS63_HOST_UART_BUS);
    if (uapi_uart_init(WS63_HOST_UART_BUS, &pin_cfg, &attr, NULL, &rx_cfg) != ERRCODE_SUCC) {
        osal_printk("[wk2114 final bsp] host uart init fail\r\n");
        return ERRCODE_FAIL;
    }

    osal_printk("[wk2114 final bsp] host uart init ok\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 主口 UART 发送适配。
 */
int32_t ws63_bsp_host_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return uapi_uart_write(WS63_HOST_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 主口 UART 读取适配。
 */
int32_t ws63_bsp_host_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return uapi_uart_read(WS63_HOST_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 清空主口 UART 接收缓冲区。
 */
errcode_t ws63_bsp_host_uart_flush_rx(void)
{
    uint8_t dummy[16] = {0};
    uint8_t round;

    /*
     * 兼容实现：循环非阻塞读取直到无数据。
     * 避免依赖某些构建配置下未导出的 uapi_uart_flush_rx_data 符号。
     */
    for (round = 0U; round < 32U; round++) {
        if (uapi_uart_read(WS63_HOST_UART_BUS, dummy, sizeof(dummy), 0U) <= 0) {
            break;
        }
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 复位引脚初始化。
 */
void ws63_bsp_reset_init(void)
{
    uapi_pin_set_mode(WS63_RST_PIN, WS63_RST_PIN_MODE);
    uapi_gpio_set_dir(WS63_RST_PIN, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(WS63_RST_PIN, GPIO_LEVEL_HIGH);
}

/**
 * @brief 设置复位引脚电平。
 */
void ws63_bsp_reset_set(uint8_t level_high)
{
    uapi_gpio_set_val(WS63_RST_PIN,
        level_high ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/**
 * @brief 初始化 IRQ 引脚。
 */
void ws63_bsp_irq_init(void)
{
    uapi_pin_set_mode(WS63_IRQ_PIN, WS63_IRQ_PIN_MODE);
    uapi_gpio_set_dir(WS63_IRQ_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_register_isr_func(WS63_IRQ_PIN,
        GPIO_INTERRUPT_FALLING_EDGE, ws63_irq_stub);
    uapi_gpio_enable_interrupt(WS63_IRQ_PIN);
}

/**
 * @brief 毫秒延时适配。
 */
void ws63_bsp_sleep_ms(uint32_t ms)
{
    (void)osal_msleep(ms);
}

/**
 * @brief 获取系统毫秒计时。
 */
uint32_t ws63_bsp_get_tick_ms(void)
{
    return (uint32_t)uapi_systick_get_ms();
}
