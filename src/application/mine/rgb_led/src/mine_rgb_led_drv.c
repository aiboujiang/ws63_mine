/**
 * @file mine_rgb_led_drv.c
 * @brief Mine单颗RGB灯珠(WS2812B时序兼容)驱动实现。
 */

#include "mine_rgb_led.h"

#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "tcxo.h"
#include "interrupt/osal_interrupt.h"
#include "common_def.h"
#include "platform_core.h"

/*
 * 根据 mine/lib/RGB_LED.pdf 的“数据传输定义”与注释门限选取时序：
 * 1) 表格范围：Tin0h(0.20~0.35us)、Tin1h(0.55~1.2us)、T0L(0.55~1.2us)、T1L(0.2~0.35us)；
 * 2) 注释门限：高电平 200~410ns 识别为“0”，640~1000ns 识别为“1”。
 * 这里取 T0H=300ns、T1H=700ns，可同时满足两组约束并留出抖动余量。
 */
#define MINE_RGB_LED_PIN               GPIO_04
#define MINE_RGB_LED_PIN_MODE_GPIO     0

#define MINE_RGB_T0H_NS                300U
#define MINE_RGB_T0L_NS                900U
#define MINE_RGB_T1H_NS                700U
#define MINE_RGB_T1L_NS                300U

/* PDF 注释给出 reset 最小 100us，这里取 120us 留出裕量。 */
#define MINE_RGB_RESET_US              120U

/* GPIO4 位于 channel0/group0/pin4，对应 data_set/data_clr 的 bit4。 */
#define MINE_RGB_GPIO_DATA_SET_ADDR    (GPIO_CHANNEL_0_BASE_ADDR + 0x30U)
#define MINE_RGB_GPIO_DATA_CLR_ADDR    (GPIO_CHANNEL_0_BASE_ADDR + 0x34U)
#define MINE_RGB_GPIO4_MASK            (1U << 4U)

/* 校准周期窗口，窗口越长越稳但初始化耗时越大。 */
#define MINE_RGB_CALIBRATION_US        4000U

static uint32_t g_cycles_per_us = 0;

/**
 * @brief GPIO4 直接拉高（寄存器快路径）。
 */
static inline void mine_rgb_gpio_high(void)
{
    uapi_reg_write32(MINE_RGB_GPIO_DATA_SET_ADDR, MINE_RGB_GPIO4_MASK);
}

/**
 * @brief GPIO4 直接拉低（寄存器快路径）。
 */
static inline void mine_rgb_gpio_low(void)
{
    uapi_reg_write32(MINE_RGB_GPIO_DATA_CLR_ADDR, MINE_RGB_GPIO4_MASK);
}

/**
 * @brief 读取当前CPU cycle计数。
 *
 * @return 当前cycle计数(32位)。
 */
static inline uint32_t mine_rgb_read_cycle32(void)
{
    uint32_t value;
    __asm__ __volatile__("csrr %0, cycle" : "=r"(value));
    return value;
}

/**
 * @brief 忙等指定cycle数，用于生成子微秒脉宽。
 *
 * @param wait_cycles 需要等待的cycle数量。
 */
static inline void mine_rgb_wait_cycles(uint32_t wait_cycles)
{
    uint32_t start = mine_rgb_read_cycle32();
    while ((uint32_t)(mine_rgb_read_cycle32() - start) < wait_cycles) {
        /* busy wait */
    }
}

/**
 * @brief 纳秒转换为cycle数。
 *
 * @param ns 时间，单位ns。
 * @return 对应的cycle数，至少返回1，避免脉冲被编译器优化掉。
 */
static inline uint32_t mine_rgb_ns_to_cycles(uint32_t ns)
{
    uint64_t cycles = ((uint64_t)g_cycles_per_us * (uint64_t)ns + 999ULL) / 1000ULL;
    return (cycles == 0U) ? 1U : (uint32_t)cycles;
}

/**
 * @brief 发送单个bit到灯珠DI。
 *
 * @param bit_val bit值，0或1。
 */
static inline void mine_rgb_send_bit(uint8_t bit_val)
{
    const uint32_t high_cycles = bit_val ? mine_rgb_ns_to_cycles(MINE_RGB_T1H_NS) : mine_rgb_ns_to_cycles(MINE_RGB_T0H_NS);
    const uint32_t low_cycles = bit_val ? mine_rgb_ns_to_cycles(MINE_RGB_T1L_NS) : mine_rgb_ns_to_cycles(MINE_RGB_T0L_NS);

    mine_rgb_gpio_high();
    mine_rgb_wait_cycles(high_cycles);
    mine_rgb_gpio_low();
    mine_rgb_wait_cycles(low_cycles);
}

/**
 * @brief 按MSB first发送1个字节。
 *
 * @param value 待发送字节。
 */
static inline void mine_rgb_send_byte(uint8_t value)
{
    for (uint8_t bit = 0; bit < 8U; bit++) {
        mine_rgb_send_bit((uint8_t)((value & 0x80U) != 0U));
        value <<= 1U;
    }
}

errcode_t mine_rgb_led_init(void)
{
    uint64_t us_start;
    uint64_t us_end;
    uint32_t cycle_start;
    uint32_t cycle_end;
    uint64_t delta_us;
    uint32_t delta_cycle;

    /* 1) GPIO4切为普通GPIO输出，默认拉低，防止上电误点亮。 */
    uapi_pin_set_mode((uint8_t)MINE_RGB_LED_PIN, MINE_RGB_LED_PIN_MODE_GPIO);
    if (uapi_gpio_set_dir(MINE_RGB_LED_PIN, GPIO_DIRECTION_OUTPUT) != ERRCODE_SUCC) {
        osal_printk("[mine_rgb_led] gpio4 set output failed\r\n");
        return ERRCODE_FAIL;
    }
    mine_rgb_gpio_low();

    /*
     * 2) 通过TCXO微秒计时窗口校准 cycles/us。
     *    这样在不同频点下都能用cycle等待产生稳定子微秒脉冲。
     */
    us_start = uapi_tcxo_get_us();
    cycle_start = mine_rgb_read_cycle32();
    uapi_tcxo_delay_us(MINE_RGB_CALIBRATION_US);
    us_end = uapi_tcxo_get_us();
    cycle_end = mine_rgb_read_cycle32();

    delta_us = us_end - us_start;
    delta_cycle = (uint32_t)(cycle_end - cycle_start);
    if (delta_us == 0U || delta_cycle == 0U) {
        osal_printk("[mine_rgb_led] calib invalid, us=%llu cycle=%u\r\n",
                    (unsigned long long)delta_us,
                    (unsigned int)delta_cycle);
        return ERRCODE_FAIL;
    }

    g_cycles_per_us = (uint32_t)((uint64_t)delta_cycle / delta_us);
    if (g_cycles_per_us == 0U) {
        osal_printk("[mine_rgb_led] calib fail, cycles_per_us=0 (us=%llu cycle=%u)\r\n",
                    (unsigned long long)delta_us,
                    (unsigned int)delta_cycle);
        return ERRCODE_FAIL;
    }

    osal_printk("[mine_rgb_led] calib ok, cycles_per_us=%u\r\n", (unsigned int)g_cycles_per_us);

    /* 3) 上电后先送一次复位码，确保像素从干净状态开始。 */
    uapi_tcxo_delay_us(MINE_RGB_RESET_US);
    return ERRCODE_SUCC;
}

void mine_rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t irq_state;

    if (g_cycles_per_us == 0U) {
        return;
    }

    /*
     * WS2812B为GRB顺序且对时序敏感。
        * PDF 注释要求：一帧传输中断时间不要超过 35us，否则可能被误判为 RESET。
        * 因此发送 24bit 期间关闭中断，避免被抢占打断。
     */
    irq_state = osal_irq_lock();
    mine_rgb_send_byte(g);
    mine_rgb_send_byte(r);
    mine_rgb_send_byte(b);
    osal_irq_restore(irq_state);

    /* 帧结束后保持低电平，触发锁存。 */
    uapi_tcxo_delay_us(MINE_RGB_RESET_US);
}

void mine_rgb_led_off(void)
{
    mine_rgb_led_set_rgb(0U, 0U, 0U);
}
