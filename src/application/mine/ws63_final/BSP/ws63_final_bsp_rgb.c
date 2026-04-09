/**
 * @file ws63_final_bsp_rgb.c
 * @brief WK2114 最终版 BSP RGB/SPI 子模块实现。
 */

#include "ws63_final_bsp.h"

#include <stdbool.h>

#include "dma.h"
#include "pinctrl.h"
#include "spi.h"

#include "ws63_final_config.h"

/* 仅在本文件内维护 RGB SPI 初始化状态，避免重复初始化导致不必要错误。 */
static bool g_ws63_rgb_spi_inited = false;

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
