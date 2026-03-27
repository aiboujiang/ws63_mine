/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块实现。
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

/* 如果原始OSAL打印函数存在，则覆盖它 */
static void (*g_mine_wk2114_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/* UART2 接收缓冲区 */
static uint8_t g_mine_wk2114_uart_rx_buffer[MINE_WK2114_HOST_UART_RX_BUFFER_SIZE] = {0};
/* 当前探测的 RX GPIO 逻辑引脚号 */
static pin_t g_mine_wk2114_host_rx_gpio_probe_pin = MINE_WK2114_HOST_UART_RX_GPIO_PIN;

/* 主应答 FIFO */
static uint8_t g_mine_wk2114_host_resp_fifo[MINE_WK2114_HOST_RESP_FIFO_SIZE] = {0};
static volatile uint16_t g_mine_wk2114_host_resp_head = 0;
static volatile uint16_t g_mine_wk2114_host_resp_tail = 0;

/* 模块状态 */
static bool g_mine_wk2114_ready = false;
static uint8_t g_mine_wk2114_gena_shadow = MINE_WK2114_GENA_RESERVED_MASK;
static uint32_t g_mine_wk2114_subuart_baud[MINE_WK2114_SUBUART_COUNT] = {
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
};
static bool g_mine_wk2114_subuart_ready[MINE_WK2114_SUBUART_COUNT] = { false, false, false, false };

/* IRQ 状态 */
static volatile bool g_mine_wk2114_irq_pending = false;
static volatile uint32_t g_mine_wk2114_irq_count = 0;

/* 接收回调字节数统计，接收轮询字节数统计 */
static volatile uint32_t g_mine_wk2114_rx_cb_bytes = 0;
static volatile uint32_t g_mine_wk2114_rx_poll_bytes = 0;

/* 前置声明：避免先调用后定义触发隐式声明。 */
static errcode_t mine_wk2114_send_autobaud_sync_sequence(void);
static errcode_t mine_wk2114_verify_register_value(uint8_t addr6, uint8_t expected, const char *reg_name);
static __attribute__((unused)) errcode_t mine_wk2114_send_autobaud_sync_fallback_burst(void);

/**
 * @brief 禁用中断
 */
static unsigned int mine_wk2114_irq_lock(void)
{
    return osal_irq_lock();
}

/**
 * @brief 使能中断
 */
static void mine_wk2114_irq_unlock(unsigned int irq_state)
{
    osal_irq_restore(irq_state);
}

/**
 * @brief 检查通道是否有效
 */
static bool mine_wk2114_channel_valid(uint8_t channel)
{
    return ((channel >= MINE_WK2114_SUBUART_MIN) && (channel <= MINE_WK2114_SUBUART_MAX));
}

/**
 * @brief 通道号转索引 1~4 -> 0~3
 */
static uint8_t mine_wk2114_channel_index(uint8_t channel)
{
    return (uint8_t)(channel - MINE_WK2114_SUBUART_MIN);
}

/**
 * @brief 构造子通道地址 6bit 地址 C1C0 + A3A2A1A0
 */
static uint8_t mine_wk2114_make_sub_addr(uint8_t channel, uint8_t reg4)
{
    uint8_t c1c0 = mine_wk2114_channel_index(channel);
    return (uint8_t)(((c1c0 & 0x03U) << 4) | (reg4 & 0x0FU));
}

/**
 * @brief 返回指定通道在全局位图中的位
 */
static uint8_t mine_wk2114_channel_bit(uint8_t channel)
{
    return (uint8_t)(1U << mine_wk2114_channel_index(channel));
}

/**
 * @brief 统计一条日志，输出到 OSAL + PRINT + UART0
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
 * @brief 记录关键引脚状态，显示当前状态
 */
static void mine_wk2114_log_pin_snapshot(const char *stage)
{
    mine_wk2114_log("[mine wk2114] %s pin irq=%u rst=%u rx=%u\r\n",
        (stage == NULL) ? "stage" : stage,
        (unsigned int)uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN),
        (unsigned int)uapi_gpio_get_val(MINE_WK2114_RESET_GPIO_PIN),
    (unsigned int)uapi_gpio_get_val(g_mine_wk2114_host_rx_gpio_probe_pin));
}

/**
 * @brief 通过 GPIO 配置方式探测 RX 原始状态
 *
 * 关键流程注释：先将UART初始化前执行，确保UART收发/中断/外部中断都未使能
 */
static void mine_wk2114_probe_host_rx_gpio_level(const char *stage)
{
    uint8_t i;
    uint8_t low_cnt = 0;
    uint8_t high_cnt = 0;

    uapi_gpio_init();
    (void)uapi_pin_set_mode(g_mine_wk2114_host_rx_gpio_probe_pin, HAL_PIO_FUNC_GPIO);
    (void)uapi_pin_set_pull(g_mine_wk2114_host_rx_gpio_probe_pin, PIN_PULL_TYPE_UP);
    (void)uapi_gpio_set_dir(g_mine_wk2114_host_rx_gpio_probe_pin, GPIO_DIRECTION_INPUT);

    for (i = 0; i < 8U; i++) {
        if (uapi_gpio_get_val(g_mine_wk2114_host_rx_gpio_probe_pin) == GPIO_LEVEL_LOW) {
            low_cnt++;
        } else {
            high_cnt++;
        }
        osal_msleep(2);
    }

    mine_wk2114_log("[mine wk2114] %s rx-gpio-probe low=%u high=%u\r\n",
        (stage == NULL) ? "stage" : stage,
        (unsigned int)low_cnt,
        (unsigned int)high_cnt);
}

/**
 * @brief IRQ 中断处理函数，仅记录中断次数和中断状态
 */
static void mine_wk2114_irq_handler(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);

    g_mine_wk2114_irq_count++;
    g_mine_wk2114_irq_pending = true;
}

/**
 * @brief 初始化 IRQ 中断，GPIO13 引脚使能中断
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
 * @brief 执行硬件复位
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

    /* 关键流程注释：先将RST拉高10ms */
    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pre-high failed, ret=%x\r\n", ret);
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_HOLD_MS);  // 拉高10ms

    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pull-low failed, ret=%x\r\n", ret);
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_LOW_HOLD_MS);  // 拉低10ms复位

    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset release-high failed, ret=%x\r\n", ret);
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_RELEASE_WAIT_MS);  // 拉高20ms完成复位

    mine_wk2114_log("[mine wk2114] hw reset pulse done on gpio10\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 初始化WK2114芯片，包含完整的自适应波特率流程
 * 
 * 流程：拉高RST10ms，拉低RST10ms复位，再拉高20ms完成复位，
 * 复位完成后发送0x55完成波特率匹配
 */
errcode_t mine_wk2114_init_chip(void)
{
    errcode_t ret;
    
    // 执行硬件复位
    ret = mine_wk2114_hw_reset_chip();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] hardware reset failed, ret=%x\r\n", ret);
        return ret;
    }
    
    // 复位完成后发送0x55完成波特率匹配
    ret = mine_wk2114_send_autobaud_sync_sequence();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] autobaud sync failed, ret=%x\r\n", ret);
        return ret;
    }
    
    // 验证连接是否成功
    ret = mine_wk2114_verify_register_value(MINE_WK2114_ADDR_GENA, MINE_WK2114_GENA_RESERVED_MASK, "GENA");
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] verify GENA failed after init, ret=%x\r\n", ret);
        return ret;
    }
    
    mine_wk2114_log("[mine wk2114] chip initialized successfully\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 重置主应答 FIFO
 */
static void mine_wk2114_host_resp_fifo_reset(void)
{
    unsigned int irq_state = mine_wk2114_irq_lock();
    g_mine_wk2114_host_resp_head = 0;
    g_mine_wk2114_host_resp_tail = 0;
    mine_wk2114_irq_unlock(irq_state);
}

/**
 * @brief 清空主 UART RX FIFO 中的残留字节
 *
 * 关键流程注释：如果一次读取失败，则认为没有残留字节，否则继续读取
 * 读取到的字节不会被使用，仅用于清空 FIFO
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
 * @brief 环回 UART2 通道并测试 TX/RX 引脚配置是否正确
 *
 * 关键流程注释：首先确保 UART2 接收缓冲区为空，然后发送测试数据
 * 通过 UART2 通道接收数据，如果接收数据与发送数据一致，则认为配置正确
 */
static __attribute__((unused)) errcode_t mine_wk2114_uart2_loopback_self_test(void)
{
    const uint8_t tx[MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN] = {0x55U, 0xA5U, 0x5AU, 0x0FU};
    uint8_t rx[MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN] = {0};
    uint8_t rx_count = 0;
    uint32_t start_ms;
    int32_t wlen;
    int32_t rlen;

    mine_wk2114_host_uart_rx_drain();
    start_ms = (uint32_t)uapi_systick_get_ms();

    wlen = uapi_uart_write(MINE_WK2114_HOST_UART_BUS, tx, MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN, 0);
    if (wlen != (int32_t)MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN) {
        mine_wk2114_log("[mine wk2114] uart2 loopback tx failed wlen=%ld\r\n", (long)wlen);
        return ERRCODE_FAIL;
    }

    while ((uint32_t)(uapi_systick_get_ms() - start_ms) < MINE_WK2114_UART2_LOOPBACK_TIMEOUT_MS) {
        rlen = uapi_uart_read(MINE_WK2114_HOST_UART_BUS,
            &rx[rx_count],
            (uint16_t)(MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN - rx_count),
            0);
        if (rlen > 0) {
            rx_count = (uint8_t)(rx_count + (uint8_t)rlen);
            if (rx_count >= MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN) {
                break;
            }
            continue;
        }
        osal_msleep(1);
    }

    if (rx_count != MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN) {
        mine_wk2114_log("[mine wk2114] uart2 loopback timeout rx_count=%u\r\n", (unsigned int)rx_count);
        return ERRCODE_FAIL;
    }

    if (memcmp(tx, rx, MINE_WK2114_UART2_LOOPBACK_PAYLOAD_LEN) != 0) {
        mine_wk2114_log("[mine wk2114] uart2 loopback mismatch tx=%02X %02X %02X %02X rx=%02X %02X %02X %02X\r\n",
            (unsigned int)tx[0],
            (unsigned int)tx[1],
            (unsigned int)tx[2],
            (unsigned int)tx[3],
            (unsigned int)rx[0],
            (unsigned int)rx[1],
            (unsigned int)rx[2],
            (unsigned int)rx[3]);
        return ERRCODE_FAIL;
    }

    mine_wk2114_log("[mine wk2114] uart2 loopback pass\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 主应答 FIFO 写 1 字节，如果满则丢弃最旧的数据
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
 * @brief 主应答 FIFO 读 1 字节
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
 * @brief 处理 RX 中断接收的数据，并存入应答 FIFO
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

    g_mine_wk2114_rx_cb_bytes += (uint32_t)length;
}

/**
 * @brief 发送主帧
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
 * @brief 向 6bit 地址写 1 字节的寄存器
 */
static errcode_t mine_wk2114_write_addr6(uint8_t addr6, uint8_t value)
{
    uint8_t frame[2] = {0};

    frame[0] = (uint8_t)(MINE_WK2114_HOST_CMD_WRITE_REG | (addr6 & 0x3FU));
    frame[1] = value;
    return mine_wk2114_send_host_frame(frame, sizeof(frame));
}

/**
 * @brief 执行一次读寄存器操作
 *
 * @param addr6 6bit 的寄存器地址
 * @param with_dummy true=发送 cmd+dummy 两个字节，false=发送一个 cmd 字节
 * @param value 读取到的寄存器值
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

        /* 关键流程注释：如果收到数据并且不再有新数据，则等待稳定 */
        if (got_data && (!got_new_data)) {
            stable_wait_ms++;
            if (stable_wait_ms >= MINE_WK2114_HOST_RESP_STABLE_WAIT_MS) {
                *value = last;
                return ERRCODE_SUCC;
            }
        }

        /* 中断无数据时轮询 RX FIFO */
        if (uapi_uart_read(MINE_WK2114_HOST_UART_BUS, &rx, 1, 0) > 0) {
            last = rx;
            got_data = true;
            stable_wait_ms = 0;
            g_mine_wk2114_rx_poll_bytes++;
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
 * @brief 读取 6bit 地址的寄存器值，采用两种方式读取
 */
static errcode_t mine_wk2114_read_addr6(uint8_t addr6, uint8_t *value)
{
    errcode_t ret;
    uint32_t rx_cb_before;
    uint32_t rx_poll_before;

    if (value == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    rx_cb_before = g_mine_wk2114_rx_cb_bytes;
    rx_poll_before = g_mine_wk2114_rx_poll_bytes;

    /* 关键流程注释：首先尝试仅发送 cmd，如果失败则自动回退到 cmd+dummy */
    ret = mine_wk2114_read_addr6_once(addr6, false, value);
    if (ret == ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] read addr6=0x%02X by cmd-only\r\n", (unsigned int)addr6);
        return ERRCODE_SUCC;
    }

    /*
     * 关键流程注释：如果上述失败，则默认发送 1 字节 CMD
     * cmd+dummy 作为备选方案，如果默认发送失败则关闭自动回退
     */
#if (MINE_WK2114_HOST_READ_DUMMY_FALLBACK_ENABLE == 1U)
    ret = mine_wk2114_read_addr6_once(addr6, true, value);
    if (ret == ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] read addr6=0x%02X by cmd+dummy\r\n", (unsigned int)addr6);
        return ERRCODE_SUCC;
    }
#endif

    mine_wk2114_log("[mine wk2114] read addr6=0x%02X timeout cb_delta=%lu poll_delta=%lu rx_level=%u\r\n",
        (unsigned int)addr6,
        (unsigned long)(g_mine_wk2114_rx_cb_bytes - rx_cb_before),
        (unsigned long)(g_mine_wk2114_rx_poll_bytes - rx_poll_before),
        (unsigned int)uapi_gpio_get_val(g_mine_wk2114_host_rx_gpio_probe_pin));
    mine_wk2114_log_pin_snapshot("read-timeout");
    return ERRCODE_FAIL;
}

/**
 * @brief 读取寄存器值，重试
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
 * @brief 验证寄存器值
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
 * @brief 写寄存器并读回验证
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
 * @brief 执行 0x55 重复发送以匹配波特率
 */
static errcode_t mine_wk2114_send_autobaud_sync_sequence(void)
{
    uint8_t syncs[MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY];
    uint8_t i;
    errcode_t ret;

    for (i = 0; i < MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY; i++) {
        syncs[i] = MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE;
    }

    ret = mine_wk2114_send_host_frame(syncs, MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    mine_wk2114_log("[mine wk2114] sync55 done count=%u irq_level=%u\r\n",
        (unsigned int)MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY,
        (unsigned int)uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN));

    /* 关键流程注释：等待WK2114内部完成自适应 */
    osal_msleep(MINE_WK2114_HOST_AUTOBAUD_LOCK_WAIT_MS);
    return ERRCODE_SUCC;
}

/**
 * @brief 自适应失败时，发送大量 0x55 以匹配波特率
 */
static __attribute__((unused)) errcode_t mine_wk2114_send_autobaud_sync_fallback_burst(void)
{
    uint8_t syncs[MINE_WK2114_HOST_AUTOBAUD_FALLBACK_BURST_COUNT];
    uint8_t i;
    errcode_t ret;

    for (i = 0; i < MINE_WK2114_HOST_AUTOBAUD_FALLBACK_BURST_COUNT; i++) {
        syncs[i] = MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE; // fallback to sync byte inside for loop
    }

    ret = mine_wk2114_send_host_frame(syncs, MINE_WK2114_HOST_AUTOBAUD_FALLBACK_BURST_COUNT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    mine_wk2114_log("[mine wk2114] sync55 fallback burst count=%u\r\n",
        (unsigned int)MINE_WK2114_HOST_AUTOBAUD_FALLBACK_BURST_COUNT);
    osal_msleep(MINE_WK2114_HOST_AUTOBAUD_FALLBACK_WAIT_MS);
    return ERRCODE_SUCC;
}

/**
 * @brief 读取子串口发送 FIFO 计数（TFCNT）
 *
 * @param channel 子串口号（1~4）
 * @param tfcnt 输出计数值
 * @return errcode_t
 */
static errcode_t mine_wk2114_read_sub_tfcnt(uint8_t channel, uint8_t *tfcnt)
{
    uint8_t addr;

    if ((!mine_wk2114_channel_valid(channel)) || (tfcnt == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_TFCNT);
    return mine_wk2114_read_addr6_retry(addr, MINE_WK2114_LINK_CHECK_READ_RETRY, tfcnt);
}

/**
 * @brief 读取子串口 FIFO 状态寄存器（FSR）
 *
 * @param channel 子串口号（1~4）
 * @param fsr 输出状态值
 * @return errcode_t
 */
static errcode_t mine_wk2114_read_sub_fsr(uint8_t channel, uint8_t *fsr)
{
    uint8_t addr;

    if ((!mine_wk2114_channel_valid(channel)) || (fsr == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_FSR);
    return mine_wk2114_read_addr6_retry(addr, MINE_WK2114_LINK_CHECK_READ_RETRY, fsr);
}

/**
 * @brief 流程：复位 -> 0x55 -> 读GENA -> 写GENA
 */
static errcode_t mine_wk2114_check_link_ready(void)
{
    uint8_t gena_test;
    errcode_t ret;

    // 使用新的初始化函数，它包含了复位和自适应波特率的完整流程
    ret = mine_wk2114_init_chip();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] chip initialization failed\r\n");
        return ret;
    }

    /* D: 写回验证，先写 UT1EN 然后恢复默认值。 */
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
 * @brief 计算初始寄存器参数（初始模式）
 */
static errcode_t mine_wk2114_calc_baud_param(uint32_t baud_rate, uint16_t *baud_reg, uint8_t *pres)
{
    uint64_t reg_x16;
    uint64_t integer_part;
    uint64_t fraction_x16;

    if ((baud_rate == 0U) || (baud_reg == NULL) || (pres == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    /* 应用笔记 5.2 公式：XTAL/(16*baud)=整数+小数，BAUD=整数-1，PRES=小数*16（取整） */
    reg_x16 = ((uint64_t)MINE_WK2114_XTAL_HZ * 16ULL + (uint64_t)(8U * baud_rate)) / (uint64_t)(16U * baud_rate);
    if (reg_x16 < 16ULL) {
        return ERRCODE_INVALID_PARAM;
    }

    integer_part = reg_x16 / 16ULL;
    fraction_x16 = reg_x16 % 16ULL;
    if (integer_part == 0ULL) {
        return ERRCODE_INVALID_PARAM;
    }

    *baud_reg = (uint16_t)(integer_part - 1ULL);
    *pres = (uint8_t)(fraction_x16 & 0x0FU);
    return ERRCODE_SUCC;
}

/**
 * @brief 使能指定通道的全局通道位
 */
static errcode_t mine_wk2114_enable_global_channel(uint8_t channel)
{
    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }

    g_mine_wk2114_gena_shadow = (uint8_t)((g_mine_wk2114_gena_shadow | MINE_WK2114_GENA_RESERVED_MASK) |
        mine_wk2114_channel_bit(channel));

    return mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GENA, g_mine_wk2114_gena_shadow, "GENA");
}

/**
 * @brief 重置指定通道的全局通道位
 *
 * 关键流程注释：GRST 对应位写 1 会自动复位通道，
 * 仅写全局通道位后等待复位位即可
 */
static errcode_t mine_wk2114_soft_reset_channel(uint8_t channel)
{
    errcode_t ret;

    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = mine_wk2114_write_addr6(MINE_WK2114_ADDR_GRST, mine_wk2114_channel_bit(channel));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    osal_msleep(2);
    return ERRCODE_SUCC;
}

/**
 * @brief 使能指定通道的全局中断位
 */
static errcode_t mine_wk2114_enable_global_channel_irq(uint8_t channel)
{
    uint8_t gier = 0;
    errcode_t ret;

    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = mine_wk2114_read_addr6_retry(MINE_WK2114_ADDR_GIER, MINE_WK2114_LINK_CHECK_READ_RETRY, &gier);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] read GIER timeout\r\n");
        return ret;
    }

    gier = (uint8_t)(gier | mine_wk2114_channel_bit(channel));
    return mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GIER, gier, "GIER");
}

/**
 * @brief 配置子通道的波特率
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

    /* 关键流程注释：首先使能全局通道位 */
    ret = mine_wk2114_enable_global_channel(channel);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 关键流程注释：然后执行一次通道复位 */
    ret = mine_wk2114_soft_reset_channel(channel);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 关键流程注释：然后使能全局中断位 */
    ret = mine_wk2114_enable_global_channel_irq(channel);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* E: 最后配置 termios 的 PAGE1 currentItem */
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

    /* 最后配置 PAGE1 的 FIFO 阈值 */
    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_RFTL);
    ret = mine_wk2114_write_readback_verify(addr, MINE_WK2114_RFTL_INIT_LEVEL, "RFTL");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_TFTL);
    ret = mine_wk2114_write_readback_verify(addr, MINE_WK2114_TFTL_INIT_LEVEL, "TFTL");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 切换回 PAGE0 配置 FIFO */
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

    /* 最后配置 RFTRIG/RXOUT 中断，关闭 TX 中断 */
    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SIER);
    ret = mine_wk2114_write_readback_verify(addr, MINE_WK2114_SIER_INIT_MASK, "SIER");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_FCR);
    ret = mine_wk2114_write_addr6(addr, MINE_WK2114_FCR_INIT_ASSERT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_write_addr6(addr, MINE_WK2114_FCR_INIT_RELEASE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* FCR 初始化完成后，读取一次以确保 FIFO 使能有效 */
    ret = mine_wk2114_verify_register_value(addr, MINE_WK2114_FCR_INIT_RELEASE, "FCR");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_mine_wk2114_subuart_baud[mine_wk2114_channel_index(channel)] = baud_rate;
    g_mine_wk2114_subuart_ready[mine_wk2114_channel_index(channel)] = true;
    mine_wk2114_log("[mine wk2114] sub%u configured, baud=%lu\r\n", (unsigned int)channel, (unsigned long)baud_rate);
    return ERRCODE_SUCC;
}

/**
 * @brief FIFO 长度转 nibble
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
 * @brief 发送 FIFO 数据块，最多 16 字节
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
 * @brief 初始化 UART2 通道接收
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
    errcode_t ret;
    uint8_t tx_pin_candidates[MINE_WK2114_HOST_UART_PROFILE_COUNT] = {
        (uint8_t)MINE_WK2114_HOST_UART_TX_PIN,
        (uint8_t)MINE_WK2114_HOST_UART_TX_PIN,
        (uint8_t)MINE_WK2114_HOST_UART_SWAP_TX_PIN,
        (uint8_t)MINE_WK2114_HOST_UART_SWAP_TX_PIN,
    };
    uint8_t rx_pin_candidates[MINE_WK2114_HOST_UART_PROFILE_COUNT] = {
        (uint8_t)MINE_WK2114_HOST_UART_RX_PIN,
        (uint8_t)MINE_WK2114_HOST_UART_RX_PIN,
        (uint8_t)MINE_WK2114_HOST_UART_SWAP_RX_PIN,
        (uint8_t)MINE_WK2114_HOST_UART_SWAP_RX_PIN,
    };
    pin_t rx_gpio_probe_candidates[MINE_WK2114_HOST_UART_PROFILE_COUNT] = {
        MINE_WK2114_HOST_UART_RX_GPIO_PIN,
        MINE_WK2114_HOST_UART_RX_GPIO_PIN,
        MINE_WK2114_HOST_UART_SWAP_RX_GPIO_PIN,
        MINE_WK2114_HOST_UART_SWAP_RX_GPIO_PIN,
    };
    uint8_t mode_candidates[MINE_WK2114_HOST_UART_PROFILE_COUNT] = {
        (uint8_t)MINE_WK2114_HOST_UART_PIN_MODE,
        (uint8_t)MINE_WK2114_HOST_UART_ALT_PIN_MODE,
        (uint8_t)MINE_WK2114_HOST_UART_PIN_MODE,
        (uint8_t)MINE_WK2114_HOST_UART_ALT_PIN_MODE,
    };
    uint8_t profile_try_count = 1;
    uint8_t profile_idx;
    uint8_t mode;

    if (g_mine_wk2114_ready) {
        return ERRCODE_SUCC;
    }

    buf_cfg.rx_buffer = g_mine_wk2114_uart_rx_buffer;
    buf_cfg.rx_buffer_size = sizeof(g_mine_wk2114_uart_rx_buffer);

    /* 如果配置为 slave 模式，则默认使用 UART2 通道配置 */
    pin_cfg.tx_pin = MINE_WK2114_HOST_UART_TX_PIN;
    pin_cfg.rx_pin = MINE_WK2114_HOST_UART_RX_PIN;
    pin_cfg.cts_pin = PIN_NONE;
    pin_cfg.rts_pin = PIN_NONE;

    ret = mine_wk2114_irq_gpio_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 关键流程注释：初始化 UART2 前探测原始 RX GPIO 逻辑电平，以便识别通道是否正确 */
    mine_wk2114_probe_host_rx_gpio_level("before-uart-init");

    if ((mode_candidates[1] != mode_candidates[0]) ||
        (tx_pin_candidates[2] != tx_pin_candidates[0]) ||
        (rx_pin_candidates[2] != rx_pin_candidates[0])) {
        profile_try_count = MINE_WK2114_HOST_UART_PROFILE_COUNT;
    }

    /*
     * 关键流程注释：首先尝试使用默认配置模式，如果失败则自动回退到其他模式
     * 如果上述失败，则关闭自动回退，尝试下一个配置
     */
    for (profile_idx = 0; profile_idx < profile_try_count; profile_idx++) {
        pin_cfg.tx_pin = tx_pin_candidates[profile_idx];
        pin_cfg.rx_pin = rx_pin_candidates[profile_idx];
        g_mine_wk2114_host_rx_gpio_probe_pin = rx_gpio_probe_candidates[profile_idx];
        mode = mode_candidates[profile_idx];
        mine_wk2114_log("[mine wk2114] uart2 cfg tx=%u rx=%u mode=%u irq=%u try=%u/%u\r\n",
            (unsigned int)pin_cfg.tx_pin,
            (unsigned int)pin_cfg.rx_pin,
            (unsigned int)mode,
            (unsigned int)uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN),
            (unsigned int)(profile_idx + 1U),
            (unsigned int)profile_try_count);

        /* 关键流程注释：首先确保 RX 引脚配置为上拉，避免错误的低电平 */
        (void)uapi_pin_set_pull(pin_cfg.rx_pin, PIN_PULL_TYPE_UP);
        uapi_pin_set_mode(pin_cfg.tx_pin, mode);
        uapi_pin_set_mode(pin_cfg.rx_pin, mode);
        mine_wk2114_log_pin_snapshot("uart-pins-configured");

        (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
        ret = uapi_uart_init(MINE_WK2114_HOST_UART_BUS, &pin_cfg, &attr, NULL, &buf_cfg);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] uart init failed mode=%u ret=%x\r\n",
                (unsigned int)mode,
                ret);
            continue;
        }

#if (MINE_WK2114_UART2_LOOPBACK_SELFTEST_ENABLE == 1U)
        ret = mine_wk2114_uart2_loopback_self_test();
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] uart2 loopback failed mode=%u tx=%u rx=%u\r\n",
                (unsigned int)mode,
                (unsigned int)pin_cfg.tx_pin,
                (unsigned int)pin_cfg.rx_pin);
            continue;
        }
#endif

#if defined(CONFIG_UART_SUPPORT_RX)
        /* 如果 slave 接收回调支持，则使用 IDLE */
        ret = uapi_uart_register_rx_callback(MINE_WK2114_HOST_UART_BUS,
            UART_RX_CONDITION_MASK_IDLE,
            1,
            mine_wk2114_uart_rx_handler);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] rx callback register failed mode=%u ret=%x\r\n",
                (unsigned int)mode,
                ret);
            continue;
        }
#else
        mine_wk2114_log("[mine wk2114] uart rx callback unsupported\r\n");
        return ERRCODE_FAIL;
#endif

        g_mine_wk2114_ready = true;
        ret = mine_wk2114_check_link_ready();
        if (ret == ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] uart2 selected mode=%u\r\n", (unsigned int)mode);
            return ERRCODE_SUCC;
        }

        g_mine_wk2114_ready = false;
        mine_wk2114_log("[mine wk2114] mode=%u link check failed ret=%x\r\n",
            (unsigned int)mode,
            ret);
    }

    return ret;
}

/**
 * @brief 设置子通道波特率
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
 * @brief 子通道发送
 */
errcode_t mine_wk2114_uart2_ext_send(uint8_t channel, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0;
    uint16_t chunk;
    uint16_t send_window;
    uint16_t remain;
    uint8_t tfcnt;
    uint8_t fsr;
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

    /* 如果子通道未配置，则重新配置 */
    if (!g_mine_wk2114_subuart_ready[mine_wk2114_channel_index(channel)]) {
        ret = mine_wk2114_config_subuart(channel, g_mine_wk2114_subuart_baud[mine_wk2114_channel_index(channel)]);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    /* 应用笔记 5.2 发送流程：先读 FSR/TFCNT 计算 FIFO 余量，再按 <=16 字节分包写 FIFO */
    while (offset < len) {
        ret = mine_wk2114_read_sub_fsr(channel, &fsr);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] sub%u read FSR failed\r\n", (unsigned int)channel);
            return ret;
        }

        if ((fsr & MINE_WK2114_FSR_TFULL_BIT) != 0U) {
            osal_msleep(1);
            continue;
        }

        ret = mine_wk2114_read_sub_tfcnt(channel, &tfcnt);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_log("[mine wk2114] sub%u read TFCNT failed\r\n", (unsigned int)channel);
            return ret;
        }

        if (tfcnt >= MINE_WK2114_SUBUART_FIFO_DEPTH) {
            osal_msleep(1);
            continue;
        }

        send_window = (uint16_t)(MINE_WK2114_SUBUART_FIFO_DEPTH - tfcnt);
        remain = (uint16_t)(len - offset);
        chunk = (send_window < remain) ? send_window : remain;
        if (chunk > MINE_WK2114_FIFO_CHUNK_MAX) {
            chunk = MINE_WK2114_FIFO_CHUNK_MAX;
        }
        if (chunk == 0U) {
            osal_msleep(1);
            continue;
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
 * @brief 自动引导，如果失败则重试，直到成功
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
 * @brief 后台任务，处理 IRQ 中断并打印状态
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
 * @brief 模块启动时，创建后台任务
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
