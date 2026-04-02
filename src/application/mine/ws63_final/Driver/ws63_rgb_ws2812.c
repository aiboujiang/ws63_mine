/**
 * @file ws63_rgb_ws2812.c
 * @brief WS63 Final 分层框架的 WS2812 RGB 驱动实现。
 */

#include "ws63_rgb_ws2812.h"

#include <stdbool.h>
#include <stdint.h>

#include "osal_debug.h"
#include "securec.h"

#include "ws63_final_bsp.h"
#include "ws63_final_config.h"

/* WS2812 编码规则：0 -> 10000，1 -> 11100。 */
#define WS63_RGB_SPI_SYMBOL_BITS            5U
#define WS63_RGB_SPI_SYMBOL_0               0x10U
#define WS63_RGB_SPI_SYMBOL_1               0x1CU

/* 单灯 24bit(GRB) 经 5bit 编码后的总长度。 */
#define WS63_RGB_SPI_FRAME_BITS             (24U * WS63_RGB_SPI_SYMBOL_BITS)
#define WS63_RGB_SPI_FRAME_BYTES            ((WS63_RGB_SPI_FRAME_BITS + 7U) / 8U)

/* 额外发送全 0，确保复位低电平窗口 > 80us。 */
#define WS63_RGB_SPI_RESET_BYTES            40U
#define WS63_RGB_SPI_TX_BYTES               (WS63_RGB_SPI_FRAME_BYTES + WS63_RGB_SPI_RESET_BYTES)

/* 单模块静态发送缓冲区，避免任务循环中的动态分配。 */
static uint8_t g_ws63_rgb_spi_tx_buf[WS63_RGB_SPI_TX_BYTES] = {0};

/**
 * @brief 往 SPI 位流缓冲区写入 1bit（MSB first）。
 */
static inline void ws63_rgb_write_1bit(uint8_t *buf, uint32_t bit_index, bool bit_val)
{
    uint32_t byte_index = bit_index >> 3;
    uint32_t bit_offset = 7U - (bit_index & 0x7U);

    if (bit_val) {
        buf[byte_index] |= (uint8_t)(1U << bit_offset);
    }
}

/**
 * @brief 将 5bit WS2812 符号追加到 SPI 位流。
 */
static void ws63_rgb_append_symbol(uint8_t *buf, uint32_t *bit_cursor, uint8_t symbol)
{
    uint8_t i;

    for (i = 0U; i < WS63_RGB_SPI_SYMBOL_BITS; i++) {
        uint8_t shift = (uint8_t)(WS63_RGB_SPI_SYMBOL_BITS - 1U - i);
        ws63_rgb_write_1bit(buf, *bit_cursor, ((symbol >> shift) & 0x1U) != 0U);
        (*bit_cursor)++;
    }
}

/**
 * @brief 将 1 字节颜色数据编码为 WS2812 SPI 位流。
 */
static void ws63_rgb_encode_byte(uint8_t value, uint8_t *buf, uint32_t *bit_cursor)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        bool bit_is_one = ((value & 0x80U) != 0U);
        ws63_rgb_append_symbol(buf, bit_cursor, bit_is_one ? WS63_RGB_SPI_SYMBOL_1 : WS63_RGB_SPI_SYMBOL_0);
        value <<= 1U;
    }
}

/**
 * @brief 按 GRB 顺序把颜色编码到 SPI 发送缓冲区。
 */
static void ws63_rgb_encode_color(const ws63_rgb_color_t *color)
{
    uint32_t bit_cursor = 0U;

    (void)memset_s(g_ws63_rgb_spi_tx_buf, sizeof(g_ws63_rgb_spi_tx_buf), 0, sizeof(g_ws63_rgb_spi_tx_buf));

    /* WS2812 协议固定为 GRB 顺序。 */
    ws63_rgb_encode_byte(color->g, g_ws63_rgb_spi_tx_buf, &bit_cursor);
    ws63_rgb_encode_byte(color->r, g_ws63_rgb_spi_tx_buf, &bit_cursor);
    ws63_rgb_encode_byte(color->b, g_ws63_rgb_spi_tx_buf, &bit_cursor);
}

/**
 * @brief 初始化 WS2812 驱动。
 */
errcode_t ws63_rgb_ws2812_init(void)
{
    errcode_t ret;

    ret = ws63_bsp_rgb_spi_init();
    if (ret != ERRCODE_SUCC) {
#if (WS63_RGB_LOG_ENABLE == 1U)
        osal_printk("[ws63 rgb][init] spi1 init fail, ret=0x%x\r\n", (unsigned int)ret);
#endif
        return ret;
    }

#if (WS63_RGB_LOG_ENABLE == 1U)
    osal_printk("[ws63 rgb][init] spi1 init ok, bus=%u data_pin=%u clk_pin=%u mode=%u\r\n",
        (unsigned int)WS63_RGB_SPI_BUS,
        (unsigned int)WS63_RGB_DATA_PIN,
        (unsigned int)WS63_RGB_CLK_PIN,
        (unsigned int)WS63_RGB_DATA_PIN_MODE);
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief 设置单颗 WS2812 颜色。
 */
errcode_t ws63_rgb_ws2812_set_color(const ws63_rgb_color_t *color)
{
    if (color == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    ws63_rgb_encode_color(color);
    return ws63_bsp_rgb_spi_write(g_ws63_rgb_spi_tx_buf,
        (uint32_t)sizeof(g_ws63_rgb_spi_tx_buf),
        WS63_RGB_SPI_TIMEOUT_MS);
}
