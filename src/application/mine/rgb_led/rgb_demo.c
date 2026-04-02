#include <stdbool.h>
#include <stdint.h>

#include "app_init.h"
#include "dma.h"
#include "pinctrl.h"
#include "platform_core.h"
#include "securec.h"
#include "soc_osal.h"
#include "spi.h"
#include "tcxo.h"
#include "watchdog.h"

#define RGB_LED_TASK_PRIO                24
#define RGB_LED_TASK_STACK_SIZE          0x1000

/* WS2812 数据线固定接 GPIO9，对应 SPI0_OUT。 */
#define WS2812_DATA_PIN                  GPIO_09
#define WS2812_SPI_DO_PIN_MODE           PIN_MODE_3

/* SPI0_SCK 走 GPIO7，维持 SPI 时钟输出。 */
#define WS2812_SPI_CLK_PIN               GPIO_07
#define WS2812_SPI_CLK_PIN_MODE          PIN_MODE_3

/* SPI+DMA 发送配置：4MHz + 5bit 符号，单 bit 对应 1.25us。 */
#define WS2812_SPI_BUS_ID                SPI_BUS_0
#define WS2812_SPI_FREQ_MHZ              4U
#define WS2812_SPI_TIMEOUT_MS            0xFFFFFFFFU

/* WS2812 编码：0 -> 10000，1 -> 11100。 */
#define WS2812_SPI_SYMBOL_BITS           5U
#define WS2812_SPI_SYMBOL_0              0x10U
#define WS2812_SPI_SYMBOL_1              0x1CU

/* 单灯 24bit(GRB) 经编码后的总长度。 */
#define WS2812_SPI_FRAME_BITS            (24U * WS2812_SPI_SYMBOL_BITS)
#define WS2812_SPI_FRAME_BYTES           ((WS2812_SPI_FRAME_BITS + 7U) / 8U)

/* 追加全 0，保证复位低电平时间窗口。 */
#define WS2812_SPI_RESET_BYTES           40U
#define WS2812_SPI_TX_BYTES              (WS2812_SPI_FRAME_BYTES + WS2812_SPI_RESET_BYTES)
#define WS2812_SPI_RESET_US              ((WS2812_SPI_RESET_BYTES * 8U + WS2812_SPI_FREQ_MHZ - 1U) / WS2812_SPI_FREQ_MHZ)

/* SPI DMA 固定使用 8bit 宽度。 */
#define WS2812_SPI_DMA_WIDTH_BYTE        0U

/* 颜色切换演示间隔。 */
#define WS2812_DEMO_INTERVAL_MS          500U

/* 调试阶段默认打开日志，稳定后可改为 0。 */
#define WS2812_DEBUG_LOG_ENABLE          1

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
 * @brief 打印关键配置，便于确认接线与时序参数。
 */
static void ws2812_log_config(void)
{
#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][cfg] data_pin=%u spi_mode=%u clk_pin=%u clk_mode=%u\r\n",
        (unsigned int)WS2812_DATA_PIN,
        (unsigned int)WS2812_SPI_DO_PIN_MODE,
        (unsigned int)WS2812_SPI_CLK_PIN,
        (unsigned int)WS2812_SPI_CLK_PIN_MODE);
    osal_printk("[mine_rgb][cfg] spi_bus=%u freq_mhz=%u frame_bytes=%u reset_bytes=%u reset_us=%u\r\n",
        (unsigned int)WS2812_SPI_BUS_ID,
        (unsigned int)WS2812_SPI_FREQ_MHZ,
        (unsigned int)WS2812_SPI_FRAME_BYTES,
        (unsigned int)WS2812_SPI_RESET_BYTES,
        (unsigned int)WS2812_SPI_RESET_US);
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
 * @brief 追加 5bit WS2812 符号到 SPI 位流。
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
 * @brief 把 1 字节颜色数据编码成 WS2812 SPI 位流。
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
 * @brief 将 GRB 颜色编码到 SPI 缓冲区。
 */
static void ws2812_spi_encode_color(const ws2812_color_t *color)
{
    uint32_t bit_cursor = 0U;

    (void)memset_s(g_ws2812_spi_tx_buf, sizeof(g_ws2812_spi_tx_buf), 0, sizeof(g_ws2812_spi_tx_buf));

    /* WS2812 固定要求 GRB 顺序。 */
    ws2812_spi_encode_byte(color->g, g_ws2812_spi_tx_buf, &bit_cursor);
    ws2812_spi_encode_byte(color->r, g_ws2812_spi_tx_buf, &bit_cursor);
    ws2812_spi_encode_byte(color->b, g_ws2812_spi_tx_buf, &bit_cursor);

    /* 缓冲区后半段保留全 0，用于复位锁存时间。 */
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
#else
    return ERRCODE_NOT_SUPPORT;
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief 发送单颗 WS2812 颜色（GRB）。
 */
static errcode_t ws2812_set_color(const ws2812_color_t *color, uint32_t *send_us, uint32_t *reset_us)
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
        *reset_us = (uint32_t)WS2812_SPI_RESET_US;
    }

    return ret;
}

/**
 * @brief 初始化 WS2812 演示。
 */
static errcode_t ws2812_init(void)
{
    errcode_t ret;

    ws2812_log_config();
    (void)uapi_tcxo_init();

    ret = ws2812_spi_init();
    if (ret != ERRCODE_SUCC) {
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][init] spi_dma init failed, ret=0x%x\r\n", (unsigned int)ret);
#endif
        return ret;
    }

#if (WS2812_DEBUG_LOG_ENABLE == 1)
    osal_printk("[mine_rgb][init] spi_dma init ok\r\n");
#endif
    return ERRCODE_SUCC;
}

/**
 * @brief RGB 演示任务：循环显示红绿蓝。
 */
static void *rgb_led_task(const char *arg)
{
    errcode_t ret;
    errcode_t wdt_ret;
    uint32_t i;
    uint32_t frame_cnt = 0U;
    uint64_t tx_begin_us;
    uint64_t tx_end_us;
    uint32_t send_us;
    uint32_t reset_us;

    unused(arg);

    ret = ws2812_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine_rgb] ws2812 init failed, ret=0x%x\r\n", (unsigned int)ret);
        return NULL;
    }

    osal_printk("[mine_rgb] ws2812 demo start, backend=spi_dma\r\n");

    while (1) {
        for (i = 0U; i < (uint32_t)(sizeof(g_ws2812_demo_colors) / sizeof(g_ws2812_demo_colors[0])); i++) {
            tx_begin_us = uapi_tcxo_get_us();
            ret = ws2812_set_color(&g_ws2812_demo_colors[i], &send_us, &reset_us);
            tx_end_us = uapi_tcxo_get_us();

#if (WS2812_DEBUG_LOG_ENABLE == 1)
            if (ret == ERRCODE_SUCC) {
                osal_printk("[mine_rgb][loop] frame=%u idx=%u rgb=(%u,%u,%u) total_us=%llu tx_us=%u reset_us=%u tx_bytes=%u\r\n",
                    (unsigned int)frame_cnt,
                    (unsigned int)i,
                    (unsigned int)g_ws2812_demo_colors[i].r,
                    (unsigned int)g_ws2812_demo_colors[i].g,
                    (unsigned int)g_ws2812_demo_colors[i].b,
                    (unsigned long long)(tx_end_us - tx_begin_us),
                    (unsigned int)send_us,
                    (unsigned int)reset_us,
                    (unsigned int)WS2812_SPI_TX_BYTES);
            } else {
                osal_printk("[mine_rgb][loop] send failed, frame=%u idx=%u ret=0x%x\r\n",
                    (unsigned int)frame_cnt,
                    (unsigned int)i,
                    (unsigned int)ret);
            }
#endif

            /* 周期内喂狗并让出 CPU，避免系统负载异常。 */
            wdt_ret = uapi_watchdog_kick();
#if (WS2812_DEBUG_LOG_ENABLE == 1)
            if (wdt_ret != ERRCODE_SUCC) {
                osal_printk("[mine_rgb][loop] watchdog_kick failed, ret=0x%x\r\n", (unsigned int)wdt_ret);
            }
#endif
            osal_msleep(WS2812_DEMO_INTERVAL_MS);
        }

        frame_cnt++;
#if (WS2812_DEBUG_LOG_ENABLE == 1)
        osal_printk("[mine_rgb][hb] frame cycle done, frame=%u uptime_ms=%llu\r\n",
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