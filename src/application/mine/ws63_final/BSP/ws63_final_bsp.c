/**
 * @file ws63_final_bsp.c
 * @brief WK2114 最终版 BSP/HAL 层实现。
 */

#include "ws63_final_bsp.h"

#include <stdbool.h>

#include "dma.h"
#include "gpio.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "pinctrl.h"
#include "spi.h"
#include "systick.h"
#include "uart.h"

#include "ws63_final_config.h"

/* 仅在本文件内维护 RGB SPI 初始化状态，避免重复初始化导致不必要错误。 */
static bool g_ws63_rgb_spi_inited = false;

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

/**
 * @brief 初始化 RGB WS2812 使用的 SPI1 输出链路。
 */
errcode_t ws63_bsp_rgb_spi_init(void)
{
    errcode_t ret;
    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    if (g_ws63_rgb_spi_inited) {
        return ERRCODE_SUCC;
    }

    ret = uapi_pin_set_mode(WS63_RGB_DATA_PIN, WS63_RGB_DATA_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_RGB_CLK_PIN, WS63_RGB_CLK_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    config.is_slave = false;
    config.slave_num = 1;
    config.bus_clk = SPI_CLK_FREQ;
    config.freq_mhz = WS63_RGB_SPI_FREQ_MHZ;
    config.clk_polarity = 0;
    config.clk_phase = 0;
    config.frame_format = 0;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = HAL_SPI_TRANS_MODE_TX;
    config.sste = 0;

    ext_config.qspi_param.wait_cycles = 0x10;

    ret = uapi_spi_init(WS63_RGB_SPI_BUS, &config, &ext_config);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

#if defined(CONFIG_SPI_SUPPORT_DMA) && (CONFIG_SPI_SUPPORT_DMA == 1)
    ret = uapi_dma_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_dma_open();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

#ifndef CONFIG_SPI_SUPPORT_POLL_AND_DMA_AUTO_SWITCH
    {
        spi_dma_config_t dma_cfg = {
            .src_width = 0,
            .dest_width = 0,
            .burst_length = 0,
            .priority = 0,
        };
        ret = uapi_spi_set_dma_mode(WS63_RGB_SPI_BUS, true, &dma_cfg);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }
#endif
#endif

    g_ws63_rgb_spi_inited = true;
    return ERRCODE_SUCC;
}

/**
 * @brief 通过 RGB SPI1 链路发送编码后的 WS2812 帧数据。
 */
errcode_t ws63_bsp_rgb_spi_write(const uint8_t *tx_buf, uint32_t tx_bytes, uint32_t timeout_ms)
{
    spi_xfer_data_t tx_data = {0};

    if ((tx_buf == NULL) || (tx_bytes == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    tx_data.tx_buff = (uint8_t *)tx_buf;
    tx_data.tx_bytes = tx_bytes;
    tx_data.rx_buff = NULL;
    tx_data.rx_bytes = 0;

    return uapi_spi_master_write(WS63_RGB_SPI_BUS, &tx_data, timeout_ms);
}
