/**
 * @file mine_wk2114_uart2_ext.h
 * @brief WK2114 UART expansion chip driver for Hi3863
 */

#ifndef MINE_WK2114_UART2_EXT_H
#define MINE_WK2114_UART2_EXT_H

#include <stdint.h>
#include "errcode.h"

/* ---------------- 全局寄存器定义 (Page x) ---------------- */
#define WK2XXX_GENA  0x00
#define WK2XXX_GRST  0x01
#define WK2XXX_GIER  0x10
#define WK2XXX_GIFR  0x11

/* ---------------- 子串口寄存器定义 (Page 0) ---------------- */
#define WK2XXX_SPAGE 0x03
#define WK2XXX_SCR   0x04
#define WK2XXX_LCR   0x05
#define WK2XXX_FCR   0x06
#define WK2XXX_SIER  0x07
#define WK2XXX_SIFR  0x08
#define WK2XXX_TFCNT 0x09
#define WK2XXX_RFCNT 0x0A
#define WK2XXX_FSR   0x0B
#define WK2XXX_LSR   0x0C
#define WK2XXX_FDAT  0x0D

/* ---------------- 子串口寄存器定义 (Page 1) ---------------- */
#define WK2XXX_BAUD1 0x04
#define WK2XXX_BAUD0 0x05
#define WK2XXX_PRES  0x06
#define WK2XXX_RFTL  0x07
#define WK2XXX_TFTL  0x08

/* ---------------- 关键标志位 ---------------- */
#define WK2XXX_TXEN       (1<<0)
#define WK2XXX_RXEN       (1<<1)
#define WK2XXX_RFTRIG_IEN (1<<0)
#define WK2XXX_RXOUT_IEN  (1<<1)
#define WK2XXX_UT1INT     (1<<0)
#define WK2XXX_UT2INT     (1<<1)
#define WK2XXX_UT3INT     (1<<2)
#define WK2XXX_UT4INT     (1<<3)

/* ---------------- 波特率枚举 ---------------- */
enum WKBaud {
    B600, B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B500000
};

/* ---------------- 接口声明 ---------------- */
void WK_RstInit(void);
void WkWriteGReg(uint8_t greg, uint8_t dat);
uint8_t WkReadGReg(uint8_t greg);
void WkWriteSReg(uint8_t port, uint8_t sreg, uint8_t dat);
uint8_t WkReadSReg(uint8_t port, uint8_t sreg);
void WkWriteSFifo(uint8_t port, uint8_t *dat, uint8_t num);
void WkReadSFifo(uint8_t port, uint8_t *rec, uint8_t num);
void Wk_BaudAdaptive(void);
void Wk_Init(uint8_t port);
void Wk_DeInit(uint8_t port);
void Wk_SetBaud(uint8_t port, enum WKBaud baud);

/* ---------------- 应用层入口 ---------------- */
errcode_t mine_wk2114_uart2_ext_init(void);

#endif // MINE_WK2114_UART2_EXT_H
