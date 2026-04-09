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
#include "pwm.h"
#include "spi.h"
#include "systick.h"
#include "uart.h"

#include "ws63_final_config.h"

/* 仅在本文件内维护 RGB SPI 初始化状态，避免重复初始化导致不必要错误。 */
static bool g_ws63_rgb_spi_inited = false;
static bool g_ws63_motor_pwm_inited = false;

/**
 * @brief 配置 GPIO 输出并拉到目标电平。
 */
static errcode_t ws63_bsp_set_gpio_output_level(pin_t pin, uint8_t mode, uint8_t level_high)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(pin, (pin_mode_t)mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(pin, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_val(pin, level_high ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/**
 * @brief 配置编码器输入引脚（模式/上下拉/方向）。
 */
static errcode_t ws63_bsp_set_gpio_input(pin_t pin, uint8_t mode, uint8_t pull_mode)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(pin, (pin_mode_t)mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_pull(pin, (pin_pull_t)pull_mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_set_dir(pin, GPIO_DIRECTION_INPUT);
}

/**
 * @brief 根据占空比构建 PWM 配置。
 */
static errcode_t ws63_bsp_motor_build_pwm_cfg(uint8_t duty_percent, pwm_config_t *cfg)
{
    uint32_t duty;
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint32_t low_ticks;

    if (cfg == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    period_ticks = WS63_MOTOR_PWM_PERIOD_TICKS;
    if (period_ticks < 2U) {
        return ERRCODE_INVALID_PARAM;
    }

    duty = duty_percent;
    if (duty > 100U) {
        duty = 100U;
    }

    high_ticks = (period_ticks * duty) / 100U;
    if (high_ticks == 0U) {
        high_ticks = 1U;
    }
    if (high_ticks >= period_ticks) {
        high_ticks = period_ticks - 1U;
    }

    low_ticks = period_ticks - high_ticks;
    if (low_ticks == 0U) {
        low_ticks = 1U;
    }

    cfg->low_time = low_ticks;
    cfg->high_time = high_ticks;
    cfg->offset_time = 0U;
    cfg->cycles = 0U;
    cfg->repeat = true;
    return ERRCODE_SUCC;
}

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

/**
 * @brief 初始化电机底层资源（GPIO/PWM）。
 */
errcode_t ws63_bsp_motor_init(void)
{
    errcode_t ret;

    uapi_pin_init();
    uapi_gpio_init();

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IA_PIN, WS63_MOTOR_IA_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IB_PIN, WS63_MOTOR_IB_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    if (!g_ws63_motor_pwm_inited) {
        ret = uapi_pwm_init();
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        g_ws63_motor_pwm_inited = true;
    }

#if defined(CONFIG_PWM_USING_V151)
    {
        uint8_t channel_id;

        channel_id = WS63_MOTOR_IA_PWM_CHANNEL;
        ret = uapi_pwm_set_group(WS63_MOTOR_IA_PWM_GROUP, &channel_id, 1U);
        if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_PWM_INVALID_PARAMETER)) {
            return ret;
        }

        channel_id = WS63_MOTOR_IB_PWM_CHANNEL;
        ret = uapi_pwm_set_group(WS63_MOTOR_IB_PWM_GROUP, &channel_id, 1U);
        if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_PWM_INVALID_PARAMETER)) {
            return ret;
        }
    }
#endif

    return ERRCODE_SUCC;
}

/**
 * @brief 关闭电机 PWM 输出并恢复 IA/IB 为 GPIO 模式。
 */
errcode_t ws63_bsp_motor_disable_pwm(void)
{
    errcode_t ret;

    if (g_ws63_motor_pwm_inited) {
        (void)uapi_pwm_close(WS63_MOTOR_IA_PWM_CHANNEL);
        (void)uapi_pwm_close(WS63_MOTOR_IB_PWM_CHANNEL);
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IA_PIN, (pin_mode_t)WS63_MOTOR_IA_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IB_PIN, (pin_mode_t)WS63_MOTOR_IB_GPIO_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 设置电机 IA/IB GPIO 电平。
 */
errcode_t ws63_bsp_motor_set_level(uint8_t ia_high, uint8_t ib_high)
{
    errcode_t ret;

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IA_PIN, WS63_MOTOR_IA_GPIO_MODE, ia_high);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ws63_bsp_set_gpio_output_level(WS63_MOTOR_IB_PIN, WS63_MOTOR_IB_GPIO_MODE, ib_high);
}

/**
 * @brief 使能 IA 通道 PWM 输出。
 */
errcode_t ws63_bsp_motor_enable_pwm_ia(uint8_t duty_percent)
{
    errcode_t ret;
    pwm_config_t cfg;

    ret = ws63_bsp_motor_build_pwm_cfg(duty_percent, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_motor_disable_pwm();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IB_PIN, WS63_MOTOR_IB_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IA_PIN, (pin_mode_t)WS63_MOTOR_IA_PWM_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    (void)uapi_pwm_close(WS63_MOTOR_IA_PWM_CHANNEL);
    ret = uapi_pwm_open(WS63_MOTOR_IA_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start(WS63_MOTOR_IA_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        (void)uapi_pwm_close(WS63_MOTOR_IA_PWM_CHANNEL);
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 使能 IB 通道 PWM 输出。
 */
errcode_t ws63_bsp_motor_enable_pwm_ib(uint8_t duty_percent)
{
    errcode_t ret;
    pwm_config_t cfg;

    ret = ws63_bsp_motor_build_pwm_cfg(duty_percent, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_motor_disable_pwm();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = ws63_bsp_set_gpio_output_level(WS63_MOTOR_IA_PIN, WS63_MOTOR_IA_GPIO_MODE, 0U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(WS63_MOTOR_IB_PIN, (pin_mode_t)WS63_MOTOR_IB_PWM_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    (void)uapi_pwm_close(WS63_MOTOR_IB_PWM_CHANNEL);
    ret = uapi_pwm_open(WS63_MOTOR_IB_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start(WS63_MOTOR_IB_PWM_CHANNEL);
    if (ret != ERRCODE_SUCC) {
        (void)uapi_pwm_close(WS63_MOTOR_IB_PWM_CHANNEL);
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 初始化编码器 IO。
 */
errcode_t ws63_bsp_encoder_init(void)
{
    errcode_t ret;

    uapi_pin_init();
    uapi_gpio_init();

    ret = ws63_bsp_set_gpio_input(WS63_ENCODER_A_PIN, WS63_ENCODER_PIN_MODE, WS63_ENCODER_PULL_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ws63_bsp_set_gpio_input(WS63_ENCODER_B_PIN, WS63_ENCODER_PIN_MODE, WS63_ENCODER_PULL_MODE);
}

/**
 * @brief 注册编码器 A 相上升沿中断回调。
 */
errcode_t ws63_bsp_encoder_register_a_isr(ws63_bsp_gpio_callback_t callback)
{
    errcode_t ret;

    if (callback == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    (void)uapi_gpio_unregister_isr_func(WS63_ENCODER_A_PIN);
    ret = uapi_gpio_register_isr_func(WS63_ENCODER_A_PIN, GPIO_INTERRUPT_RISING_EDGE, callback);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_enable_interrupt(WS63_ENCODER_A_PIN);
}

/**
 * @brief 读取编码器 B 相电平。
 */
uint8_t ws63_bsp_encoder_get_b_level(void)
{
    return (uapi_gpio_get_val(WS63_ENCODER_B_PIN) == GPIO_LEVEL_HIGH) ? 1U : 0U;
}

/**
 * @brief 初始化调试串口。
 */
errcode_t ws63_bsp_debug_uart_init(uint8_t *rx_buffer, uint16_t rx_buffer_len)
{
    uart_attr_t attr = {
        .baud_rate = WS63_DEBUG_UART_BAUD,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };
    uart_pin_config_t pin_cfg = {
        .tx_pin = WS63_DEBUG_UART_TX_PIN,
        .rx_pin = WS63_DEBUG_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };
    uart_buffer_config_t rx_cfg;

    if ((rx_buffer == NULL) || (rx_buffer_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    (void)uapi_pin_set_mode(WS63_DEBUG_UART_TX_PIN, WS63_DEBUG_UART_PIN_MODE);
    (void)uapi_pin_set_mode(WS63_DEBUG_UART_RX_PIN, WS63_DEBUG_UART_PIN_MODE);

    rx_cfg.rx_buffer = rx_buffer;
    rx_cfg.rx_buffer_size = rx_buffer_len;

    /*
     * 先 deinit 再 init：
     * 避免调试命令口与系统其他 UART 用户（如 AT）共享同一总线时发生并发抢读。
     */
    (void)uapi_uart_deinit(WS63_DEBUG_UART_BUS);

    if (uapi_uart_init(WS63_DEBUG_UART_BUS, &pin_cfg, &attr, NULL, &rx_cfg) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 调试串口写数据。
 */
int32_t ws63_bsp_debug_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return uapi_uart_write(WS63_DEBUG_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 调试串口读数据。
 */
int32_t ws63_bsp_debug_uart_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    return uapi_uart_read(WS63_DEBUG_UART_BUS, data, len, timeout_ms);
}

/**
 * @brief 注册调试串口接收回调。
 */
errcode_t ws63_bsp_debug_uart_register_rx_callback(ws63_bsp_uart_rx_callback_t callback, uint16_t min_len)
{
    uint16_t threshold;

    if (callback == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    threshold = (min_len == 0U) ? 1U : min_len;
    if (uapi_uart_register_rx_callback(WS63_DEBUG_UART_BUS,
        UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE,
        threshold,
        callback) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}
