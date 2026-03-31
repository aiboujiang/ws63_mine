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
#define WK2XXX_LCR   0x05U
#define WK2XXX_FCR   0x06U
#define WK2XXX_SIER  0x07U
#define WK2XXX_TFCNT 0x09U
#define WK2XXX_RFCNT 0x0AU
#define WK2XXX_FSR   0x0BU
#define WK2XXX_LSR   0x0CU

/* 子串口寄存器（Page1）。 */
#define WK2XXX_BAUD1 0x04U
#define WK2XXX_BAUD0 0x05U
#define WK2XXX_PRES  0x06U
#define WK2XXX_RFTL  0x07U
#define WK2XXX_TFTL  0x08U

/* 关键位定义。 */
#define WK2XXX_RXEN       (1U << 0)
#define WK2XXX_TXEN       (1U << 1)
#define WK2XXX_SLEEPEN    (1U << 2)
#define WK2XXX_RFTRIG_IEN (1U << 0)
#define WK2XXX_RXOVT_IEN  (1U << 1)

#define WK2XXX_FCR_RFRST  (1U << 0)
#define WK2XXX_FCR_TFRST  (1U << 1)
#define WK2XXX_FCR_RFEN   (1U << 2)
#define WK2XXX_FCR_TFEN   (1U << 3)

#define WK2XXX_FSR_ERR_MASK 0xF0U
#define WK2XXX_LSR_ERR_MASK 0x0FU

/* 子串口默认配置（对应 LCR 位定义：无校验、1 停止位、非红外）。 */
#define WK2XXX_LCR_DEFAULT 0x00U

/* 初始化验证轮询次数：用于等待 W1/R0 位自动回清。 */
#define WK2XXX_VERIFY_RETRY_MAX 4U

/*
 * 某些板级/芯片版本下，GENA/GIER 读回可能固定为 0x00，
 * 但子串口功能寄存器访问与通信仍正常。
 * 0: 读回异常仅告警，不阻断初始化；1: 严格要求读回位必须置位。
 */
#define WK2XXX_STRICT_GREG_VERIFY 0U

/* 静态寄存器访问函数前置声明（供初始化校验逻辑复用）。 */
static void wk2114_write_greg(uint8_t greg, uint8_t data);
static uint8_t wk2114_read_greg(uint8_t greg);
static void wk2114_write_sreg(uint8_t sub_port, uint8_t sreg, uint8_t data);
static uint8_t wk2114_read_sreg(uint8_t sub_port, uint8_t sreg);

/**
 * @brief 计算子串口对应位掩码。
 */
static uint8_t wk2114_subport_mask(uint8_t sub_port)
{
    return (uint8_t)(1U << (sub_port - 1U));
}

/**
 * @brief 计算 GRST 中子串口休眠状态位掩码。
 */
static uint8_t wk2114_subport_sleep_mask(uint8_t sub_port)
{
    return (uint8_t)(1U << (sub_port + 3U));
}

/**
 * @brief 等待 GRST 的 UTxRST 自动回 0，并校验 UTxSLEEP=0。
 *
 * 规格书说明：UTxRST 属于 W1/R0 位，写 1 后硬件会自动清 0。
 */
static errcode_t wk2114_wait_grst_ready(uint8_t sub_port)
{
    uint8_t retry;
    uint8_t grst;
    uint8_t rst_mask;
    uint8_t sleep_mask;

    rst_mask = wk2114_subport_mask(sub_port);
    sleep_mask = wk2114_subport_sleep_mask(sub_port);

    for (retry = 0U; retry < WK2XXX_VERIFY_RETRY_MAX; retry++) {
        grst = wk2114_read_greg(WK2XXX_GRST);
        if (((grst & rst_mask) == 0U) && ((grst & sleep_mask) == 0U)) {
            return ERRCODE_SUCC;
        }
        ws63_bsp_sleep_ms(1U);
    }

    osal_printk("[wk2114 final drv] verify fail: sub-uart%u GRST=0x%02x\r\n",
        (unsigned int)sub_port, (unsigned int)wk2114_read_greg(WK2XXX_GRST));
    return ERRCODE_FAIL;
}

/**
 * @brief 校验子串口初始化关键寄存器。
 *
 * 按规格书逐项验收：复位、使能、FIFO、波特率、格式、中断与错误标志。
 */
static errcode_t wk2114_verify_subport_init(uint8_t sub_port,
    uint8_t baud1_expect, uint8_t baud0_expect, uint8_t pres_expect,
    uint8_t sier_expect)
{
    uint8_t port_mask;
    uint8_t gena;
    uint8_t gier;
    uint8_t scr;
    uint8_t fcr;
    uint8_t tfcnt;
    uint8_t rfcnt;
    uint8_t lcr;
    uint8_t baud1_now;
    uint8_t baud0_now;
    uint8_t pres_now;
    uint8_t sier_now;
    uint8_t fsr;
    uint8_t lsr;
    uint8_t retry;

    if (wk2114_wait_grst_ready(sub_port) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    port_mask = wk2114_subport_mask(sub_port);

    /* GENA: 子串口时钟必须使能。 */
    gena = wk2114_read_greg(WK2XXX_GENA);
    if ((gena & port_mask) == 0U) {
        osal_printk("[wk2114 final drv] verify warn: sub-uart%u GENA readback=0x%02x\r\n",
            (unsigned int)sub_port, (unsigned int)gena);
#if (WK2XXX_STRICT_GREG_VERIFY == 1U)
        return ERRCODE_FAIL;
#endif
    }

    /* SCR: TXEN/RXEN 使能且 SLEEPEN 关闭。 */
    scr = wk2114_read_sreg(sub_port, WK2XXX_SCR);
    if (((scr & WK2XXX_TXEN) == 0U) || ((scr & WK2XXX_RXEN) == 0U) ||
        ((scr & WK2XXX_SLEEPEN) != 0U)) {
        osal_printk("[wk2114 final drv] verify fail: sub-uart%u SCR=0x%02x\r\n",
            (unsigned int)sub_port, (unsigned int)scr);
        return ERRCODE_FAIL;
    }

    /*
     * FCR: FIFO 使能位保持 1，复位位需自动回 0。
     * 若 FIFO 计数非 0，先补一次清 FIFO，再次确认为空。
     */
    for (retry = 0U; retry < WK2XXX_VERIFY_RETRY_MAX; retry++) {
        fcr = wk2114_read_sreg(sub_port, WK2XXX_FCR);
        if (((fcr & (WK2XXX_FCR_TFEN | WK2XXX_FCR_RFEN)) !=
            (WK2XXX_FCR_TFEN | WK2XXX_FCR_RFEN)) ||
            ((fcr & (WK2XXX_FCR_TFRST | WK2XXX_FCR_RFRST)) != 0U)) {
            ws63_bsp_sleep_ms(1U);
            continue;
        }

        tfcnt = wk2114_read_sreg(sub_port, WK2XXX_TFCNT);
        rfcnt = wk2114_read_sreg(sub_port, WK2XXX_RFCNT);
        if ((tfcnt == 0U) && (rfcnt == 0U)) {
            break;
        }

        wk2114_write_sreg(sub_port, WK2XXX_FCR, 0xFFU);
        ws63_bsp_sleep_ms(1U);
    }

    fcr = wk2114_read_sreg(sub_port, WK2XXX_FCR);
    tfcnt = wk2114_read_sreg(sub_port, WK2XXX_TFCNT);
    rfcnt = wk2114_read_sreg(sub_port, WK2XXX_RFCNT);
    if (((fcr & (WK2XXX_FCR_TFEN | WK2XXX_FCR_RFEN)) !=
        (WK2XXX_FCR_TFEN | WK2XXX_FCR_RFEN)) ||
        ((fcr & (WK2XXX_FCR_TFRST | WK2XXX_FCR_RFRST)) != 0U) ||
        (tfcnt != 0U) || (rfcnt != 0U)) {
        osal_printk("[wk2114 final drv] verify fail: sub-uart%u FCR=0x%02x TFCNT=%u RFCNT=%u\r\n",
            (unsigned int)sub_port, (unsigned int)fcr,
            (unsigned int)tfcnt, (unsigned int)rfcnt);
        return ERRCODE_FAIL;
    }

    /* BAUD1/BAUD0/PRES: PAGE1 读回值必须和写入值一致。 */
    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 1U);
    baud1_now = wk2114_read_sreg(sub_port, WK2XXX_BAUD1);
    baud0_now = wk2114_read_sreg(sub_port, WK2XXX_BAUD0);
    pres_now = wk2114_read_sreg(sub_port, WK2XXX_PRES);
    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 0U);
    if ((baud1_now != baud1_expect) || (baud0_now != baud0_expect) ||
        (pres_now != pres_expect)) {
        osal_printk("[wk2114 final drv] verify fail: sub-uart%u BAUD=%02x%02x PRES=%02x exp=%02x%02x/%02x\r\n",
            (unsigned int)sub_port,
            (unsigned int)baud1_now, (unsigned int)baud0_now, (unsigned int)pres_now,
            (unsigned int)baud1_expect, (unsigned int)baud0_expect, (unsigned int)pres_expect);
        return ERRCODE_FAIL;
    }

    /* LCR: 当前方案使用默认配置（普通 UART、无校验、1 停止位）。 */
    lcr = wk2114_read_sreg(sub_port, WK2XXX_LCR);
    if ((lcr & 0x3FU) != WK2XXX_LCR_DEFAULT) {
        osal_printk("[wk2114 final drv] verify fail: sub-uart%u LCR=0x%02x\r\n",
            (unsigned int)sub_port, (unsigned int)lcr);
        return ERRCODE_FAIL;
    }

    /* GIER/SIER: 全局与子串口中断使能位必须与配置一致。 */
    gier = wk2114_read_greg(WK2XXX_GIER);
    if ((gier & port_mask) == 0U) {
        osal_printk("[wk2114 final drv] verify warn: sub-uart%u GIER readback=0x%02x\r\n",
            (unsigned int)sub_port, (unsigned int)gier);
#if (WK2XXX_STRICT_GREG_VERIFY == 1U)
        return ERRCODE_FAIL;
#endif
    }

    sier_now = wk2114_read_sreg(sub_port, WK2XXX_SIER);
    if (sier_now != sier_expect) {
        osal_printk("[wk2114 final drv] verify fail: sub-uart%u SIER=0x%02x exp=0x%02x\r\n",
            (unsigned int)sub_port, (unsigned int)sier_now, (unsigned int)sier_expect);
        return ERRCODE_FAIL;
    }

    /* FSR/LSR: 溢出、帧错、校验错、Line-Break 均应为 0。 */
    fsr = wk2114_read_sreg(sub_port, WK2XXX_FSR);
    lsr = wk2114_read_sreg(sub_port, WK2XXX_LSR);
    if (((fsr & WK2XXX_FSR_ERR_MASK) != 0U) ||
        ((lsr & WK2XXX_LSR_ERR_MASK) != 0U)) {
        osal_printk("[wk2114 final drv] verify fail: sub-uart%u FSR=0x%02x LSR=0x%02x\r\n",
            (unsigned int)sub_port, (unsigned int)fsr, (unsigned int)lsr);
        return ERRCODE_FAIL;
    }

    osal_printk("[wk2114 final drv] sub-uart%u verify ok\r\n",
        (unsigned int)sub_port);
    return ERRCODE_SUCC;
}

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
    (void)ws63_bsp_host_uart_write(&value, 1U, 1000U);
}

/**
 * @brief 接收主口单字节（带超时）。
 */
static uint8_t wk2114_recv_byte(void)
{
    uint8_t value = 0U;
    int32_t retry = 200;

    while (ws63_bsp_host_uart_read(&value, 1U, 10U) <= 0) {
        ws63_bsp_sleep_ms(1U);
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
        ws63_bsp_reset_set(1U);
        ws63_bsp_sleep_ms(10U);
        ws63_bsp_reset_set(0U);
        ws63_bsp_sleep_ms(WS63_RESET_LOW_MS);
        ws63_bsp_reset_set(1U);
        ws63_bsp_sleep_ms(WS63_RESET_READY_MS);

        /* 多次发送 0x55，提升不同线缆条件下的锁定成功率。 */
        for (send_idx = 0U; send_idx < WS63_MATCH_SEND55_COUNT; send_idx++) {
            wk2114_send_byte(0x55U);
            ws63_bsp_sleep_ms(WS63_MATCH_GAP_MS);
        }

        ws63_bsp_sleep_ms(WS63_MATCH_LOCK_MS);
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

    ws63_bsp_reset_init();
    ret = ws63_bsp_host_uart_init(g_wk2114_host_rx_buffer,
        WS63_HOST_RX_BUFFER_SIZE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ws63_bsp_irq_init();
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

    if (!ws63_is_subport_valid(sub_port)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (ws63_calc_baud_regs(baud, &baud1, &baud0, &pres) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    port_mask = wk2114_subport_mask(sub_port);

    /*
     * 第一步：按应用笔记 Wk_Init 顺序完成子串口基础初始化。
     * 顺序依次为 GENA -> GRST -> GIER -> SIER -> FCR -> RFTL/TFTL -> SCR。
     */
    gena = wk2114_read_greg(WK2XXX_GENA);
    wk2114_write_greg(WK2XXX_GENA, (uint8_t)(gena | port_mask));

    grst = wk2114_read_greg(WK2XXX_GRST);
    wk2114_write_greg(WK2XXX_GRST, (uint8_t)(grst | port_mask));

    /*
     * 软复位位（W1/R0）可能在 1 个以上时钟后才自动回清。
     * 必须等待复位完成后再写 SIER/FCR/SCR/BAUD，避免配置被复位过程覆盖。
     */
    if (wk2114_wait_grst_ready(sub_port) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

    gier = wk2114_read_greg(WK2XXX_GIER);
    wk2114_write_greg(WK2XXX_GIER, (uint8_t)(gier | port_mask));

    sier = wk2114_read_sreg(sub_port, WK2XXX_SIER);
    sier = (uint8_t)(sier | WK2XXX_RFTRIG_IEN | WK2XXX_RXOVT_IEN);
    wk2114_write_sreg(sub_port, WK2XXX_SIER, sier);

    /* 按应用笔记示例直接写 0xFF：使能 FIFO、触发清 FIFO，并设置固定触点。 */
    wk2114_write_sreg(sub_port, WK2XXX_FCR, 0xFFU);

    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 1U);
    /* 应用笔记中 RFTL/TFTL 在 Wk_Init 阶段配置。 */
    wk2114_write_sreg(sub_port, WK2XXX_RFTL, WS63_RX_TRIGGER_LEVEL);
    wk2114_write_sreg(sub_port, WK2XXX_TFTL, WS63_TX_TRIGGER_LEVEL);
    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 0U);

    /* 使能子串口收发，显式关闭休眠位。 */
    scr = wk2114_read_sreg(sub_port, WK2XXX_SCR);
    scr = (uint8_t)(scr | WK2XXX_TXEN | WK2XXX_RXEN);
    scr = (uint8_t)(scr & (~WK2XXX_SLEEPEN));
    wk2114_write_sreg(sub_port, WK2XXX_SCR, scr);

    /*
     * 第二步：按应用笔记 Wk_SetBaud 流程写波特率寄存器。
     * 先切 PAGE1 写 BAUD1/BAUD0/PRES，再切回 PAGE0。
     */
    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 1U);
    wk2114_write_sreg(sub_port, WK2XXX_BAUD1, baud1);
    wk2114_write_sreg(sub_port, WK2XXX_BAUD0, baud0);
    wk2114_write_sreg(sub_port, WK2XXX_PRES, pres);
    wk2114_write_sreg(sub_port, WK2XXX_SPAGE, 0U);

    if (wk2114_verify_subport_init(sub_port, baud1, baud0, pres, sier) != ERRCODE_SUCC) {
        return ERRCODE_FAIL;
    }

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

    if ((!ws63_is_subport_valid(sub_port)) || (data == NULL) || (len == 0U)) {
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

    if ((!ws63_is_subport_valid(sub_port)) || (data == NULL) || (max_len == 0U)) {
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
