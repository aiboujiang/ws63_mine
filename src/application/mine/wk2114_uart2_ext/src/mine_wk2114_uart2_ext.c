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
#include "systick.h"
#include "uart.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef PRINT
#define PRINT(fmt, arg...)
#endif

/* 保留原 OSAL 日志出口，并镜像到 PRINT 通道。 */
static void (*g_mine_wk2114_raw_osal_printk)(const char *fmt, ...) = osal_printk;

/**
 * @brief 将日志同步镜像到 UART0，保证串口调试口持续可见。
 *
 * 采用“尽力发送”策略：不额外打印失败日志，避免日志回路递归。
 *
 * @param log_buf    日志缓冲区。
 * @param format_len 已格式化日志长度。
 */
static void mine_wk2114_log_mirror_uart0(const char *log_buf, int32_t format_len)
{
    if ((log_buf == NULL) || (format_len <= 0)) {
        return;
    }

    /* 保持 UART0 与系统日志同步输出，不因串口未就绪中断主流程。 */
    (void)uapi_uart_write(UART_BUS_0, (const uint8_t *)log_buf, (uint16_t)format_len, 0);
}

/* WK2114 主口 UART 收包缓存。 */
static uint8_t g_mine_wk2114_uart_rx_buffer[MINE_WK2114_UART_RX_BUFFER_SIZE] = {0};

/*
 * 主口回读 FIFO：
 * 用于“读寄存器命令”的同步等待，不替代原有 OLED 事件展示。
 */
static uint8_t g_mine_wk2114_host_resp_fifo[MINE_WK2114_HOST_RESP_FIFO_SIZE] = {0};
static volatile uint16_t g_mine_wk2114_host_resp_head = 0;
static volatile uint16_t g_mine_wk2114_host_resp_tail = 0;

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
static uint8_t g_mine_wk2114_gena_shadow = MINE_WK2114_GENA_RESERVED_MASK;

/**
 * @brief 进入短临界区，保护主口回读 FIFO。
 *
 * @return unsigned int 中断状态快照。
 */
static unsigned int mine_wk2114_irq_lock(void)
{
    return osal_irq_lock();
}

/**
 * @brief 退出短临界区，恢复中断状态。
 *
 * @param irq_status 进入临界区前保存的中断状态。
 */
static void mine_wk2114_irq_unlock(unsigned int irq_status)
{
    osal_irq_restore(irq_status);
}

/**
 * @brief 清空主口回读 FIFO。
 */
static void mine_wk2114_host_resp_fifo_reset(void)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_host_resp_head = 0;
    g_mine_wk2114_host_resp_tail = 0;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 向主口回读 FIFO 写入一个字节（满时丢弃最旧数据）。
 *
 * @param byte 输入字节。
 */
static void mine_wk2114_host_resp_fifo_push(uint8_t byte)
{
    uint16_t next_head;
    unsigned int irq_status = mine_wk2114_irq_lock();

    next_head = (uint16_t)((g_mine_wk2114_host_resp_head + 1U) % MINE_WK2114_HOST_RESP_FIFO_SIZE);
    if (next_head == g_mine_wk2114_host_resp_tail) {
        g_mine_wk2114_host_resp_tail =
            (uint16_t)((g_mine_wk2114_host_resp_tail + 1U) % MINE_WK2114_HOST_RESP_FIFO_SIZE);
    }

    g_mine_wk2114_host_resp_fifo[g_mine_wk2114_host_resp_head] = byte;
    g_mine_wk2114_host_resp_head = next_head;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 从主口回读 FIFO 读取一个字节。
 *
 * @param byte 输出字节。
 * @return true  读取成功。
 * @return false FIFO 为空或参数非法。
 */
static bool mine_wk2114_host_resp_fifo_pop(uint8_t *byte)
{
    unsigned int irq_status;

    if (byte == NULL) {
        return false;
    }

    irq_status = mine_wk2114_irq_lock();
    if (g_mine_wk2114_host_resp_head == g_mine_wk2114_host_resp_tail) {
        mine_wk2114_irq_unlock(irq_status);
        return false;
    }

    *byte = g_mine_wk2114_host_resp_fifo[g_mine_wk2114_host_resp_tail];
    g_mine_wk2114_host_resp_tail =
        (uint16_t)((g_mine_wk2114_host_resp_tail + 1U) % MINE_WK2114_HOST_RESP_FIFO_SIZE);
    mine_wk2114_irq_unlock(irq_status);
    return true;
}

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
    mine_wk2114_log_mirror_uart0(log_buf, format_len);
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

    frame[0] = (uint8_t)(MINE_WK2114_HOST_CMD_WRITE_REG | (addr6 & 0x3FU));
    frame[1] = value;
    return mine_wk2114_send_host_frame(frame, sizeof(frame), "HOST TX REG");
}

/**
 * @brief 从 WK2114 指定 6bit 地址读取 1 字节寄存器。
 *
 * @param addr6 6bit 地址。
 * @param value 读出值。
 * @return errcode_t
 * @retval ERRCODE_SUCC         读取成功。
 * @retval ERRCODE_INVALID_PARAM 参数非法。
 * @retval ERRCODE_FAIL         超时或主口异常。
 */
static errcode_t mine_wk2114_read_addr6(uint8_t addr6, uint8_t *value)
{
    uint8_t cmd;
    errcode_t ret;
    uint32_t start_ms;

    if (value == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    cmd = (uint8_t)(MINE_WK2114_HOST_CMD_READ_REG | (addr6 & 0x3FU));
    mine_wk2114_host_resp_fifo_reset();

    ret = mine_wk2114_send_host_frame(&cmd, 1, "HOST TX RREG");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    start_ms = (uint32_t)uapi_systick_get_ms();
    while ((uint32_t)(uapi_systick_get_ms() - start_ms) <= MINE_WK2114_HOST_READ_TIMEOUT_MS) {
        if (mine_wk2114_host_resp_fifo_pop(value)) {
            return ERRCODE_SUCC;
        }
        osal_msleep(1);
    }

    mine_wk2114_oled_push_state("HOST RREG TO");
    return ERRCODE_FAIL;
}

/**
 * @brief 执行 WK2114 上电连通性检查。
 *
 * 检查流程基于规格书：
 * 1) 先发 0x55 完成主口波特率自适应锁定；
 * 2) 回读 GENA（0x00）确认器件有响应；
 * 3) 校验 GENA 高 4 位保留位应为 1（手册 7.2.1 复位定义）。
 *
 * @return errcode_t
 */
static errcode_t mine_wk2114_check_link_ready(void)
{
    uint8_t sync_byte = MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE;
    uint8_t gena = 0;
    errcode_t ret;

    mine_wk2114_host_resp_fifo_reset();

    ret = mine_wk2114_send_host_frame(&sync_byte, 1, "HOST TX SYNC");
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK SYNC FAIL");
        return ret;
    }

    /* 给主口自适应逻辑一个短暂锁定时间，避免首条读命令误判。 */
    osal_msleep(MINE_WK2114_HOST_AUTOBAUD_LOCK_WAIT_MS);

    ret = mine_wk2114_read_addr6(MINE_WK2114_ADDR_GENA, &gena);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK LINK FAIL");
        return ret;
    }

    if ((gena & MINE_WK2114_GENA_RESERVED_MASK) != MINE_WK2114_GENA_RESERVED_MASK) {
        osal_printk("[mine wk2114] link check failed, GENA=0x%02X\r\n", gena);
        mine_wk2114_oled_push_state("WK REG FAIL");
        return ERRCODE_FAIL;
    }

    /* 同步影子寄存器，保持后续 enable 操作与当前芯片状态一致。 */
    g_mine_wk2114_gena_shadow = (uint8_t)(gena & (MINE_WK2114_GENA_RESERVED_MASK | 0x0FU));
    osal_printk("[mine wk2114] link ok, GENA=0x%02X\r\n", gena);
    return ERRCODE_SUCC;
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

    cmd = (uint8_t)(MINE_WK2114_HOST_CMD_WRITE_FIFO |
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
    const uint8_t *data_ptr = (const uint8_t *)buffer;
    uint16_t idx;

    if (error) {
        mine_wk2114_oled_push_state("HOST RX ERR");
    }

    if ((data_ptr == NULL) || (length == 0U)) {
        return;
    }

    /*
     * 回读数据统一进入 FIFO，供读寄存器流程同步消费。
     * 即使当前没有等待者，也会保留最近一段响应，便于后续排查。
     */
    for (idx = 0; idx < length; idx++) {
        mine_wk2114_host_resp_fifo_push(data_ptr[idx]);
    }

    mine_wk2114_oled_push_data_event("HOST RX", data_ptr, length);
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

    /* 先允许主口收发，再执行规格书要求的链路就绪检查。 */
    g_mine_wk2114_uart_ready = true;

    ret = mine_wk2114_check_link_ready();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK2114 LINK FAIL");
        g_mine_wk2114_uart_ready = false;
        return ret;
    }

    mine_wk2114_oled_push_state("UART2+WK2114 OK");
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
 * @brief 执行 WK2114 启动流程并在失败时自动重试。
 *
 * 重试阶段分两步：
 * 1) 主口初始化 + 连通性检查失败时重试；
 * 2) 子串口1默认配置失败时重试。
 *
 * 该流程确保模块上电后最终进入可工作状态，而不是首次失败后直接退出任务。
 *
 * @return errcode_t
 * @retval ERRCODE_SUCC 启动流程完成。
 */
static errcode_t mine_wk2114_bootstrap_with_retry(void)
{
    errcode_t ret;
    uint32_t init_retry_count = 0;
    uint32_t sub1_retry_count = 0;

    while (1) {
        ret = mine_wk2114_uart2_ext_init();
        if (ret == ERRCODE_SUCC) {
            break;
        }

        init_retry_count++;
        mine_wk2114_oled_push_state("INIT RETRY");
        osal_printk("[mine wk2114] init retry #%lu, ret=%x\r\n",
            (unsigned long)init_retry_count, ret);
        mine_wk2114_oled_flush_pending();
        osal_msleep(MINE_WK2114_INIT_RETRY_WAIT_MS);
    }

    while (1) {
        ret = mine_wk2114_uart2_ext_set_subuart_baud(1, g_mine_wk2114_subuart_baud[0]);
        if (ret == ERRCODE_SUCC) {
            mine_wk2114_oled_push_state("SUB1 READY");
            return ERRCODE_SUCC;
        }

        sub1_retry_count++;
        mine_wk2114_oled_push_state("SUB1 RETRY");
        osal_printk("[mine wk2114] sub1 cfg retry #%lu, ret=%x\r\n",
            (unsigned long)sub1_retry_count, ret);
        mine_wk2114_oled_flush_pending();
        osal_msleep(MINE_WK2114_INIT_RETRY_WAIT_MS);
    }
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

    /*
     * 启动阶段必须做失败重试：
     * - 主口初始化/连通性失败时，自动重试直到成功；
     * - 子串口默认配置失败时，同样自动重试。
     */
    ret = mine_wk2114_bootstrap_with_retry();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] bootstrap failed unexpectedly, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("BOOT FAIL");
    }

    while (1) {
        mine_wk2114_oled_flush_pending();
        osal_msleep(MINE_WK2114_TASK_LOOP_WAIT_MS);
    }

    /* 防御性返回，满足编译器对非 void 任务入口的返回约束。 */
    return NULL;
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
