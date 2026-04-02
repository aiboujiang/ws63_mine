#include <stdbool.h>
#include <stdint.h>

#include "app_init.h"
#include "dma.h"
#include "gpio.h"
#include "pinctrl.h"
#include "platform_core.h"
#include "securec.h"
#include "soc_osal.h"
#include "spi.h"
#include "tcxo.h"
#include "watchdog.h"

#define RGB_LED_TASK_PRIO                24
#define RGB_LED_TASK_STACK_SIZE          0x1000

/* WS2812 数据脚使用 GPIO9，复用为 SPI0_OUT。 */
#define WS2812_DATA_PIN                  GPIO_09
#define WS2812_SPI_DO_PIN_MODE           PIN_MODE_3

/* SPI0_SCK 在 GPIO7 的复用信号 3。 */
#define WS2812_SPI_CLK_PIN               GPIO_07
#define WS2812_SPI_CLK_PIN_MODE          PIN_MODE_3

/* SPI+DMA 发送配置：4MHz + 5bit 符号，满足 WS2812 单 bit 1.25us。 */
#define WS2812_SPI_BUS_ID                SPI_BUS_0
#define WS2812_SPI_FREQ_MHZ              4U
#define WS2812_SPI_TIMEOUT_MS            0xFFFFFFFFU

/* WS2812 编码：0 -> 10000，1 -> 11100。 */
#define WS2812_SPI_SYMBOL_BITS           5U
#define WS2812_SPI_SYMBOL_0              0x10U
#define WS2812_SPI_SYMBOL_1              0x1CU

/* 单灯 24bit(GRB) 经 5bit 编码后的总长度。 */
#define WS2812_SPI_FRAME_BITS            (24U * WS2812_SPI_SYMBOL_BITS)
#define WS2812_SPI_FRAME_BYTES           ((WS2812_SPI_FRAME_BITS + 7U) / 8U)

/* 额外发送全 0，确保复位低电平窗口 > 80us。 */
#define WS2812_SPI_RESET_BYTES           40U
#define WS2812_SPI_TX_BYTES              (WS2812_SPI_FRAME_BYTES + WS2812_SPI_RESET_BYTES)

/* SPI DMA 固定使用 8bit 宽度。 */
#define WS2812_SPI_DMA_WIDTH_BYTE        0U

/* 调试阶段默认打开日志，稳定后可改为 0 降低串口输出。 */
#define WS2812_DEBUG_LOG_ENABLE          1

/* 颜色切换演示间隔。 */
#define WS2812_DEMO_INTERVAL_MS          500U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_color_t;

static const ws2812_color_t g_ws2812_demo_colors[] = {
    {255U, 0U, 0U},
    {0U, 255U, 0U},
    {0U, 0U, 255U}
};

static uint8_t g_ws2812_spi_tx_buf[WS2812_SPI_TX_BYTES] = {0};

/**
 * @brief 打印 SPI 配置参数。
 */
static void ws2812_log_config(void)
{
#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][cfg] data_pin=%u spi_mode=%u interval_ms=%u\r\n",
        (unsigned int)WS2812_DATA_PIN,
        (unsigned int)WS2812_SPI_DO_PIN_MODE,
        (unsigned int)WS2812_DEMO_INTERVAL_MS);
    osal_printk("[mine_rgb][cfg] spi bus=%u clk_pin=%u clk_mode=%u freq_mhz=%u frame_bytes=%u reset_bytes=%u reset_us=%u\r\n",
        (unsigned int)WS2812_SPI_BUS_ID,
        (unsigned int)WS2812_SPI_CLK_PIN,
        (unsigned int)WS2812_SPI_CLK_PIN_MODE,
        (unsigned int)WS2812_SPI_FREQ_MHZ,
        (unsigned int)WS2812_SPI_FRAME_BYTES,
        (unsigned int)WS2812_SPI_RESET_BYTES,
        (unsigned int)((WS2812_SPI_RESET_BYTES * 8U + WS2812_SPI_FREQ_MHZ - 1U) / WS2812_SPI_FREQ_MHZ));
#endif
}

/**
 * @brief 往 SPI 位流缓冲区写入 1bit（MSB first）。
 */
static inline void ws2812_spi_write_1bit(uint8_t *buf, uint32_t bit_index, bool bit_val)
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
static void ws2812_spi_append_symbol(uint8_t *buf, uint32_t *bit_cursor, uint8_t symbol)
{
    uint8_t i;

    for (i = 0U; i < WS2812_SPI_SYMBOL_BITS; i++) {
        uint8_t shift = (uint8_t)(WS2812_SPI_SYMBOL_BITS - 1U - i);
        ws2812_spi_write_1bit(buf, *bit_cursor, ((symbol >> shift) & 0x1U) != 0U);
        (*bit_cursor)++;
    }
}

/**
 * @brief 将 1 字节颜色数据编码为 WS2812 SPI 位流。
 */
static void ws2812_spi_encode_byte(uint8_t value, uint8_t *buf, uint32_t *bit_cursor)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        bool bit_is_one = ((value & 0x80U) != 0U);
        ws2812_spi_append_symbol(buf, bit_cursor, bit_is_one ? WS2812_SPI_SYMBOL_1 : WS2812_SPI_SYMBOL_0);
        value <<= 1U;
    }
}

/**
 * @brief 按 GRB 顺序把颜色编码到 SPI 发送缓冲区。
 */
static void ws2812_spi_encode_color(const ws2812_color_t *color)
{
    uint32_t bit_cursor = 0U;

    (void)memset_s(g_ws2812_spi_tx_buf, sizeof(g_ws2812_spi_tx_buf), 0, sizeof(g_ws2812_spi_tx_buf));

    /* WS2812 固定要求 GRB 顺序。 */
    ws2812_spi_encode_byte(color->g, g_ws2812_spi_tx_buf, &bit_cursor);
    ws2812_spi_encode_byte(color->r, g_ws2812_spi_tx_buf, &bit_cursor);
    ws2812_spi_encode_byte(color->b, g_ws2812_spi_tx_buf, &bit_cursor);

    /* 末尾保持全 0，形成复位低电平时间窗口。 */
}

/**
 * @brief 初始化 SPI+DMA 发送链路。
 */
static errcode_t ws2812_spi_init(void)
{
    errcode_t ret;
    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    ret = uapi_pin_set_mode(WS2812_DATA_PIN, WS2812_SPI_DO_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS2812_SPI_CLK_PIN, WS2812_SPI_CLK_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    config.is_slave = false;
    config.slave_num = 1;
    config.bus_clk = SPI_CLK_FREQ;
    config.freq_mhz = WS2812_SPI_FREQ_MHZ;
    config.clk_polarity = 0;
    config.clk_phase = 0;
    config.frame_format = 0;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = HAL_SPI_TRANS_MODE_TX;
    config.sste = 0;

    ext_config.qspi_param.wait_cycles = 0x10;

    ret = uapi_spi_init(WS2812_SPI_BUS_ID, &config, &ext_config);
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
            .src_width = WS2812_SPI_DMA_WIDTH_BYTE,
            .dest_width = WS2812_SPI_DMA_WIDTH_BYTE,
            .burst_length = 0,
            .priority = 0,
        };
        ret = uapi_spi_set_dma_mode(WS2812_SPI_BUS_ID, true, &dma_cfg);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }
#endif
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief 通过 SPI+DMA 发送单颗 WS2812 颜色数据。
 */
static errcode_t ws2812_spi_send_color(const ws2812_color_t *color, uint32_t *send_us, uint32_t *reset_us)
{
    spi_xfer_data_t tx_data = {0};
    uint64_t t_send_begin;
    uint64_t t_send_end;
    errcode_t ret;

    if (color == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    ws2812_spi_encode_color(color);

    tx_data.tx_buff = g_ws2812_spi_tx_buf;
    tx_data.tx_bytes = sizeof(g_ws2812_spi_tx_buf);
    tx_data.rx_buff = NULL;
    tx_data.rx_bytes = 0;

    t_send_begin = uapi_tcxo_get_us();
    ret = uapi_spi_master_write(WS2812_SPI_BUS_ID, &tx_data, WS2812_SPI_TIMEOUT_MS);
    t_send_end = uapi_tcxo_get_us();

    if (send_us != NULL) {
        *send_us = (uint32_t)(t_send_end - t_send_begin);
    }
    if (reset_us != NULL) {
        *reset_us = (uint32_t)((WS2812_SPI_RESET_BYTES * 8U + WS2812_SPI_FREQ_MHZ - 1U) / WS2812_SPI_FREQ_MHZ);
    }

    return ret;
}

/**
 * @brief 设置单颗 WS2812 颜色（GRB 顺序）。
 *
 * @param color 目标颜色。
 * @param send_us 输出参数：实际 SPI 传输耗时（微秒）。
 * @param reset_us 输出参数：复位低电平理论耗时（微秒）。
 */
static void ws2812_set_color(const ws2812_color_t *color, uint32_t *send_us, uint32_t *reset_us)
{
    errcode_t ret;

    if (color == NULL) {
        return;
    }

    if (send_us != NULL) {
        *send_us = 0U;
    }
    if (reset_us != NULL) {
        *reset_us = 0U;
    }

    ret = ws2812_spi_send_color(color, send_us, reset_us);
    if (ret != ERRCODE_SUCC) {
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][spi] send failed, ret=0x%x\n", (unsigned int)ret);
#endif
    }
}

/**
 * @brief 初始化 WS2812 所需外设（仅 SPI+DMA）。
 *
 * @return errcode_t 成功返回 ERRCODE_SUCC。
 */
static errcode_t ws2812_init(void)
{
    errcode_t ret;

    ws2812_log_config();

    (void)uapi_tcxo_init();
    uapi_gpio_init();

    ret = ws2812_spi_init();
    if (ret != ERRCODE_SUCC) {
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][init] spi_dma init failed, ret=0x%x\n", (unsigned int)ret);
#endif
        return ret;
    }

#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][init] spi_dma init ok\n");
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief RGB 演示任务：循环显示红绿蓝。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *rgb_led_task(const char *arg)
{
    errcode_t ret;
    errcode_t wdt_ret;
    uint32_t i;
    uint32_t frame_cnt = 0U;
    uint64_t tx_begin_us;
    uint64_t tx_end_us;
    uint32_t send24_us;
    uint32_t reset_cost_us;

    unused(arg);

    ret = ws2812_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb] ws2812 init failed, ret=0x%x\n", (unsigned int)ret);
        return NULL;
    }

    osal_printk("[mine_rgb] ws2812 demo start (SPI+DMA)\n");

    while (1) {
        for (i = 0U; i < (uint32_t)(sizeof(g_ws2812_demo_colors) / sizeof(g_ws2812_demo_colors[0])); i++) {
            tx_begin_us = uapi_tcxo_get_us();
#if (WS2812_DEBUG_LOG_ENABLE == 1)
            osal_printk("[mine_rgb][loop] frame=%u idx=%u rgb=(%u,%u,%u) begin_us=%llu\n",
                (unsigned int)frame_cnt,
                (unsigned int)i,
                (unsigned int)g_ws2812_demo_colors[i].r,
                (unsigned int)g_ws2812_demo_colors[i].g,
                (unsigned int)g_ws2812_demo_colors[i].b,
                (unsigned long long)tx_begin_us);
#endif

            ws2812_set_color(&g_ws2812_demo_colors[i], &send24_us, &reset_cost_us);
            tx_end_us = uapi_tcxo_get_us();

#if (WS2812_DEBUG_LOG_ENABLE == 1)
            osal_printk("[mine_rgb][loop] frame=%u idx=%u total_cost_us=%llu tx_us=%u reset_us=%u tx_bytes=%u\n",
                (unsigned int)frame_cnt,
                (unsigned int)i,
                (unsigned long long)(tx_end_us - tx_begin_us),
                (unsigned int)send24_us,
                (unsigned int)reset_cost_us,
                (unsigned int)WS2812_SPI_TX_BYTES);
#endif

            wdt_ret = uapi_watchdog_kick();
#if (WS2812_DEBUG_LOG_ENABLE == 1)
            if (wdt_ret != ERRCODE_SUCC) {
                osal_printk("[mine_rgb][loop] watchdog_kick failed, ret=0x%x\n", (unsigned int)wdt_ret);
            }
#endif
            osal_msleep(WS2812_DEMO_INTERVAL_MS);
        }

        frame_cnt++;
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][hb] frame cycle done, frame=%u uptime_ms=%llu\n",
            (unsigned int)frame_cnt,
            (unsigned long long)uapi_tcxo_get_ms());
#endif
    }
}

/**
 * @brief 创建 RGB LED 演示任务。
 */
static void rgb_led_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)rgb_led_task, 0, "RgbLedTask", RGB_LED_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, RGB_LED_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the rgb_led_entry. */
app_run(rgb_led_entry);