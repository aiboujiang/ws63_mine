/**
 * @file mine_rgb_led_drv.c
 * @brief Mine单颗RGB灯珠(WS2812B时序兼容)驱动实现。
 */

#include "mine_rgb_led.h"

#include <stdbool.h>

#include "pinctrl.h"
#include "pwm.h"
#include "soc_osal.h"
#include "tcxo.h"

/* WS2812B 的 DIN 改为 PWM 复用输出。 */
#define MINE_RGB_LED_PIN               GPIO_04
#define MINE_RGB_LED_PIN_MODE_PWM      PIN_MODE_1

/*
 * 根据 mine/lib/RGB_LED.pdf 的“数据传输定义”与注释门限选取时序：
 * 1) 表格范围：Tin0h(0.20~0.35us)、Tin1h(0.55~1.2us)、T0L(0.55~1.2us)、T1L(0.2~0.35us)；
 * 2) 注释门限：高电平 200~410ns 识别为“0”，640~1000ns 识别为“1”。
 * 这里取 T0H=300ns、T1H=700ns，可同时满足两组约束并留出抖动余量。
 */
#define MINE_RGB_T0H_NS                300U
#define MINE_RGB_T0L_NS                900U
#define MINE_RGB_T1H_NS                700U
#define MINE_RGB_T1L_NS                300U

/* PDF 注释给出 reset 最小 100us，这里取 120us 留出裕量。 */
#define MINE_RGB_RESET_US              120U

/* PWM 时钟来自 PWM 驱动，当前 WS63 端口固定为 40MHz。 */
#define MINE_PWM_CLOCK_HZ              40000000U

/* 选用 PWM4 + Group7 作为 WS2812 专用输出通道。 */
#define MINE_WS2812_PWM_CHANNEL        4U
#define MINE_WS2812_PWM_GROUP          7U

/* 单 bit 发送等待超时，防止异常情况下死循环阻塞任务。 */
#define MINE_WS2812_BIT_TIMEOUT_US     200U

/* 兼容 STM32 示例中的占空比编码：0 码=30，1 码=60（ARR=90）。 */
#define MINE_WS2812_DUTY_0             30U
#define MINE_WS2812_DUTY_1             60U
#define MINE_WS2812_DUTY_THRESHOLD     45U

uint16_t WS2812_Value[24U * Led_Num] = {0};
static volatile bool g_mine_ws2812_bit_done = false;
static bool g_mine_ws2812_inited = false;

/**
 * @brief WS2812 PWM 中断回调：标记单 bit 发送完成。
 *
 * @param channel 触发中断的 PWM 通道。
 * @return ERRCODE_SUCC。
 */
static errcode_t mine_ws2812_pwm_callback(uint8_t channel)
{
    if (channel == MINE_WS2812_PWM_CHANNEL) {
        g_mine_ws2812_bit_done = true;
    }
    return ERRCODE_SUCC;
}

/**
 * @brief 纳秒转换为 PWM 时钟计数（40MHz）。
 *
 * @param ns 时间，单位 ns。
 * @return 对应计数值，至少为 1。
 */
static inline uint32_t mine_ws2812_ns_to_ticks(uint32_t ns)
{
    uint64_t ticks = ((uint64_t)ns * (uint64_t)MINE_PWM_CLOCK_HZ + 999999999ULL) / 1000000000ULL;
    return (ticks == 0U) ? 1U : (uint32_t)ticks;
}

/**
 * @brief 通过 PWM 发送单个 WS2812 bit。
 *
 * @param bit_val bit 值（0 或 1）。
 * @return ERRCODE_SUCC 成功。
 * @return Other        失败。
 */
static errcode_t mine_ws2812_send_bit(uint8_t bit_val)
{
    errcode_t ret;
    uint32_t high_ticks;
    uint32_t low_ticks;
    uint64_t start_us;
    pwm_config_t cfg;

    high_ticks = mine_ws2812_ns_to_ticks(bit_val ? MINE_RGB_T1H_NS : MINE_RGB_T0H_NS);
    low_ticks = mine_ws2812_ns_to_ticks(bit_val ? MINE_RGB_T1L_NS : MINE_RGB_T0L_NS);

    cfg.low_time = low_ticks;
    cfg.high_time = high_ticks;
    cfg.offset_time = 0U;
    cfg.cycles = 1U;
    cfg.repeat = false;

    g_mine_ws2812_bit_done = false;
    ret = uapi_pwm_open(MINE_WS2812_PWM_CHANNEL, &cfg);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pwm_start_group(MINE_WS2812_PWM_GROUP);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    start_us = uapi_tcxo_get_us();
    while (!g_mine_ws2812_bit_done) {
        if ((uapi_tcxo_get_us() - start_us) > MINE_WS2812_BIT_TIMEOUT_US) {
            return ERRCODE_FAIL;
        }
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 将占空比值解释为 WS2812 单 bit。
 *
 * @param duty STM32 风格占空比值（典型为 30/60）。
 * @return 0/1 bit 值。
 */
static inline uint8_t mine_ws2812_duty_to_bit(uint16_t duty)
{
    return (duty > MINE_WS2812_DUTY_THRESHOLD) ? 1U : 0U;
}

/**
 * @brief 发送 WS2812_Value 前 bit_count 个 bit。
 *
 * @param bit_count 要发送的 bit 数。
 */
static void mine_ws2812_send_buffer_bits(uint32_t bit_count)
{
    uint32_t i;
    const uint32_t max_bits = 24U * Led_Num;

    if (bit_count > max_bits) {
        bit_count = max_bits;
    }

    if (!g_mine_ws2812_inited) {
        return;
    }

    /*
     * 逐 bit 由 PWM 硬件输出高低电平时长。
     * 若某一 bit 发送失败则终止该帧，避免输出错误帧污染后续显示。
     */
    for (i = 0U; i < bit_count; i++) {
        if (mine_ws2812_send_bit(mine_ws2812_duty_to_bit(WS2812_Value[i])) != ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] send bit fail at %u\r\n", (unsigned int)i);
            break;
        }
    }
}

/**
 * @brief 编码 1 字节到占空比缓冲区（MSB first）。
 *
 * @param value 待编码字节。
 * @param dst 对应的 8 个占空比槽位。
 */
static void mine_ws2812_encode_byte(uint8_t value, uint16_t *dst)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        dst[bit] = ((value & 0x80U) != 0U) ? MINE_WS2812_DUTY_1 : MINE_WS2812_DUTY_0;
        value <<= 1U;
    }
}

errcode_t mine_rgb_led_init(void)
{
    uint8_t channel_id = MINE_WS2812_PWM_CHANNEL;

    /* 1) 将 GPIO4 复用为 PWM 输出。 */
    uapi_pin_set_mode((uint8_t)MINE_RGB_LED_PIN, MINE_RGB_LED_PIN_MODE_PWM);

    /* 2) 初始化 PWM 并绑定中断回调。 */
    if (uapi_pwm_init() != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm init failed\r\n");
        return ERRCODE_FAIL;
    }
    (void)uapi_pwm_unregister_interrupt(MINE_WS2812_PWM_CHANNEL);
    if (uapi_pwm_register_interrupt(MINE_WS2812_PWM_CHANNEL, mine_ws2812_pwm_callback) != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm register irq failed\r\n");
        return ERRCODE_FAIL;
    }

    /* 3) 配置独立 PWM 分组，后续按 bit 启动 group 输出。 */
    (void)uapi_pwm_clear_group(MINE_WS2812_PWM_GROUP);
    if (uapi_pwm_set_group(MINE_WS2812_PWM_GROUP, &channel_id, 1U) != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] pwm set group failed\r\n");
        return ERRCODE_FAIL;
    }

    g_mine_ws2812_inited = true;

    /* 4) 上电后先送一次复位码，确保像素从干净状态开始。 */
    uapi_tcxo_delay_us(MINE_RGB_RESET_US);
    osal_printk("[mine_rgb_led] pwm init ok, ch=%u group=%u\r\n",
                (unsigned int)MINE_WS2812_PWM_CHANNEL,
                (unsigned int)MINE_WS2812_PWM_GROUP);
    return ERRCODE_SUCC;
}

void mine_rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    WS2812_SetRGB(0U, r, g, b);
    WS2812_Show(0U);
}

void mine_rgb_led_off(void)
{
    WS2812_SetRGB(0U, 0U, 0U, 0U);
    WS2812_Show(0U);
}

void WS2812_rest(void)
{
    /* 关闭 group 后 PWM 输出保持低电平，再等待复位时间。 */
    if (g_mine_ws2812_inited) {
        (void)uapi_pwm_stop_group(MINE_WS2812_PWM_GROUP);
    }
    uapi_tcxo_delay_us(MINE_RGB_RESET_US);
}

void WS2812_Clear(void)
{
    uint32_t i;
    const uint32_t max_bits = 24U * Led_Num;

    for (i = 0U; i < max_bits; i++) {
        WS2812_Value[i] = MINE_WS2812_DUTY_0;
    }
}

void WS2812_SetRGB(uint16_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t base;

    if (led_index >= Led_Num) {
        return;
    }

    /* WS2812 数据顺序固定为 GRB，且每字节 MSB first。 */
    base = (uint32_t)led_index * 24U;
    mine_ws2812_encode_byte(g, &WS2812_Value[base]);
    mine_ws2812_encode_byte(r, &WS2812_Value[base + 8U]);
    mine_ws2812_encode_byte(b, &WS2812_Value[base + 16U]);
}

void WS2812_Show(uint8_t Num)
{
    uint32_t led_count;

    if (!g_mine_ws2812_inited) {
        return;
    }

    led_count = (uint32_t)Num + 1U;
    if (led_count > Led_Num) {
        led_count = Led_Num;
    }

    /* 与 STM32 参考流程一致：发送前后都发送复位低电平。 */
    WS2812_rest();
    mine_ws2812_send_buffer_bits(24U * led_count);
    WS2812_rest();
}

void WS2812_Init(void)
{
    if (!g_mine_ws2812_inited) {
        if (mine_rgb_led_init() != ERRCODE_SUCC) {
            osal_printk("[mine_rgb_led] WS2812_Init failed\r\n");
            return;
        }
    }

    /*
     * 与 STM32 示例一致：
     * 1) 全灯数据清零并发送；
     * 2) 再补发 1 颗清零帧，尽量规避上电残色（常见为首颗绿残影）。
     */
    WS2812_Clear();
    WS2812_Show((uint8_t)(Led_Num - 1U));
    WS2812_Show(0U);
}
