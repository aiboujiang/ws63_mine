/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块主流程与协议发送实现。
 */

#include "mine_wk2114_uart2_ext_module.h"

#include <stdbool.h>
#include <stdarg.h>

#include "app_init.h"
#include "common_def.h"
#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "uart.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/* 保留原 OSAL 日志出口，并镜像到 PRINT 通道。 */
static void (*g_mine_wk2114_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/* WK2114 主口 UART 收包缓存。 */
static uint8_t g_mine_wk2114_uart_rx_buffer[MINE_WK2114_UART_RX_BUFFER_SIZE] = {0};

/* 运行态缓存：就绪标记、各子通道波特率、通道使能状态。 */
static bool g_mine_wk2114_uart_ready = false;
static uint32_t g_mine_wk2114_subuart_baud[MINE_WK2114_SUBUART_COUNT] = {
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
};
static bool g_mine_wk2114_subuart_ready[MINE_WK2114_SUBUART_COUNT] = { false, false, false, false };

/* GENA 低 4 位用于 UT1~UT4 使能，保留位按手册保持为 1。 */
static uint8_t g_mine_wk2114_gena_shadow = 0xF0;

/**
 * @brief WK2114 模块统一日志接口，双路输出到 OSAL 与 PRINT。
 *
 * @param fmt printf 风格格式串。
 */
void mine_wk2114_log(const char *fmt, ...)
{
    char log_buf[MINE_WK2114_LOG_BUFFER_LEN] = {0};
    va_list args;
    int32_t format_len;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    format_len = vsnprintf_s(log_buf, sizeof(log_buf), sizeof(log_buf) - 1, fmt, args);
    va_end(args);
    if (format_len <= 0) {
        return;
    }

    g_mine_wk2114_raw_osal_printk("%s", log_buf);
    PRINT("%s", log_buf);
}

#define osal_printk mine_wk2114_log

/**
 * @brief 判断子串口号是否在 1~4 范围内。
 *
 * @param channel 子串口号。
 * @return true  合法。
 * @return false 非法。
 */
static bool mine_wk2114_channel_valid(uint8_t channel)
{
    return ((channel >= MINE_WK2114_SUBUART_MIN) && (channel <= MINE_WK2114_SUBUART_MAX));
}

/**
 * @brief 将子串口号映射为数组索引。
 *
 * @param channel 子串口号（1~4）。
 * @return uint8_t 索引（0~3）。
 */
static uint8_t mine_wk2114_channel_to_index(uint8_t channel)
{
    return (uint8_t)(channel - MINE_WK2114_SUBUART_MIN);
}

/**
 * @brief 生成子串口 6bit 地址（C1C0 + REG[3:0]）。
 *
 * @param channel 子串口号（1~4）。
 * @param reg4    子串口寄存器低 4 位地址。
 * @return uint8_t WK2114 6bit 地址。
 */
static uint8_t mine_wk2114_make_sub_addr(uint8_t channel, uint8_t reg4)
{
    uint8_t c1c0 = mine_wk2114_channel_to_index(channel);
    return (uint8_t)(((c1c0 & 0x03U) << 4) | (reg4 & 0x0FU));
}

/**
 * @brief 发送一帧主 UART 数据并同步上报 OLED。
 *
 * @param frame  数据帧。
 * @param len    帧长。
 * @param prefix OLED 事件前缀。
 * @return errcode_t
 */
static errcode_t mine_wk2114_send_host_frame(const uint8_t *frame, uint16_t len, const char *prefix)
{
    int32_t write_ret;

    if (!g_mine_wk2114_uart_ready) {
        return ERRCODE_UART_NOT_INIT;
    }
    if ((frame == NULL) || (len == 0)) {
        return ERRCODE_INVALID_PARAM;
    }

    write_ret = uapi_uart_write(MINE_WK2114_HOST_UART_BUS, frame, len, 0);
    if (write_ret < 0) {
        mine_wk2114_oled_push_state("HOST TX FAIL");
        return ERRCODE_FAIL;
    }

    mine_wk2114_oled_push_data_event(prefix, frame, len);
    return ERRCODE_SUCC;
}

/**
 * @brief 向 WK2114 指定 6bit 地址写 1 字节数据。
 *
 * @param addr6 6bit 地址。
 * @param value 数据值。
 * @return errcode_t
 */
static errcode_t mine_wk2114_write_addr6(uint8_t addr6, uint8_t value)
{
    uint8_t frame[2] = {0};

    frame[0] = (uint8_t)(addr6 & 0x3FU);
    frame[1] = value;
    return mine_wk2114_send_host_frame(frame, sizeof(frame), "HOST TX REG");
}

/**
 * @brief 按手册公式换算波特率寄存器值。
 *
 * @param baud_rate 输入波特率。
 * @param baud_reg  输出 BAUD[15:0]。
 * @param pres      输出 PRES[3:0]。
 * @return errcode_t
 */
static errcode_t mine_wk2114_calc_baud_param(uint32_t baud_rate, uint16_t *baud_reg, uint8_t *pres)
{
    uint64_t numerator;
    uint64_t denominator;
    uint32_t reg_x10;
    uint32_t reg_integer;

    if ((baud_rate == 0U) || (baud_reg == NULL) || (pres == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    numerator = (uint64_t)MINE_WK2114_XTAL_HZ * 10ULL;
    denominator = (uint64_t)16U * baud_rate;
    if (denominator == 0ULL) {
        return ERRCODE_INVALID_PARAM;
    }

    /* 使用 x10 定点数并四舍五入，兼容手册 PRES 一位小数的设置方式。 */
    reg_x10 = (uint32_t)((numerator + (denominator / 2ULL)) / denominator);
    if (reg_x10 < 10U) {
        return ERRCODE_INVALID_PARAM;
    }

    reg_integer = reg_x10 / 10U;
    if ((reg_integer == 0U) || (reg_integer > 65536U)) {
        return ERRCODE_INVALID_PARAM;
    }

    *baud_reg = (uint16_t)(reg_integer - 1U);
    *pres = (uint8_t)(reg_x10 % 10U);
    return ERRCODE_SUCC;
}

/**
 * @brief 使能指定子串口全局时钟位（GENA.UTxEN）。
 *
 * @param channel 子串口号（1~4）。
 * @return errcode_t
 */
static errcode_t mine_wk2114_enable_global_channel(uint8_t channel)
{
    uint8_t bit_index;

    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }

    bit_index = mine_wk2114_channel_to_index(channel);
    g_mine_wk2114_gena_shadow |= (uint8_t)(1U << bit_index);
    return mine_wk2114_write_addr6(MINE_WK2114_ADDR_GENA, g_mine_wk2114_gena_shadow);
}

/**
 * @brief 配置子串口波特率并使能 RX/TX/FIFO。
 *
 * @param channel   子串口号（1~4）。
 * @param baud_rate 波特率。
 * @return errcode_t
 */
static errcode_t mine_wk2114_config_subuart(uint8_t channel, uint32_t baud_rate)
{
    errcode_t ret;
    uint8_t sub_addr;
    uint16_t baud_reg = 0;
    uint8_t pres = 0;

    ret = mine_wk2114_calc_baud_param(baud_rate, &baud_reg, &pres);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 1) 切到 PAGE1，写 BAUD1/BAUD0/PRES。 */
    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SPAGE);
    ret = mine_wk2114_write_addr6(sub_addr, 0x01);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_BAUD1);
    ret = mine_wk2114_write_addr6(sub_addr, (uint8_t)((baud_reg >> 8) & 0xFFU));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_BAUD0);
    ret = mine_wk2114_write_addr6(sub_addr, (uint8_t)(baud_reg & 0xFFU));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_PRES);
    ret = mine_wk2114_write_addr6(sub_addr, (uint8_t)(pres & 0x0FU));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 2) 切回 PAGE0，开启 RX/TX 与 FIFO。 */
    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SPAGE);
    ret = mine_wk2114_write_addr6(sub_addr, 0x00);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SCR);
    ret = mine_wk2114_write_addr6(sub_addr, 0x03);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_FCR);
    ret = mine_wk2114_write_addr6(sub_addr, 0x0F);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 3) 使能全局对应通道时钟。 */
    ret = mine_wk2114_enable_global_channel(channel);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    g_mine_wk2114_subuart_baud[mine_wk2114_channel_to_index(channel)] = baud_rate;
    g_mine_wk2114_subuart_ready[mine_wk2114_channel_to_index(channel)] = true;
    mine_wk2114_oled_set_channel(channel, baud_rate);
    mine_wk2114_oled_push_state("SUB UART READY");
    return ERRCODE_SUCC;
}

/**
 * @brief 将 FIFO 字节数转换为 WK2114 命令低 4 位。
 *
 * @param len FIFO 发送字节数（1~16）。
 * @return uint8_t 命令低 4 位编码。
 */
static uint8_t mine_wk2114_fifo_len_to_nibble(uint16_t len)
{
    if (len <= 1U) {
        return 0x00;
    }
    if (len >= 16U) {
        return 0x0F;
    }
    return (uint8_t)len;
}

/**
 * @brief 向指定子串口发送一个 FIFO 分片（最多 16 字节）。
 *
 * @param channel 子串口号。
 * @param data    数据指针。
 * @param len     分片长度（1~16）。
 * @return errcode_t
 */
static errcode_t mine_wk2114_send_fifo_chunk(uint8_t channel, const uint8_t *data, uint16_t len)
{
    uint8_t frame[MINE_WK2114_UART_FRAME_MAX] = {0};
    uint8_t cmd;

    if ((data == NULL) || (len == 0U) || (len > MINE_WK2114_FIFO_CHUNK_MAX)) {
        return ERRCODE_INVALID_PARAM;
    }

    cmd = (uint8_t)(0x80U |
        ((mine_wk2114_channel_to_index(channel) & 0x03U) << 4) |
        mine_wk2114_fifo_len_to_nibble(len));
    frame[0] = cmd;

    if (memcpy_s(&frame[1], sizeof(frame) - 1, data, len) != EOK) {
        return ERRCODE_FAIL;
    }

    return mine_wk2114_send_host_frame(frame, (uint16_t)(len + 1U), "HOST TX FIFO");
}

/**
 * @brief WK2114 主口 UART2 接收回调。
 *
 * @param buffer 接收缓冲区。
 * @param length 数据长度。
 * @param error  回调错误标志。
 */
static void mine_wk2114_uart_rx_handler(const void *buffer, uint16_t length, bool error)
{
    if (error) {
        mine_wk2114_oled_push_state("HOST RX ERR");
    }

    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    mine_wk2114_oled_push_data_event("HOST RX", (const uint8_t *)buffer, length);
}

/**
 * @brief 初始化 WK2114 主口 UART2 并注册接收回调。
 *
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_init(void)
{
    uart_attr_t attr = {
        .baud_rate = MINE_WK2114_HOST_UART_BAUD,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE,
    };
    uart_buffer_config_t uart_buffer_cfg = {0};
    uart_pin_config_t pin_cfg = {0};
    errcode_t ret;

    if (g_mine_wk2114_uart_ready) {
        return ERRCODE_SUCC;
    }

    pin_cfg.tx_pin = MINE_WK2114_HOST_UART_TX_PIN;
    pin_cfg.rx_pin = MINE_WK2114_HOST_UART_RX_PIN;
    pin_cfg.cts_pin = PIN_NONE;
    pin_cfg.rts_pin = PIN_NONE;

    uart_buffer_cfg.rx_buffer = g_mine_wk2114_uart_rx_buffer;
    uart_buffer_cfg.rx_buffer_size = sizeof(g_mine_wk2114_uart_rx_buffer);

    uapi_pin_set_mode(pin_cfg.tx_pin, MINE_WK2114_HOST_UART_PIN_MODE);
    uapi_pin_set_mode(pin_cfg.rx_pin, MINE_WK2114_HOST_UART_PIN_MODE);

    (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
    ret = uapi_uart_init(MINE_WK2114_HOST_UART_BUS, &pin_cfg, &attr, NULL, &uart_buffer_cfg);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("UART2 INIT FAIL");
        return ret;
    }

    ret = uapi_uart_register_rx_callback(MINE_WK2114_HOST_UART_BUS,
        UART_RX_CONDITION_MASK_IDLE, 1, mine_wk2114_uart_rx_handler);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("UART2 RXCB FAIL");
        return ret;
    }

    g_mine_wk2114_uart_ready = true;
    mine_wk2114_oled_push_state("UART2 HOST OK");
    mine_wk2114_oled_set_channel(0, MINE_WK2114_HOST_UART_BAUD);
    return ERRCODE_SUCC;
}

/**
 * @brief 配置并使能指定子串口。
 *
 * @param channel   子串口号（1~4）。
 * @param baud_rate 波特率。
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_set_subuart_baud(uint8_t channel, uint32_t baud_rate)
{
    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (!g_mine_wk2114_uart_ready) {
        return ERRCODE_UART_NOT_INIT;
    }

    return mine_wk2114_config_subuart(channel, baud_rate);
}

/**
 * @brief 通过 WK2114 指定子串口发送数据。
 *
 * @param channel 子串口号（1~4）。
 * @param data    发送数据。
 * @param len     数据长度。
 * @return errcode_t
 */
errcode_t mine_wk2114_uart2_ext_send(uint8_t channel, const uint8_t *data, uint16_t len)
{
    errcode_t ret;
    uint16_t offset = 0;

    if (!mine_wk2114_channel_valid(channel)) {
        return ERRCODE_INVALID_PARAM;
    }
    if ((data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (!g_mine_wk2114_uart_ready) {
        return ERRCODE_UART_NOT_INIT;
    }

    /* 首次发送前自动按缓存波特率完成子串口配置。 */
    if (!g_mine_wk2114_subuart_ready[mine_wk2114_channel_to_index(channel)]) {
        ret = mine_wk2114_config_subuart(channel,
            g_mine_wk2114_subuart_baud[mine_wk2114_channel_to_index(channel)]);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    /* WK2114 FIFO 单次最多 16 字节，分片发送。 */
    while (offset < len) {
        uint16_t chunk_len = (uint16_t)(len - offset);
        if (chunk_len > MINE_WK2114_FIFO_CHUNK_MAX) {
            chunk_len = MINE_WK2114_FIFO_CHUNK_MAX;
        }

        ret = mine_wk2114_send_fifo_chunk(channel, &data[offset], chunk_len);
        if (ret != ERRCODE_SUCC) {
            mine_wk2114_oled_push_state("SUB TX FAIL");
            return ret;
        }

        offset = (uint16_t)(offset + chunk_len);
    }

    mine_wk2114_oled_set_channel(channel,
        g_mine_wk2114_subuart_baud[mine_wk2114_channel_to_index(channel)]);
    mine_wk2114_oled_push_data_event("SUB TX", data, len);
    return ERRCODE_SUCC;
}

/**
 * @brief WK2114 模块主任务。
 *
 * @param arg 任务参数（当前未使用）。
 * @return void* 任务退出返回值。
 */
static void *mine_wk2114_uart2_ext_task(const char *arg)
{
    errcode_t ret;

    unused(arg);
    osal_msleep(MINE_WK2114_INIT_DELAY_MS);

    mine_wk2114_oled_init();
    mine_wk2114_oled_push_state("INIT...");

    ret = mine_wk2114_uart2_ext_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] uart init failed, ret=%x\\r\\n", ret);
        mine_wk2114_oled_flush_pending();
        return NULL;
    }

    /* 默认先拉起子串口1，便于上电即验证 OLED 调试链路。 */
    ret = mine_wk2114_uart2_ext_set_subuart_baud(1, g_mine_wk2114_subuart_baud[0]);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] sub1 cfg failed, ret=%x\\r\\n", ret);
        mine_wk2114_oled_push_state("SUB1 CFG FAIL");
    } else {
        mine_wk2114_oled_push_state("SUB1 READY");
    }

    while (1) {
        mine_wk2114_oled_flush_pending();
        osal_msleep(MINE_WK2114_TASK_LOOP_WAIT_MS);
    }
}

/**
 * @brief WK2114 模块应用入口，创建任务线程。
 */
static void mine_wk2114_uart2_ext_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mine_wk2114_uart2_ext_task,
        0, "mine_wk2114", MINE_WK2114_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MINE_WK2114_TASK_PRIO);
        osal_kfree(task_handle);
        osal_printk("[mine wk2114] task created\\r\\n");
    } else {
        osal_printk("[mine wk2114] task create failed\\r\\n");
    }
    osal_kthread_unlock();
}

app_run(mine_wk2114_uart2_ext_entry);
