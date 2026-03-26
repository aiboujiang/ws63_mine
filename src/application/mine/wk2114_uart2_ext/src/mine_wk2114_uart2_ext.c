/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块主实现。
 */

#include "mine_wk2114_uart2_ext_module.h"

#include <stdbool.h>
#include <stdarg.h>

#include "app_init.h"
#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef UART_RX_CONDITION_MASK_SUFFICIENT_DATA
#define UART_RX_CONDITION_MASK_SUFFICIENT_DATA 2
#endif

#ifndef UART_RX_CONDITION_MASK_FULL
#define UART_RX_CONDITION_MASK_FULL 4
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/* 保留原始日志出口，避免递归调用。 */
static void (*g_mine_wk2114_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/* UART2 主口接收缓存。 */
static uint8_t g_mine_wk2114_uart_rx_buffer[MINE_WK2114_HOST_UART_RX_BUFFER_SIZE] = {0};

/* 读寄存器响应 FIFO。 */
static uint8_t g_mine_wk2114_host_resp_fifo[MINE_WK2114_HOST_RESP_FIFO_SIZE] = {0};
static volatile uint16_t g_mine_wk2114_host_resp_head = 0;
static volatile uint16_t g_mine_wk2114_host_resp_tail = 0;

/* 模块运行状态。 */
static bool g_mine_wk2114_ready = false;
/* 记录最近成功的 UART2 profile，下一次优先从该配置开始。 */
static uint8_t g_mine_wk2114_uart_profile_index = 0;
static uint8_t g_mine_wk2114_gena_shadow = MINE_WK2114_GENA_RESERVED_MASK;
static uint32_t g_mine_wk2114_subuart_baud[MINE_WK2114_SUBUART_COUNT] = {
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
};
static bool g_mine_wk2114_subuart_ready[MINE_WK2114_SUBUART_COUNT] = { false, false, false, false };

/* IRQ 统计状态。 */
static volatile bool g_mine_wk2114_irq_pending = false;
static volatile uint32_t g_mine_wk2114_irq_count = 0;

/**
 * @brief 临界区加锁。
 */
static unsigned int mine_wk2114_irq_lock(void)
{
    return osal_irq_lock();
}

/**
 * @brief 临界区解锁。
 */
static void mine_wk2114_irq_unlock(unsigned int irq_state)
{
    osal_irq_restore(irq_state);
}

/**
 * @brief 检查子串口号是否合法。
 */
static bool mine_wk2114_channel_valid(uint8_t channel)
{
    return ((channel >= MINE_WK2114_SUBUART_MIN) && (channel <= MINE_WK2114_SUBUART_MAX));
}

/**
 * @brief 子串口号转索引（1~4 -> 0~3）。
 */
static uint8_t mine_wk2114_channel_index(uint8_t channel)
{
    return (uint8_t)(channel - MINE_WK2114_SUBUART_MIN);
}

/**
 * @brief 构造子串口 6bit 地址：C1C0 + A3A2A1A0。
 */
static uint8_t mine_wk2114_make_sub_addr(uint8_t channel, uint8_t reg4)
{
    uint8_t c1c0 = mine_wk2114_channel_index(channel);
    return (uint8_t)(((c1c0 & 0x03U) << 4) | (reg4 & 0x0FU));
}

/**
 * @brief 统一日志输出：OSAL + PRINT + UART0。
 */
void mine_wk2114_log(const char *fmt, ...)
{
    char log_buf[MINE_WK2114_LOG_BUFFER_LEN] = {0};
    va_list args;
    int32_t n;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    n = vsnprintf_s(log_buf, sizeof(log_buf), sizeof(log_buf) - 1, fmt, args);
    va_end(args);
    if (n <= 0) {
        return;
    }

    g_mine_wk2114_raw_osal_printk("%s", log_buf);
    PRINT("%s", log_buf);
    (void)uapi_uart_write(UART_BUS_0, (const uint8_t *)log_buf, (uint16_t)n, 0);
}

/**
 * @brief IRQ 回调：只做最小动作，置位并计数。
 */
static void mine_wk2114_irq_handler(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);

    g_mine_wk2114_irq_count++;
    g_mine_wk2114_irq_pending = true;
}

/**
 * @brief 配置 IRQ 引脚（GPIO13，低电平有效）。
 */
static errcode_t mine_wk2114_irq_gpio_init(void)
{
    errcode_t ret;

    uapi_gpio_init();
    (void)uapi_pin_set_mode(MINE_WK2114_IRQ_GPIO_PIN, MINE_WK2114_IRQ_PIN_MODE);
    (void)uapi_pin_set_pull(MINE_WK2114_IRQ_GPIO_PIN, PIN_PULL_TYPE_UP);

    ret = uapi_gpio_set_dir(MINE_WK2114_IRQ_GPIO_PIN, GPIO_DIRECTION_INPUT);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] irq dir failed, ret=%x\r\n", ret);
        return ret;
    }

    (void)uapi_gpio_unregister_isr_func(MINE_WK2114_IRQ_GPIO_PIN);
    ret = uapi_gpio_register_isr_func(MINE_WK2114_IRQ_GPIO_PIN, MINE_WK2114_IRQ_TRIGGER_MODE, mine_wk2114_irq_handler);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] irq register failed, ret=%x\r\n", ret);
        return ret;
    }

    ret = uapi_gpio_enable_interrupt(MINE_WK2114_IRQ_GPIO_PIN);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] irq enable failed, ret=%x\r\n", ret);
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 执行硬件复位：高10ms -> 低1500ms -> 高20ms。
 */
static errcode_t mine_wk2114_hw_reset_chip(void)
{
    errcode_t ret;

    uapi_gpio_init();
    (void)uapi_pin_set_mode(MINE_WK2114_RESET_GPIO_PIN, HAL_PIO_FUNC_GPIO);
    (void)uapi_pin_set_pull(MINE_WK2114_RESET_GPIO_PIN, PIN_PULL_TYPE_UP);

    ret = uapi_gpio_set_dir(MINE_WK2114_RESET_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset dir failed, ret=%x\r\n", ret);
        return ret;
    }

    /* 关键流程注释：先给高电平稳定，再执行低脉冲复位。 */
    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pre-high failed, ret=%x\r\n", ret);
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_HOLD_MS);

    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pull-low failed, ret=%x\r\n", ret);
        return ret;
    }
    /* 关键流程注释：拉长低电平保持时间，验证 RST 脉冲是否被芯片可靠识别。 */
    osal_msleep(MINE_WK2114_RESET_LOW_HOLD_MS);

#if (MINE_WK2114_RESET_FORCE_LOW_ONLY == 1U)
    mine_wk2114_log("[mine wk2114] reset force-low mode enabled\r\n");
    return ERRCODE_SUCC;
#endif

    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset release-high failed, ret=%x\r\n", ret);
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_RELEASE_WAIT_MS);

    mine_wk2114_log("[mine wk2114] hw reset pulse done on gpio10\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 清空软件响应 FIFO。
 */
static void mine_wk2114_host_resp_fifo_reset(void)
{
    unsigned int irq_state = mine_wk2114_irq_lock();
    g_mine_wk2114_host_resp_head = 0;
    g_mine_wk2114_host_resp_tail = 0;
    mine_wk2114_irq_unlock(irq_state);
}

/**
 * @brief 清理主口硬件 RX FIFO 中的残留字节。
 *
 * 关键流程注释：上一轮失败后若残留脏字节，会污染下一次读寄存器结果，
 * 因此在发起读命令前先做一次有限清理。
 */
static void mine_wk2114_host_uart_rx_drain(void)
{
    uint8_t byte = 0;
    uint16_t drain_count = 0;

    while ((uapi_uart_read(MINE_WK2114_HOST_UART_BUS, &byte, 1, 0) > 0) &&
        (drain_count < MINE_WK2114_HOST_RX_DRAIN_MAX)) {
        drain_count++;
    }

    if (drain_count > 0U) {
        mine_wk2114_log("[mine wk2114] drain stale host rx=%u\r\n", (unsigned int)drain_count);
    }
}

/**
 * @brief 向软件 FIFO 写 1 字节（满则覆盖最旧数据）。
 */
static void mine_wk2114_host_resp_fifo_push(uint8_t byte)
{
    uint16_t next_head;
    unsigned int irq_state = mine_wk2114_irq_lock();

    next_head = (uint16_t)((g_mine_wk2114_host_resp_head + 1U) % MINE_WK2114_HOST_RESP_FIFO_SIZE);
    if (next_head == g_mine_wk2114_host_resp_tail) {
        g_mine_wk2114_host_resp_tail = (uint16_t)((g_mine_wk2114_host_resp_tail + 1U) % MINE_WK2114_HOST_RESP_FIFO_SIZE);
    }

    g_mine_wk2114_host_resp_fifo[g_mine_wk2114_host_resp_head] = byte;
    g_mine_wk2114_host_resp_head = next_head;
    mine_wk2114_irq_unlock(irq_state);
}

/**
 * @brief 从软件 FIFO 取 1 字节。
 */
static bool mine_wk2114_host_resp_fifo_pop(uint8_t *byte)
{
    unsigned int irq_state;

    if (byte == NULL) {
        return false;
    }

    irq_state = mine_wk2114_irq_lock();
    if (g_mine_wk2114_host_resp_head == g_mine_wk2114_host_resp_tail) {
        mine_wk2114_irq_unlock(irq_state);
        return false;
    }

    *byte = g_mine_wk2114_host_resp_fifo[g_mine_wk2114_host_resp_tail];
    g_mine_wk2114_host_resp_tail = (uint16_t)((g_mine_wk2114_host_resp_tail + 1U) % MINE_WK2114_HOST_RESP_FIFO_SIZE);
    mine_wk2114_irq_unlock(irq_state);
    return true;
}

/**
 * @brief 主口 RX 回调：将数据放入响应 FIFO。
 */
static void mine_wk2114_uart_rx_handler(const void *buffer, uint16_t length, bool error)
{
    const uint8_t *data = (const uint8_t *)buffer;
    uint16_t i;

    if (error) {
        mine_wk2114_log("[mine wk2114] host rx error\r\n");
    }
    if ((data == NULL) || (length == 0U)) {
        return;
    }

    for (i = 0; i < length; i++) {
        mine_wk2114_host_resp_fifo_push(data[i]);
    }
}

/**
 * @brief 发送主口帧。
 */
static errcode_t mine_wk2114_send_host_frame(const uint8_t *frame, uint16_t len)
{
    int32_t wlen;

    if ((frame == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (!g_mine_wk2114_ready) {
        return ERRCODE_UART_NOT_INIT;
    }

    wlen = uapi_uart_write(MINE_WK2114_HOST_UART_BUS, frame, len, 0);
    if (wlen != (int32_t)len) {
        mine_wk2114_log("[mine wk2114] host tx fail, want=%u ret=%ld\r\n", (unsigned int)len, (long)wlen);
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 向 6bit 地址写 1 字节寄存器。
 */
static errcode_t mine_wk2114_write_addr6(uint8_t addr6, uint8_t value)
{
    uint8_t frame[2] = {0};

    frame[0] = (uint8_t)(MINE_WK2114_HOST_CMD_WRITE_REG | (addr6 & 0x3FU));
    frame[1] = value;
    return mine_wk2114_send_host_frame(frame, sizeof(frame));
}

/**
 * @brief 执行一次读寄存器命令。
 *
 * @param addr6 6bit 寄存器地址。
 * @param with_dummy true=发送 cmd+dummy 两字节，false=仅发送 cmd 一字节。
 * @param value 读到的寄存器值输出。
 * @return errcode_t
 */
static errcode_t mine_wk2114_read_addr6_once(uint8_t addr6, bool with_dummy, uint8_t *value)
{
    uint8_t frame[2] = {0};
    uint16_t frame_len;
    uint8_t rx;
    uint8_t last = 0;
    bool got_data = false;
    bool got_new_data;
    uint32_t start_ms;
    uint32_t stable_wait_ms = 0;
    errcode_t ret;

    if (value == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    frame[0] = (uint8_t)(MINE_WK2114_HOST_CMD_READ_REG | (addr6 & 0x3FU));
    frame[1] = MINE_WK2114_HOST_READ_DUMMY_BYTE;
    frame_len = with_dummy ? 2U : 1U;

    mine_wk2114_host_uart_rx_drain();
    mine_wk2114_host_resp_fifo_reset();
    ret = mine_wk2114_send_host_frame(frame, frame_len);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    start_ms = (uint32_t)uapi_systick_get_ms();
    while ((uint32_t)(uapi_systick_get_ms() - start_ms) <= MINE_WK2114_HOST_READ_TIMEOUT_MS) {
        got_new_data = false;

        while (mine_wk2114_host_resp_fifo_pop(&rx)) {
            last = rx;
            got_data = true;
            got_new_data = true;
            stable_wait_ms = 0;
        }

        /* 关键流程注释：收到数据后等待短稳定窗口，避免读到半包。 */
        if (got_data && (!got_new_data)) {
            stable_wait_ms++;
            if (stable_wait_ms >= MINE_WK2114_HOST_RESP_STABLE_WAIT_MS) {
                *value = last;
                return ERRCODE_SUCC;
            }
        }

        /* 回调没进时，轮询硬件 FIFO 兜底。 */
        if (uapi_uart_read(MINE_WK2114_HOST_UART_BUS, &rx, 1, 0) > 0) {
            last = rx;
            got_data = true;
            stable_wait_ms = 0;
        } else {
            osal_msleep(1);
        }
    }

    if (got_data) {
        *value = last;
        return ERRCODE_SUCC;
    }

    return ERRCODE_FAIL;
}

/**
 * @brief 读取 6bit 地址寄存器（兼容两种主口读命令格式）。
 */
static errcode_t mine_wk2114_read_addr6(uint8_t addr6, uint8_t *value)
{
    errcode_t ret;

    if (value == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    /* 关键流程注释：优先按单字节读命令尝试，失败后自动回退到 cmd+dummy。 */
    ret = mine_wk2114_read_addr6_once(addr6, false, value);
    if (ret == ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] read addr6=0x%02X by cmd-only\r\n", (unsigned int)addr6);
        return ERRCODE_SUCC;
    }

    /*
     * 关键流程注释：按规格书，主口读寄存器默认仅发送 1 字节 CMD。
     * cmd+dummy 仅作为可选排障路径，默认关闭避免偏离标准时序。
     */
#if (MINE_WK2114_HOST_READ_DUMMY_FALLBACK_ENABLE == 1U)
    ret = mine_wk2114_read_addr6_once(addr6, true, value);
    if (ret == ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] read addr6=0x%02X by cmd+dummy\r\n", (unsigned int)addr6);
        return ERRCODE_SUCC;
    }
#endif

    return ERRCODE_FAIL;
}

/**
 * @brief 读寄存器重试。
 */
static errcode_t mine_wk2114_read_addr6_retry(uint8_t addr6, uint8_t retry_max, uint8_t *value)
{
    uint8_t i;
    errcode_t ret;

    if ((value == NULL) || (retry_max == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    for (i = 0; i < retry_max; i++) {
        ret = mine_wk2114_read_addr6(addr6, value);
        if (ret == ERRCODE_SUCC) {
            return ERRCODE_SUCC;
        }
        osal_msleep(1);
    }

    return ERRCODE_FAIL;
}

/**
 * @brief 校验寄存器固定值。
 */
static errcode_t mine_wk2114_verify_register_value(uint8_t addr6, uint8_t expected, const char *reg_name)
{
    uint8_t value = 0;
    errcode_t ret;

    ret = mine_wk2114_read_addr6_retry(addr6, MINE_WK2114_LINK_CHECK_READ_RETRY, &value);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] read %s timeout\r\n", (reg_name == NULL) ? "REG" : reg_name);
        return ret;
    }

    if (value != expected) {
        mine_wk2114_log("[mine wk2114] verify %s mismatch got=0x%02X expect=0x%02X\r\n",
            (reg_name == NULL) ? "REG" : reg_name,
            (unsigned int)value,
            (unsigned int)expected);
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 写后读回校验。
 */
static errcode_t mine_wk2114_write_readback_verify(uint8_t addr6, uint8_t value, const char *reg_name)
{
    errcode_t ret;

    ret = mine_wk2114_write_addr6(addr6, value);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return mine_wk2114_verify_register_value(addr6, value, reg_name);
}

/**
 * @brief 执行 0x55 主口波特率自适应。
 */
static errcode_t mine_wk2114_send_autobaud_sync_sequence(void)
{
    uint8_t sync = MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE;
    uint8_t i;
    errcode_t ret;

    for (i = 0; i < MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY; i++) {
        ret = mine_wk2114_send_host_frame(&sync, 1);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        if (MINE_WK2114_HOST_AUTOBAUD_SYNC_INTERVAL_MS > 0U) {
            osal_msleep(MINE_WK2114_HOST_AUTOBAUD_SYNC_INTERVAL_MS);
        }
    }

    mine_wk2114_log("[mine wk2114] sync55 done count=%u irq_level=%u\r\n",
        (unsigned int)MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY,
        (unsigned int)uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN));

    /* 关键流程注释：应用笔记要求发 0x55 后等待锁定。 */
    osal_msleep(MINE_WK2114_HOST_AUTOBAUD_LOCK_WAIT_MS);
    return ERRCODE_SUCC;
}

/**
 * @brief 链路检查：复位 -> 0x55 -> 读GENA -> 写读回GENA。
 */
static errcode_t mine_wk2114_check_link_ready(void)
{
    uint8_t gena_test;
    errcode_t ret;

    ret = mine_wk2114_hw_reset_chip();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

#if (MINE_WK2114_RESET_FORCE_LOW_ONLY == 1U)
    return ERRCODE_FAIL;
#endif

    ret = mine_wk2114_send_autobaud_sync_sequence();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] sync 0x55 failed\r\n");
        return ret;
    }

    /* C: 固定寄存器读校验，GENA 默认应为 0xF0。 */
    ret = mine_wk2114_verify_register_value(MINE_WK2114_ADDR_GENA, MINE_WK2114_GENA_RESERVED_MASK, "GENA");
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] link check read GENA timeout\r\n");
        return ret;
    }

    /* D: 写读回校验，先置 UT1EN，再恢复默认值。 */
    gena_test = (uint8_t)(MINE_WK2114_GENA_RESERVED_MASK | 0x01U);
    ret = mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GENA, gena_test, "GENA");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GENA, MINE_WK2114_GENA_RESERVED_MASK, "GENA");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_mine_wk2114_gena_shadow = MINE_WK2114_GENA_RESERVED_MASK;
    mine_wk2114_log("[mine wk2114] link ok, GENA=0x%02X\r\n", (unsigned int)g_mine_wk2114_gena_shadow);
    return ERRCODE_SUCC;
}

/**
 * @brief 波特率寄存器计算（应用笔记公式）。
 */
static errcode_t mine_wk2114_calc_baud_param(uint32_t baud_rate, uint16_t *baud_reg, uint8_t *pres)
{
    uint64_t reg_x10;

    if ((baud_rate == 0U) || (baud_reg == NULL) || (pres == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    reg_x10 = ((uint64_t)MINE_WK2114_XTAL_HZ * 10ULL + (uint64_t)(16U * baud_rate / 2U)) / (uint64_t)(16U * baud_rate);
    if (reg_x10 < 10ULL) {
        return ERRCODE_INVALID_PARAM;
    }

    *baud_reg = (uint16_t)((reg_x10 / 10ULL) - 1ULL);
    *pres = (uint8_t)(reg_x10 % 10ULL);
    return ERRCODE_SUCC;
}

/**
 * @brief 使能 GENA 对应子串口时钟位。
 */
static errcode_t mine_wk2114_enable_global_channel(uint8_t channel)
{
    uint8_t bit_index;

    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }

    bit_index = mine_wk2114_channel_index(channel);
    g_mine_wk2114_gena_shadow = (uint8_t)((g_mine_wk2114_gena_shadow | MINE_WK2114_GENA_RESERVED_MASK) | (uint8_t)(1U << bit_index));

    return mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GENA, g_mine_wk2114_gena_shadow, "GENA");
}

/**
 * @brief 配置子串口寄存器并做读回校验。
 */
static errcode_t mine_wk2114_config_subuart(uint8_t channel, uint32_t baud_rate)
{
    uint8_t addr;
    uint16_t baud_reg = 0;
    uint8_t pres = 0;
    errcode_t ret;

    ret = mine_wk2114_calc_baud_param(baud_rate, &baud_reg, &pres);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* E: 先切 PAGE1 配波特率参数。 */
    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SPAGE);
    ret = mine_wk2114_write_readback_verify(addr, 0x01U, "SPAGE(P1)");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_BAUD1);
    ret = mine_wk2114_write_readback_verify(addr, (uint8_t)((baud_reg >> 8) & 0xFFU), "BAUD1");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_BAUD0);
    ret = mine_wk2114_write_readback_verify(addr, (uint8_t)(baud_reg & 0xFFU), "BAUD0");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_PRES);
    ret = mine_wk2114_write_readback_verify(addr, (uint8_t)(pres & 0x0FU), "PRES");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 切回 PAGE0，打开收发与 FIFO。 */
    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SPAGE);
    ret = mine_wk2114_write_readback_verify(addr, 0x00U, "SPAGE(P0)");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SCR);
    ret = mine_wk2114_write_readback_verify(addr, 0x03U, "SCR");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_FCR);
    ret = mine_wk2114_write_addr6(addr, 0x0FU);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    /* FCR 的复位位会自动清 0，回读只校验触点位。 */
    ret = mine_wk2114_verify_register_value(addr, 0x0CU, "FCR");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_enable_global_channel(channel);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_mine_wk2114_subuart_baud[mine_wk2114_channel_index(channel)] = baud_rate;
    g_mine_wk2114_subuart_ready[mine_wk2114_channel_index(channel)] = true;
    mine_wk2114_log("[mine wk2114] sub%u configured, baud=%lu\r\n", (unsigned int)channel, (unsigned long)baud_rate);
    return ERRCODE_SUCC;
}

/**
 * @brief FIFO 长度编码。
 */
static uint8_t mine_wk2114_fifo_len_to_nibble(uint16_t len)
{
    if (len <= 1U) {
        return 0x00;
    }
    if (len >= 16U) {
        return 0x0FU;
    }
    return (uint8_t)len;
}

/**
 * @brief 发送单个 FIFO 分片（最多 16 字节）。
 */
static errcode_t mine_wk2114_send_fifo_chunk(uint8_t channel, const uint8_t *data, uint16_t len)
{
    uint8_t frame[MINE_WK2114_UART_FRAME_MAX] = {0};
    uint8_t cmd;

    if ((data == NULL) || (len == 0U) || (len > MINE_WK2114_FIFO_CHUNK_MAX)) {
        return ERRCODE_INVALID_PARAM;
    }

    cmd = (uint8_t)(MINE_WK2114_HOST_CMD_WRITE_FIFO |
        ((mine_wk2114_channel_index(channel) & 0x03U) << 4) |
        mine_wk2114_fifo_len_to_nibble(len));

    frame[0] = cmd;
    if (memcpy_s(&frame[1], sizeof(frame) - 1, data, len) != EOK) {
        return ERRCODE_FAIL;
    }

    return mine_wk2114_send_host_frame(frame, (uint16_t)(len + 1U));
}

/**
 * @brief 构建 UART2 初始化 profile（复用模式与收发方向组合）。
 *
 * profile 说明：
 * 0: tx=8 rx=7 mode=默认模式
 * 1: tx=8 rx=7 mode=备选模式
 * 2: tx=7 rx=8 mode=默认模式（收发互换）
 * 3: tx=7 rx=8 mode=备选模式（收发互换）
 */
static void mine_wk2114_build_uart_profile(uint8_t profile_idx, uart_pin_config_t *pin_cfg, uint8_t *pin_mode)
{
    if ((pin_cfg == NULL) || (pin_mode == NULL)) {
        return;
    }

    switch (profile_idx) {
        case 1:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            *pin_mode = MINE_WK2114_HOST_UART_ALT_PIN_MODE;
            break;
        case 2:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            *pin_mode = MINE_WK2114_HOST_UART_PIN_MODE;
            break;
        case 3:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            *pin_mode = MINE_WK2114_HOST_UART_ALT_PIN_MODE;
            break;
        case 0:
        default:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            *pin_mode = MINE_WK2114_HOST_UART_PIN_MODE;
            break;
    }

    pin_cfg->cts_pin = PIN_NONE;
    pin_cfg->rts_pin = PIN_NONE;
}

/**
 * @brief 初始化 UART2 主口和回调。
 */
errcode_t mine_wk2114_uart2_ext_init(void)
{
    uart_attr_t attr = {
        .baud_rate = MINE_WK2114_HOST_UART_BAUD,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE,
    };
    uart_pin_config_t pin_cfg = {0};
    uart_buffer_config_t buf_cfg = {0};
    uint8_t profile_try;
    uint8_t profile_idx;
    uint8_t pin_mode = MINE_WK2114_HOST_UART_PIN_MODE;
    errcode_t ret;

    if (g_mine_wk2114_ready) {
        return ERRCODE_SUCC;
    }

    buf_cfg.rx_buffer = g_mine_wk2114_uart_rx_buffer;
    buf_cfg.rx_buffer_size = sizeof(g_mine_wk2114_uart_rx_buffer);

    ret = mine_wk2114_irq_gpio_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    for (profile_try = 0; profile_try < MINE_WK2114_HOST_UART_PROFILE_COUNT; profile_try++) {
        profile_idx = (uint8_t)((g_mine_wk2114_uart_profile_index + profile_try) % MINE_WK2114_HOST_UART_PROFILE_COUNT);
        mine_wk2114_build_uart_profile(profile_idx, &pin_cfg, &pin_mode);

        mine_wk2114_log("[mine wk2114] try profile=%u tx=%u rx=%u mode=%u irq=%u\r\n",
            (unsigned int)profile_idx,
            (unsigned int)pin_cfg.tx_pin,
            (unsigned int)pin_cfg.rx_pin,
            (unsigned int)pin_mode,
            (unsigned int)uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN));

        uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
        uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

        (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
        ret = uapi_uart_init(MINE_WK2114_HOST_UART_BUS, &pin_cfg, &attr, NULL, &buf_cfg);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] uart init profile=%u failed, ret=%x\r\n",
                (unsigned int)profile_idx,
                ret);
            continue;
        }

#if defined(CONFIG_UART_SUPPORT_RX)
        ret = uapi_uart_register_rx_callback(MINE_WK2114_HOST_UART_BUS,
            (UART_RX_CONDITION_MASK_FULL | UART_RX_CONDITION_MASK_SUFFICIENT_DATA | UART_RX_CONDITION_MASK_IDLE),
            1,
            mine_wk2114_uart_rx_handler);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] rx callback profile=%u failed, ret=%x\r\n",
                (unsigned int)profile_idx,
                ret);
            (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
            continue;
        }
#else
        mine_wk2114_log("[mine wk2114] uart rx callback unsupported\r\n");
        return ERRCODE_FAIL;
#endif

        g_mine_wk2114_ready = true;
        ret = mine_wk2114_check_link_ready();
        if (ret == ERRCODE_SUCC) {
            g_mine_wk2114_uart_profile_index = profile_idx;
            mine_wk2114_log("[mine wk2114] uart profile ok idx=%u tx=%u rx=%u mode=%u\r\n",
                (unsigned int)profile_idx,
                (unsigned int)pin_cfg.tx_pin,
                (unsigned int)pin_cfg.rx_pin,
                (unsigned int)pin_mode);
            return ERRCODE_SUCC;
        }

        mine_wk2114_log("[mine wk2114] link fail profile=%u ret=%x\r\n",
            (unsigned int)profile_idx,
            ret);
        g_mine_wk2114_ready = false;
        (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
    }

    mine_wk2114_log("[mine wk2114] all uart profile failed\r\n");
    return ERRCODE_FAIL;
}

/**
 * @brief 配置子串口波特率。
 */
errcode_t mine_wk2114_uart2_ext_set_subuart_baud(uint8_t channel, uint32_t baud_rate)
{
    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (!g_mine_wk2114_ready) {
        return ERRCODE_UART_NOT_INIT;
    }

    return mine_wk2114_config_subuart(channel, baud_rate);
}

/**
 * @brief 子串口发送。
 */
errcode_t mine_wk2114_uart2_ext_send(uint8_t channel, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0;
    uint16_t chunk;
    errcode_t ret;

    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (!g_mine_wk2114_ready) {
        return ERRCODE_UART_NOT_INIT;
    }

    /* 首次发送自动配置子串口。 */
    if (!g_mine_wk2114_subuart_ready[mine_wk2114_channel_index(channel)]) {
        ret = mine_wk2114_config_subuart(channel, g_mine_wk2114_subuart_baud[mine_wk2114_channel_index(channel)]);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    while (offset < len) {
        chunk = (uint16_t)(len - offset);
        if (chunk > MINE_WK2114_FIFO_CHUNK_MAX) {
            chunk = MINE_WK2114_FIFO_CHUNK_MAX;
        }

        ret = mine_wk2114_send_fifo_chunk(channel, &data[offset], chunk);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] sub%u tx failed\r\n", (unsigned int)channel);
            return ret;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 启动重试流程：先主口，再子串口1。
 */
static errcode_t mine_wk2114_bootstrap_with_retry(void)
{
    errcode_t ret;
    uint32_t retry = 0;

    while (1) {
        ret = mine_wk2114_uart2_ext_init();
        if (ret == ERRCODE_SUCC) {
            break;
        }

        retry++;
        mine_wk2114_log("[mine wk2114] init retry #%lu ret=%x\r\n", (unsigned long)retry, ret);
        osal_msleep(MINE_WK2114_INIT_RETRY_WAIT_MS);
    }

    retry = 0;
    while (1) {
        ret = mine_wk2114_uart2_ext_set_subuart_baud(1, g_mine_wk2114_subuart_baud[0]);
        if (ret == ERRCODE_SUCC) {
            return ERRCODE_SUCC;
        }

        retry++;
        mine_wk2114_log("[mine wk2114] sub1 cfg retry #%lu ret=%x\r\n", (unsigned long)retry, ret);
        osal_msleep(MINE_WK2114_INIT_RETRY_WAIT_MS);
    }
}

/**
 * @brief 后台任务：处理 IRQ 事件并维持模块运行。
 */
static void *mine_wk2114_task(const char *arg)
{
    unused(arg);
    osal_msleep(MINE_WK2114_TASK_INIT_DELAY_MS);

    (void)mine_wk2114_bootstrap_with_retry();

    while (1) {
        if (g_mine_wk2114_irq_pending) {
            g_mine_wk2114_irq_pending = false;
            mine_wk2114_log("[mine wk2114] irq pending total=%lu level=%u\r\n",
                (unsigned long)g_mine_wk2114_irq_count,
                (unsigned int)uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN));
        }
        osal_msleep(MINE_WK2114_TASK_LOOP_WAIT_MS);
    }

    return NULL;
}

/**
 * @brief 模块入口：创建后台线程。
 */
static void mine_wk2114_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mine_wk2114_task,
        0, "mine_wk2114", MINE_WK2114_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MINE_WK2114_TASK_PRIO);
        osal_kfree(task_handle);
        mine_wk2114_log("[mine wk2114] task created\r\n");
    } else {
        mine_wk2114_log("[mine wk2114] task create failed\r\n");
    }
    osal_kthread_unlock();
}

app_run(mine_wk2114_entry);
