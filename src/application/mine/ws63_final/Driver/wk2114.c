/**
 * @file ws63_final_driver.c
 * @brief WK2114 最终版驱动层实现。
 */

#include "wk2114.h"

#include "osal_debug.h"

#include "ws63_final_bsp.h"
#include "ws63_final_common.h"
#include "ws63_final_config.h"

/* 全局寄存器。 */
#define WK2XXX_GENA  0x00U
#define WK2XXX_GRST  0x01U
#define WK2XXX_GIER  0x10U

/* 子串口寄存器（Page0）。 */
#define WK2XXX_SPAGE 0x03U
#define WK2XXX_SCR   0x04U
#define WK2XXX_FCR   0x06U
#define WK2XXX_SIER  0x07U
#define WK2XXX_RFCNT 0x0AU

/* 子串口寄存器（Page1）。 */
#define WK2XXX_BAUD1 0x04U
#define WK2XXX_BAUD0 0x05U
#define WK2XXX_PRES  0x06U
#define WK2XXX_RFTL  0x07U
#define WK2XXX_TFTL  0x08U

/* 关键位定义。 */
#define WK2XXX_TXEN       (1U << 0)
#define WK2XXX_RXEN       (1U << 1)
#define WK2XXX_RFTRIG_IEN (1U << 0)
#define WK2XXX_RXOUT_IEN  (1U << 1)

/* 主口接收缓存。 */
#define WS63_HOST_RX_BUFFER_SIZE 256U
static uint8_t g_wk2114_host_rx_buffer[WS63_HOST_RX_BUFFER_SIZE];

/* 链路状态缓存。 */
static wk2114_link_status_t g_wk2114_link_status = {0U, 0xFFU};

/**
 * @brief 发送主口单字节。
 */
static void wk2114_send_byte(uint8_t value)
{
    (void)wk2114_bsp_host_uart_write(&value, 1U, 1000U);
}

/**
 * @brief 接收主口单字节（带超时）。
 */
static uint8_t wk2114_recv_byte(void)
{
    uint8_t value = 0U;
    int32_t retry = 200;

    while (wk2114_bsp_host_uart_read(&value, 1U, 10U) <= 0) {
        wk2114_bsp_sleep_ms(1U);
        retry--;
        if (retry <= 0) {
            osal_printk("[wk2114 final drv] recv byte timeout\r\n");
            return 0xFFU;
        }
    }

    return value;
}

/**
 * @brief 写全局寄存器。
 */
static void wk2114_write_greg(uint8_t greg, uint8_t data)
{
    wk2114_send_byte(greg);
    wk2114_send_byte(data);
}

/**
 * @brief 读全局寄存器。
 */
static uint8_t wk2114_read_greg(uint8_t greg)
{
    wk2114_send_byte((uint8_t)(0x40U | greg));
    return wk2114_recv_byte();
}

/**
 * @brief 写子串口寄存器。
 */
static void wk2114_write_sreg(uint8_t sub_port, uint8_t sreg, uint8_t data)
{
    uint8_t cmd = (uint8_t)(((sub_port - 1U) << 4) | sreg);
    wk2114_send_byte(cmd);
    wk2114_send_byte(data);
}

/**
 * @brief 读子串口寄存器。
 */
static uint8_t wk2114_read_sreg(uint8_t sub_port, uint8_t sreg)
{
    uint8_t cmd = (uint8_t)(0x40U | ((sub_port - 1U) << 4) | sreg);
    wk2114_send_byte(cmd);
    return wk2114_recv_byte();
}

/**
 * @brief 向子串口 FIFO 写入一块数据（最大 16 字节）。
 */
static void wk2114_write_fifo_chunk(uint8_t sub_port, const uint8_t *data, uint8_t len)
{
    uint8_t idx;
    uint8_t cmd;

    cmd = (uint8_t)(0x80U | ((sub_port - 1U) << 4) | (len - 1U));
    wk2114_send_byte(cmd);
    for (idx = 0U; idx < len; idx++) {
        wk2114_send_byte(data[idx]);
    }
}

/**
 * @brief 从子串口 FIFO 读取一块数据。
 */
static void wk2114_read_fifo_chunk(uint8_t sub_port, uint8_t *data, uint8_t len)
{
    uint8_t idx;
    uint8_t cmd;

    cmd = (uint8_t)(0xC0U | ((sub_port - 1U) << 4) | (len - 1U));
    wk2114_send_byte(cmd);
    for (idx = 0U; idx < len; idx++) {
        data[idx] = wk2114_recv_byte();
    }
}

/**
 * @brief 执行主口自动波特率匹配。
 */
static void wk2114_auto_baud(void)
{
    uint8_t attempt;
    uint8_t send_idx;

    g_wk2114_link_status.matched = 0U;
    g_wk2114_link_status.last_gena = 0xFFU;

    for (attempt = 0U; attempt < WS63_MATCH_MAX_RETRY; attempt++) {
        wk2114_bsp_reset_set(1U);
        wk2114_bsp_sleep_ms(10U);
        wk2114_bsp_reset_set(0U);
        wk2114_bsp_sleep_ms(WS63_RESET_LOW_MS);
        wk2114_bsp_reset_set(1U);
        wk2114_bsp_sleep_ms(WS63_RESET_READY_MS);

        /* 多次发送 0x55，提升不同线缆条件下的锁定成功率。 */
        for (send_idx = 0U; send_idx < WS63_MATCH_SEND55_COUNT; send_idx++) {
            wk2114_send_byte(0x55U);
            wk2114_bsp_sleep_ms(WS63_MATCH_GAP_MS);
        }

        wk2114_bsp_sleep_ms(WS63_MATCH_LOCK_MS);
        g_wk2114_link_status.last_gena = wk2114_read_greg(WK2XXX_GENA);
        if (g_wk2114_link_status.last_gena != 0xFFU) {
            g_wk2114_link_status.matched = 1U;
            osal_printk("[wk2114 final drv] auto baud lock success, GENA=0x%02x\r\n",
                g_wk2114_link_status.last_gena);
            return;
        }

        osal_printk("[wk2114 final drv] auto baud try %u failed\r\n",
            (unsigned int)(attempt + 1U));
    }

    osal_printk("[wk2114 final drv] auto baud failed\r\n");
}

/**
 * @brief 初始化 WK2114 驱动。
 */
errcode_t wk2114_init(void)
{
    errcode_t ret;

    wk2114_bsp_reset_init();
    ret = wk2114_bsp_host_uart_init(g_wk2114_host_rx_buffer,
        WS63_HOST_RX_BUFFER_SIZE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    wk2114_bsp_irq_init();
    wk2114_auto_baud();
    if (g_wk2114_link_status.matched == 0U) {
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 初始化子串口并设置波特率。
 */
errcode_t wk2114_subport_init(uint8_t sub_port, uint32_t baud)
{
    uint8_t gena;
    uint8_t grst;
    uint8_t gier;
    uint8_t sier;
    uint8_t scr;
    uint8_t baud1 = 0U;
    uint8_t baud0 = 0U;
    uint8_t pres = 0U;
    uint8_t port_mask;

    if (!wk2114_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (wk2114_calc_baud_regs(baud, &baud1, &baud0, &pres) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    port_mask = (uint8_t)(1U << (sub_port - 1U));

    gena = wk2114_read_greg(WK2XXX_GENA);
    wk2114_write_greg(WK2XXX_GENA, (uint8_t)(gena | port_mask));

    grst = wk2114_read_greg(WK2XXX_GRST);
    wk2114_write_greg(WK2XXX_GRST, (uint8_t)(grst | port_mask));

    gier = wk2114_read_greg(WK2XXX_GIER);
    wk2114_write_greg(WK2XXX_GIER, (uint8_t)(gier | port_mask));

    sier = wk2114_read_sreg(sub_port, WK2XXX_SIER);
    sier = (uint8_t)(sier | WK2XXX_RFTRIG_IEN | WK2XXX_RXOUT_IEN);
    wk2114_write_sreg(sub_port, WK2XXX_SIER, sier);

    wk2114_write_sreg(sub_port, WK2XXX_FCR, 0xFFU);

    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 1U);
    wk2114_write_sreg(sub_port, WK2XXX_BAUD1, baud1);
    wk2114_write_sreg(sub_port, WK2XXX_BAUD0, baud0);
    wk2114_write_sreg(sub_port, WK2XXX_PRES, pres);
    wk2114_write_sreg(sub_port, WK2XXX_RFTL, WS63_RX_TRIGGER_LEVEL);
    wk2114_write_sreg(sub_port, WK2XXX_TFTL, WS63_TX_TRIGGER_LEVEL);
    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 0U);

    scr = wk2114_read_sreg(sub_port, WK2XXX_SCR);
    scr = (uint8_t)(scr | WK2XXX_TXEN | WK2XXX_RXEN);
    wk2114_write_sreg(sub_port, WK2XXX_SCR, scr);

    osal_printk("[wk2114 final drv] sub-uart%u init ok, baud=%u\r\n",
        (unsigned int)sub_port, (unsigned int)baud);
    return ERRCODE_SUCC;
}

/**
 * @brief 向子串口发送数据。
 */
errcode_t wk2114_subport_write(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0U;
    uint16_t remain;
    uint8_t chunk;

    if ((!wk2114_is_subport_valid(sub_port)) || (data == NULL) || (len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    while (offset < len) {
        remain = (uint16_t)(len - offset);
        chunk = (remain > WS63_FIFO_CHUNK_MAX) ?
            WS63_FIFO_CHUNK_MAX : (uint8_t)remain;
        wk2114_write_fifo_chunk(sub_port, &data[offset], chunk);
        offset = (uint16_t)(offset + chunk);
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 从子串口读取数据。
 */
uint8_t wk2114_subport_read(uint8_t sub_port, uint8_t *data, uint8_t max_len)
{
    uint8_t rx_cnt;

    if ((!wk2114_is_subport_valid(sub_port)) || (data == NULL) || (max_len == 0U)) {
        return 0U;
    }

    rx_cnt = wk2114_read_sreg(sub_port, WK2XXX_RFCNT);
    if (rx_cnt == 0U) {
        return 0U;
    }

    if (rx_cnt > max_len) {
        rx_cnt = max_len;
    }

    wk2114_read_fifo_chunk(sub_port, data, rx_cnt);
    return rx_cnt;
}

/**
 * @brief 查询主口链路状态。
 */
void wk2114_get_link_status(wk2114_link_status_t *status)
{
    if (status == NULL) {
        return;
    }

    *status = g_wk2114_link_status;
}
