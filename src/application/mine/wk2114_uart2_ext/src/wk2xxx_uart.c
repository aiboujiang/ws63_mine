 /*
*   FILE NAME  : wk2xxx_uart.c   
*
*   WKIC Ltd.
*   By  Xu XunWei Tech  
*   DEMO Version :2.4 Data:2022-09-24
*   DESCRIPTION: Implements an interface for the wk2xxx of spi interface
*
*  
*   
*/
#include <linux/init.h>                        
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/io.h>
#include "linux/version.h"
#include <linux/regmap.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <asm/byteorder.h>
#include <linux/busfreq-imx.h>
#include <linux/platform_data/serial-imx.h>
#include <linux/platform_data/dma-imx.h>

MODULE_LICENSE("Dual BSD/GPL");



#define DRIVER_AUTHOR "Xuxunwei"
#define DRIVER_DESC   "UART driver for UART to serial chip WK2114, etc."
#define VERSION_DESC  "V2.4 On 2022.09.24"
/*************The debug control **********************************/
//#define _DEBUG_WK_FUNCTION
//#define _DEBUG_WK_RX
//#define _DEBUG_WK_TX
//#define _DEBUG_WK_IRQ
//#define _DEBUG_WK_VALUE
//#define _DEBUG_WK_TEST

/*************Functional control interface************************/
#define WK_FIFO_FUNCTION
//#define WK_FlowControl_FUNCTION
#define WK_WORK_KTHREAD
//#define WK_RS485_FUNCTION
#define WK_RSTGPIO_FUNCTION
/*************SPI control interface******************************/
#define SPI_LEN_LIMIT       14    

/*************Uart Setting interface******************************/
#define WK2XXX_TXFIFO_LEVEL		(0x01) /* TX FIFO level */
#define WK2XXX_RXFIFO_LEVEL		(0x40) /* RX FIFO level */

#define WK2XXX_STATUS_PE    1
#define WK2XXX_STATUS_FE    2
#define WK2XXX_STATUS_BRK   4
#define WK2XXX_STATUS_OE    8





static DEFINE_MUTEX(wk2xxxs_lock);               
static DEFINE_MUTEX(wk2xxxs_reg_lock);              
static DEFINE_MUTEX(wk2xxxs_global_lock);

/******************************************/
#define 	NR_PORTS 	                4         
//
#define 	SERIAL_WK2XXX_MAJOR	    	207
#define 	CALLOUT_WK2XXX_MAJOR		208	
#define 	MINOR_START		            5
//wk2xxx hardware configuration
//#define 	WK_CRASTAL_CLK		        (24000000)
#define 	WK_CRASTAL_CLK		        (11059200)
#define 	WK2XXX_ISR_PASS_LIMIT	    2
#define		PORT_WK2XXX                 1
/******************************************/


/************** WK2XXX register definitions********************/
/*wk2xxx  Global register address defines*/
#define 	WK2XXX_GENA_REG     0X00    /*Slave UART Clock Set */
#define 	WK2XXX_GRST_REG     0X01    /*Reset Slave UART*/ 
#define		WK2XXX_GMUT_REG     0X02    /*Master UART Control*/
#define 	WK2XXX_GIER_REG     0X10    /*Slave UART Interrupt Enable */
#define 	WK2XXX_GIFR_REG     0X11    /*Slave UART Interrupt Flag*/
#define 	WK2XXX_GPDIR_REG    0X21    /*GPIO Direction*/ /*WK2168/WK2212*/
#define 	WK2XXX_GPDAT_REG    0X31    /*GPIO Data Input and Data Output*/ /*WK2168/WK2212*/


/*****************************
****wk2xxx  slave uarts  register address defines****
******************************/
#define 	WK2XXX_SPAGE_REG    0X03    /*Slave UART Register page selection*/
#define 	WK2XXX_PAGE1        1
#define 	WK2XXX_PAGE0        0

/*PAGE0**/
#define 	WK2XXX_SCR_REG      0X04    /*Slave UART Transmitter and Receiver Enable*/
#define 	WK2XXX_LCR_REG      0X05    /* Line Control */
#define 	WK2XXX_FCR_REG      0X06    /* FIFO control */
#define 	WK2XXX_SIER_REG     0X07    /* Interrupt enable */
#define 	WK2XXX_SIFR_REG     0X08    /* Interrupt Identification */
#define 	WK2XXX_TFCNT_REG    0X09    /* TX FIFO counter */
#define 	WK2XXX_RFCNT_REG    0X0A    /* RX FIFO counter */
#define 	WK2XXX_FSR_REG      0X0B    /* FIFO Status */
#define 	WK2XXX_LSR_REG      0X0C    /* Line Status */
#define 	WK2XXX_FDAT_REG     0X0D    /*  Write transmit FIFO data or Read receive FIFO data */
#define 	WK2XXX_FWCR_REG     0X0E    /* Flow  Control */
#define 	WK2XXX_RS485_REG    0X0F    /* RS485 Control */
/*PAGE1*/
#define 	WK2XXX_BAUD1_REG    0X04    /* Divisor Latch High */
#define 	WK2XXX_BAUD0_REG    0X05    /* Divisor Latch Low */
#define 	WK2XXX_PRES_REG     0X06    /* Divisor Latch Fractional Part */
#define 	WK2XXX_RFTL_REG     0X07    /* Receive FIFO Trigger Level */
#define 	WK2XXX_TFTL_REG     0X08    /* Transmit FIFO Trigger Level */
#define 	WK2XXX_FWTH_REG     0X09    /*Flow control trigger high level*/
#define 	WK2XXX_FWTL_REG     0X0A    /*Flow control trigger low level*/
#define 	WK2XXX_XON1_REG     0X0B    /* Xon1 word */
#define 	WK2XXX_XOFF1_REG    0X0C    /* Xoff1 word */
#define 	WK2XXX_SADR_REG     0X0D    /*RS485 auto address*/
#define 	WK2XXX_SAEN_REG     0X0E    /*RS485 auto address mask*/
#define 	WK2XXX_RRSDLY_REG   0X0F    /*RTS delay when transmit in RS485*/


//wkxxx register bit defines
/*GENA register*/
#define 	WK2XXX_GENA_UT4EN_BIT	    0x08
#define 	WK2XXX_GENA_UT3EN_BIT	    0x04
#define 	WK2XXX_GENA_UT2EN_BIT	    0x02
#define 	WK2XXX_GENA_UT1EN_BIT	    0x01
/*GRST register*/
#define 	WK2XXX_GRST_UT4SLEEP_BIT	0x80
#define 	WK2XXX_GRST_UT3SLEEP_BIT	0x40
#define 	WK2XXX_GRST_UT2SLEEP_BIT	0x20
#define 	WK2XXX_GRST_UT1SLEEP_BIT	0x10
#define 	WK2XXX_GRST_UT4RST_BIT	    0x08
#define 	WK2XXX_GRST_UT3RST_BIT		0x04
#define 	WK2XXX_GRST_UT2RST_BIT		0x02
#define 	WK2XXX_GRST_UT1RST_BIT		0x01
/*GIER register bits*/
#define 	WK2XXX_GIER_UT4IE_BIT		0x08
#define 	WK2XXX_GIER_UT3IE_BIT		0x04
#define 	WK2XXX_GIER_UT2IE_BIT		0x02
#define 	WK2XXX_GIER_UT1IE_BIT		0x01
/*GIFR register bits*/
#define 	WK2XXX_GIFR_UT4INT_BIT		0x08
#define 	WK2XXX_GIFR_UT3INT_BIT		0x04
#define 	WK2XXX_GIFR_UT2INT_BIT		0x02
#define 	WK2XXX_GIFR_UT1INT_BIT		0x01
/*SPAGE register bits*/
#define 	WK2XXX_SPAGE_PAGE_BIT	    0x01
/*SCR register bits*/
#define 	WK2XXX_SCR_SLEEPEN_BIT      0x04
#define 	WK2XXX_SCR_TXEN_BIT         0x02
#define 	WK2XXX_SCR_RXEN_BIT         0x01
/*LCR register bits*/
#define 	WK2XXX_LCR_BREAK_BIT	    0x20
#define 	WK2XXX_LCR_IREN_BIT         0x10

#define 	WK2XXX_LCR_PAEN_BIT         0x08
#define 	WK2XXX_LCR_PAM1_BIT         0x04
#define 	WK2XXX_LCR_PAM0_BIT         0x02
/*ODD Parity*/
#define     WK2XXX_LCR_ODD_PARITY       0x0a
/*Even Parity*/
#define     WK2XXX_LCR_EVEN_PARITY      0x0c
/*Parity :=0*/
#define     WK2XXX_LCR_SPACE_PARITY     0x08
/*Parity :=1*/           
#define     WK2XXX_LCR_MARK_PARITY      0x0e

#define 	WK2XXX_LCR_STPL_BIT         0x01


/*FCR register bits*/
#define 	WK2XXX_FCR_TFEN_BIT           0x08
#define 	WK2XXX_FCR_RFEN_BIT           0x02
#define 	WK2XXX_FCR_RFRST_BIT          0x01
/*SIER register bits*/
#define 	WK2XXX_SIER_FERR_IEN_BIT      0x80
#define 	WK2XXX_SIER_CTS_IEN_BIT       0x40
#define 	WK2XXX_SIER_RTS_IEN_BIT       0x20
#define 	WK2XXX_SIER_XOFF_IEN_BIT      0x10
#define 	WK2XXX_SIER_TFEMPTY_IEN_BIT   0x08
#define 	WK2XXX_SIER_TFTRIG_IEN_BIT    0x04
#define 	WK2XXX_SIER_RXOUT_IEN_BIT     0x02
#define 	WK2XXX_SIER_RFTRIG_IEN_BIT    0x01
/*SIFR register bits*/
#define 	WK2XXX_SIFR_FERR_INT_BIT      0x80
#define 	WK2XXX_SIFR_CTS_INT_BIT       0x40
#define 	WK2XXX_SIFR_RTS_INT_BIT       0x20
#define 	WK2XXX_SIFR_XOFF_INT_BIT      0x10
#define 	WK2XXX_SIFR_TFEMPTY_INT_BIT   0x08
#define 	WK2XXX_SIFR_TFTRIG_INT_BIT    0x04
#define 	WK2XXX_SIFR_RXOVT_INT_BIT     0x02
#define 	WK2XXX_SIFR_RFTRIG_INT_BIT    0x01
/*FSR register bits*/
#define 	WK2XXX_FSR_RFOE_BIT           0x80
#define 	WK2XXX_FSR_RFBI_BIT           0x40
#define 	WK2XXX_FSR_RFFE_BIT           0x20
#define 	WK2XXX_FSR_RFPE_BIT           0x10

#define 	WK2XXX_FSR_ERR_MASK           0xF0

#define 	WK2XXX_FSR_RDAT_BIT           0x08
#define 	WK2XXX_FSR_TDAT_BIT           0x04
#define 	WK2XXX_FSR_TFULL_BIT          0x02
#define 	WK2XXX_FSR_TBUSY_BIT          0x01
/*LSR register bits*/
#define     WK2XXX_LSR_BRK_ERROR_MASK     0X0F  /* BI, FE, PE, OE bits */
#define 	WK2XXX_LSR_OE_BIT             0x08
#define 	WK2XXX_LSR_BI_BIT             0x04
#define 	WK2XXX_LSR_FE_BIT             0x02
#define 	WK2XXX_LSR_PE_BIT             0x01
/*FWCR register bits*/
#define 	WK2XXX_FWCR_RTS_BIT           0x02
#define 	WK2XXX_FWCR_CTS _BIT          0x01
/*RS485 register bits*/
#define 	WK2XXX_RS485_RSRS485_BIT      0x40
#define 	WK2XXX_RS485_ATADD_BIT        0x20
#define 	WK2XXX_RS485_DATEN_BIT        0x10
#define 	WK2XXX_RS485_RTSEN_BIT        0x02
#define 	WK2XXX_RS485_RTSINV_BIT       0x01




/*************************************iMX8 UART start--------->>>>>>>>>>>>>>>********************************************/
/*************************************iMX8 UART register*****************************************************************/
/* IMX8 UART Register definitions */
#define IMX8_URXD0 0x0  /* Receiver Register */
#define IMX8_URTX0 0x40 /* Transmitter Register */
#define IMX8_UCR1  0x80 /* Control Register 1 */
#define IMX8_UCR2  0x84 /* Control Register 2 */
#define IMX8_UCR3  0x88 /* Control Register 3 */
#define IMX8_UCR4  0x8c /* Control Register 4 */
#define IMX8_UFCR  0x90 /* FIFO Control Register */
#define IMX8_USR1  0x94 /* Status Register 1 */
#define IMX8_USR2  0x98 /* Status Register 2 */
#define IMX8_UESC  0x9c /* Escape Character Register */
#define IMX8_UTIM  0xa0 /* Escape Timer Register */
#define IMX8_UBIR  0xa4 /* BRM Incremental Register */
#define IMX8_UBMR  0xa8 /* BRM Modulator Register */
#define IMX8_UBRC  0xac /* Baud Rate Count Register */
#define IMX8_UMCR  0xb8 /* */
#define IMX8_IMX21_ONEMS 0xb0 /* One Millisecond register */
#define IMX8_IMX1_UTS 0xd0 /* UART Test Register on i.mx1 */
#define IMX8_IMX21_UTS 0xb4 /* UART Test Register on all other i.mx*/

/* IMX8 UART Control Register Bit Fields.*/
#define IMX8_URXD_DUMMY_READ (1<<16)
#define IMX8_URXD_CHARRDY	(1<<15)
#define IMX8_URXD_ERR	(1<<14)
#define IMX8_URXD_OVRRUN	(1<<13)
#define IMX8_URXD_FRMERR	(1<<12)
#define IMX8_URXD_BRK	(1<<11)
#define IMX8_URXD_PRERR	(1<<10)
#define IMX8_URXD_RX_DATA	(0xFF<<0)
#define IMX8_UCR1_ADEN	(1<<15) /* Auto detect interrupt */
#define IMX8_UCR1_ADBR	(1<<14) /* Auto detect baud rate */
#define IMX8_UCR1_TRDYEN	(1<<13) /* Transmitter ready interrupt enable */
#define IMX8_UCR1_IDEN	(1<<12) /* Idle condition interrupt */
#define IMX8_UCR1_ICD_REG(x) (((x) & 3) << 10) /* idle condition detect */
#define IMX8_UCR1_RRDYEN	(1<<9)	/* Recv ready interrupt enable */
#define IMX8_UCR1_RDMAEN	(1<<8)	/* Recv ready DMA enable */
#define IMX8_UCR1_IREN	(1<<7)	/* Infrared interface enable */
#define IMX8_UCR1_TXMPTYEN	(1<<6)	/* Transimitter empty interrupt enable */
#define IMX8_UCR1_RTSDEN	(1<<5)	/* RTS delta interrupt enable */
#define IMX8_UCR1_SNDBRK	(1<<4)	/* Send break */
#define IMX8_UCR1_TDMAEN	(1<<3)	/* Transmitter ready DMA enable */
#define IMX8_IMX1_UCR1_UARTCLKEN (1<<2) /* UART clock enabled, i.mx1 only */
#define IMX8_UCR1_ATDMAEN    (1<<2)  /* Aging DMA Timer Enable */
#define IMX8_UCR1_DOZE	(1<<1)	/* Doze */
#define IMX8_UCR1_UARTEN	(1<<0)	/* UART enabled */
#define IMX8_UCR2_ESCI	(1<<15)	/* Escape seq interrupt enable */
#define IMX8_UCR2_IRTS	(1<<14)	/* Ignore RTS pin */
#define IMX8_UCR2_CTSC	(1<<13)	/* CTS pin control */
#define IMX8_UCR2_CTS	(1<<12)	/* Clear to send */
#define IMX8_UCR2_ESCEN	(1<<11)	/* Escape enable */
#define IMX8_UCR2_PREN	(1<<8)	/* Parity enable */
#define IMX8_UCR2_PROE	(1<<7)	/* Parity odd/even */
#define IMX8_UCR2_STPB	(1<<6)	/* Stop */
#define IMX8_UCR2_WS		(1<<5)	/* Word size */
#define IMX8_UCR2_RTSEN	(1<<4)	/* Request to send interrupt enable */
#define IMX8_UCR2_ATEN	(1<<3)	/* Aging Timer Enable */
#define IMX8_UCR2_TXEN	(1<<2)	/* Transmitter enabled */
#define IMX8_UCR2_RXEN	(1<<1)	/* Receiver enabled */
#define IMX8_UCR2_SRST	(1<<0)	/* SW reset */
#define IMX8_UCR3_DTREN	(1<<13) /* DTR interrupt enable */
#define IMX8_UCR3_PARERREN	(1<<12) /* Parity enable */
#define IMX8_UCR3_FRAERREN	(1<<11) /* Frame error interrupt enable */
#define IMX8_UCR3_DSR	(1<<10) /* Data set ready */
#define IMX8_UCR3_DCD	(1<<9)	/* Data carrier detect */
#define IMX8_UCR3_RI		(1<<8)	/* Ring indicator */
#define IMX8_UCR3_ADNIMP	(1<<7)	/* Autobaud Detection Not Improved */
#define IMX8_UCR3_RXDSEN	(1<<6)	/* Receive status interrupt enable */
#define IMX8_UCR3_AIRINTEN	(1<<5)	/* Async IR wake interrupt enable */
#define IMX8_UCR3_AWAKEN	(1<<4)	/* Async wake interrupt enable */
#define IMX8_UCR3_DTRDEN	(1<<3)	/* Data Terminal Ready Delta Enable. */
#define IMX8_IMX21_UCR3_RXDMUXSEL	(1<<2)	/* RXD Muxed Input Select */
#define IMX8_UCR3_INVT	(1<<1)	/* Inverted Infrared transmission */
#define IMX8_UCR3_BPEN	(1<<0)	/* Preset registers enable */
#define IMX8_UCR4_CTSTL_SHF	10	/* CTS trigger level shift */
#define IMX8_UCR4_CTSTL_MASK	0x3F	/* CTS trigger is 6 bits wide */
#define IMX8_UCR4_INVR	(1<<9)	/* Inverted infrared reception */
#define IMX8_UCR4_ENIRI	(1<<8)	/* Serial infrared interrupt enable */
#define IMX8_UCR4_WKEN	(1<<7)	/* Wake interrupt enable */
#define IMX8_UCR4_REF16	(1<<6)	/* Ref freq 16 MHz */
#define IMX8_UCR4_IDDMAEN    (1<<6)  /* DMA IDLE Condition Detected */
#define IMX8_UCR4_IRSC	(1<<5)	/* IR special case */
#define IMX8_UCR4_TCEN	(1<<3)	/* Transmit complete interrupt enable */
#define IMX8_UCR4_BKEN	(1<<2)	/* Break condition interrupt enable */
#define IMX8_UCR4_OREN	(1<<1)	/* Receiver overrun interrupt enable */
#define IMX8_UCR4_DREN	(1<<0)	/* Recv data ready interrupt enable */
#define IMX8_UFCR_RXTL_SHF	0	/* Receiver trigger level shift */
#define IMX8_UFCR_DCEDTE	(1<<6)	/* DCE/DTE mode select */
#define IMX8_UFCR_RFDIV	(7<<7)	/* Reference freq divider mask */
#define IMX8_UFCR_RFDIV_REG(x)	(((x) < 7 ? 6 - (x) : 6) << 7)
#define IMX8_UFCR_TXTL_SHF	10	/* Transmitter trigger level shift */
#define IMX8_USR1_PARITYERR	(1<<15) /* Parity error interrupt flag */
#define IMX8_USR1_RTSS	(1<<14) /* RTS pin status */
#define IMX8_USR1_TRDY	(1<<13) /* Transmitter ready interrupt/dma flag */
#define IMX8_USR1_RTSD	(1<<12) /* RTS delta */
#define IMX8_USR1_ESCF	(1<<11) /* Escape seq interrupt flag */
#define IMX8_USR1_FRAMERR	(1<<10) /* Frame error interrupt flag */
#define IMX8_USR1_RRDY	(1<<9)	 /* Receiver ready interrupt/dma flag */
#define IMX8_USR1_AGTIM	(1<<8)	 /* Ageing timer interrupt flag */
#define IMX8_USR1_DTRD	(1<<7)	 /* DTR Delta */
#define IMX8_USR1_RXDS	 (1<<6)	 /* Receiver idle interrupt flag */
#define IMX8_USR1_AIRINT	 (1<<5)	 /* Async IR wake interrupt flag */
#define IMX8_USR1_AWAKE	 (1<<4)	 /* Aysnc wake interrupt flag */
#define IMX8_USR2_ADET	 (1<<15) /* Auto baud rate detect complete */
#define IMX8_USR2_TXFE	 (1<<14) /* Transmit buffer FIFO empty */
#define IMX8_USR2_DTRF	 (1<<13) /* DTR edge interrupt flag */
#define IMX8_USR2_IDLE	 (1<<12) /* Idle condition */
#define IMX8_USR2_RIDELT	 (1<<10) /* Ring Interrupt Delta */
#define IMX8_USR2_RIIN	 (1<<9)	 /* Ring Indicator Input */
#define IMX8_USR2_IRINT	 (1<<8)	 /* Serial infrared interrupt flag */
#define IMX8_USR2_WAKE	 (1<<7)	 /* Wake */
#define IMX8_USR2_DCDIN	 (1<<5)	 /* Data Carrier Detect Input */
#define IMX8_USR2_RTSF	 (1<<4)	 /* RTS edge interrupt flag */
#define IMX8_USR2_TXDC	 (1<<3)	 /* Transmitter complete */
#define IMX8_USR2_BRCD	 (1<<2)	 /* Break condition */
#define IMX8_USR2_ORE	(1<<1)	 /* Overrun error */
#define IMX8_USR2_RDR	(1<<0)	 /* Recv data ready */
#define IMX8_UTS_FRCPERR	(1<<13) /* Force parity error */
#define IMX8_UTS_LOOP	(1<<12)	 /* Loop tx and rx */
#define IMX8_UTS_TXEMPTY	 (1<<6)	 /* TxFIFO empty */
#define IMX8_UTS_RXEMPTY	 (1<<5)	 /* RxFIFO empty */
#define IMX8_UTS_TXFULL	 (1<<4)	 /* TxFIFO full */
#define IMX8_UTS_RXFULL	 (1<<3)	 /* RxFIFO full */
#define IMX8_UTS_SOFTRST	 (1<<0)	 /* Software reset */

#define IMX_MODULE_MAX_CLK_RATE	80000000


/************************iMX8 UART  The data structure*********************************************/
/* device type dependent stuff */
struct imx_uart_data {
	unsigned uts_reg;
	//enum imx_uart_type devtype;
};
struct imx_port {
	struct uart_port	port;
	struct timer_list	timer;
	unsigned int		old_status;
	unsigned int		have_rtscts:1;
	unsigned int		have_rtsgpio:1;
	unsigned int		dte_mode:1;
	struct clk		*clk_ipg;
	struct clk		*clk_per;
	const struct imx_uart_data *devdata;

	struct mctrl_gpios *gpios;

	/* DMA fields */
	unsigned int		dma_is_inited:1;
	unsigned int		dma_is_enabled:1;
	unsigned int		dma_is_rxing:1;
	unsigned int		dma_is_txing:1;
	struct dma_chan		*dma_chan_rx, *dma_chan_tx;
	struct scatterlist	tx_sgl[2];
	unsigned int		tx_bytes;
	unsigned int            saved_reg[10];
	bool			context_saved;
#define DMA_TX_IS_WORKING 1
	unsigned long		flags;
};

struct imx_port *sport;
/******************************iMX8 UART  receive and send functions**********************************************/
void imx8_uart_putc(unsigned char ch1)
{
	int timeout=500,ret;
    while(!(readl(sport->port.membase + IMX8_USR2) & IMX8_USR2_TXFE)&&timeout)
	{   
		udelay(1);
		timeout--;
	}
	timeout=500;
	writel(ch1, sport->port.membase + IMX8_URTX0);
	ret=readl(sport->port.membase + IMX8_USR2) ;
	while((!(readl(sport->port.membase + IMX8_USR2)& IMX8_USR2_TXDC))&&timeout)
	{   
		udelay(1);
		timeout--;
	}
	//printk(KERN_ALERT "%s! end;:USR1:%X;USR2:%x,timeout:%d!!!\n",__func__,readl(sport->port.membase + USR1),readl(sport->port.membase + IMX8_USR2),timeout);
}

int imx8_uart_getc(unsigned char *dat)
{
    int timeout=1000;
	int ret=0;
	int reg;
	while(!(readl(sport->port.membase + IMX8_USR2) & IMX8_USR2_RDR)&&timeout)
	{
		--timeout;
		if(timeout==0){
		   	printk(KERN_ALERT "%s! imx8_uart get data timeout,USR2:0X%x!!!\n",__func__,readl(sport->port.membase + IMX8_USR2));
			ret=1;
		}
		udelay(1);
	}
    reg= readl(sport->port.membase + IMX8_URXD0);
	*dat=(unsigned char)reg;
	//printk(KERN_ALERT "%s! end;:USR1:%X;USR2:%x,reg:%x!!!\n",__func__,readl(sport->port.membase + USR1),readl(sport->port.membase + IMX8_USR2),reg);
	return ret;
}
void uart_imx8_reg_printf(void)
{   

	unsigned long dat;
	#ifdef _DEBUG_WK_FUNCTION
	printk(KERN_ALERT"%s!------begin-----!\n",__func__);
	#endif

	dat=readl(sport->port.membase + IMX8_UCR1);
	printk(KERN_ALERT "%s!,UCR1=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UCR2);
	printk(KERN_ALERT "%s!,UCR2=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UCR3);
	printk(KERN_ALERT "%s!,UCR3=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UCR4);
	printk(KERN_ALERT "%s!,UCR4=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UFCR);
	printk(KERN_ALERT "%s!,UFCR=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UBIR);
	printk(KERN_ALERT "%s!,UBIR=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UBMR);
	printk(KERN_ALERT "%s!,UBMR=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UMCR);
	printk(KERN_ALERT "%s!,UMCR=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_USR1);
	printk(KERN_ALERT "%s!,USR1=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_USR2);
	printk(KERN_ALERT "%s!,USR2=%lx!\n",__func__,dat);
	dat=readl(sport->port.membase + IMX8_UBRC);
	printk(KERN_ALERT "%s!,UBRC=%lx!\n",__func__,dat);

	#ifdef _DEBUG_WK_FUNCTION
	printk(KERN_ALERT"%s!------over-----!\n",__func__);
	#endif

}
void imx8_uart_clean_buf(void)
{
	readl(sport->port.membase + IMX8_URXD0);
    //readl(sport->port.membase + IMX8_URXD0);
    //readl(sport->port.membase + IMX8_URXD0);
}

#define TXTL 2 /* reset default */
#define RXTL 1 /* For console port */
#define RXTL_UART 16 /* For uart */

static int imx8_uart_setbuad(u32 baud)
{
	unsigned long ubir,ubmr;
    //baud=115200;
    /* Set the numerator value minus one of the BRM ratio */
    ubir = (baud / 100) - 1;

    /* Set the denominator value minus one of the BRM ratio    */
    ubmr = ((sport->port.uartclk / 1600) - 1);
	writel(ubir, sport->port.membase + IMX8_UBIR);
	writel(ubmr, sport->port.membase + IMX8_UBMR);
	return 0;
}


static int imx8_uart_init(void)
{   

	//int retval;
	unsigned long flags;
	#ifdef _DEBUG_WK_FUNCTION
	printk(KERN_ALERT"%s!------begin-----!\n",__func__);
	#endif
	spin_lock_irqsave(&sport->port.lock, flags);
	writel(0x0001, sport->port.membase + IMX8_UCR1);
	//writel(0x6127, sport->port.membase + IMX8_UCR2);
	writel(0x6027, sport->port.membase + IMX8_UCR2);
	writel(0x0704, sport->port.membase + IMX8_UCR3);
	writel(0x7c00, sport->port.membase + IMX8_UCR4);
	//writel(0x0a9e, sport->port.membase + IMX8_UFCR);
	//writel(0x08FF, sport->port.membase + IMX8_UBIR);
	//writel(0x0C34, sport->port.membase + IMX8_UBMR);
	imx8_uart_setbuad(250000);//250k
	writel(0x2201, sport->port.membase + IMX8_UCR1);
	writel(0x0000, sport->port.membase + IMX8_UMCR);
	writel(0x0a9e, sport->port.membase + IMX8_UFCR);//
	spin_unlock_irqrestore(&sport->port.lock, flags);

	#ifdef _DEBUG_WK_FUNCTION
	printk(KERN_ALERT"%s!------out-----!\n",__func__);
	#endif	
	return 0;
}

static void imx8_uart_remove(void)
{
	unsigned long temp;
	unsigned long flags;
	spin_lock_irqsave(&sport->port.lock, flags);
	temp = readl(sport->port.membase + IMX8_UCR1);
	temp &= ~(IMX8_UCR1_TXMPTYEN | IMX8_UCR1_RRDYEN | IMX8_UCR1_RTSDEN | IMX8_UCR1_UARTEN);
	writel(temp, sport->port.membase + IMX8_UCR1);
	spin_unlock_irqrestore(&sport->port.lock, flags);
	clk_disable_unprepare(sport->clk_per);
	clk_disable_unprepare(sport->clk_ipg);
}

/******************<<<<<<<<<---------iIMX8 UART  END----------------->>>>>>>>>>*****************************/

/*********************************************wk2xxx chip  start************************************************************************/
struct wk2xxx_devtype {
	char name[10];
	int	 nr_uart;
};
struct wk2xxx_one 
{

    struct uart_port port;//[NR_PORTS];
    struct kthread_work		start_tx_work;
	struct kthread_work		stop_rx_work;
    uint8_t line;
    uint8_t new_lcr_reg;
	uint8_t new_fwcr_reg;
    uint8_t new_scr_reg; 
    /*baud register*/
    uint8_t new_baud1_reg;
    uint8_t new_baud0_reg;
    uint8_t new_pres_reg;
};

struct wk2xxx_port 
{
    const struct wk2xxx_devtype	*devtype;
    struct uart_driver		uart;
    //struct platform_device *wk_pdev;
    struct imx_port         sport;
    struct workqueue_struct *workqueue;
    struct work_struct      work;
    unsigned char			buf[256];
	struct kthread_worker	kworker;
	struct task_struct		*kworker_task;
	struct kthread_work		irq_work;
    int irq_gpio_num;
    int rst_gpio_num;
    int irq_gpio;
    int minor;      /* minor number */
    int tx_empty; 
    struct wk2xxx_one		p[NR_PORTS];
};

static const struct wk2xxx_devtype wk2114_devtype = {
	.name		= "WK2114",
	.nr_uart	= 4,
};
static const struct wk2xxx_devtype wk2132_devtype = {
	.name		= "WK2132",
	.nr_uart	= 2,
};
static const struct wk2xxx_devtype wk2204_devtype = {
	.name		= "WK2204",
	.nr_uart	= 4,
};
static const struct wk2xxx_devtype wk2168_devtype = {
	.name		= "WK2168",
	.nr_uart	= 4,
};
static const struct wk2xxx_devtype wk2202_devtype = {
	.name		= "WK2202",
	.nr_uart	= 2,
};

#define to_wk2xxx_port(p,e)	((container_of((p), struct wk2xxx_port, e)))
#define to_wk2xxx_one(p,e)	((container_of((p), struct wk2xxx_one, e)))

/*
* This function read wk2xxx of Global register:
*/
static int wk2xxx_read_global_reg(uint8_t greg,uint8_t *dat)
{
     int ret=0;
     uint8_t wk_command;
     mutex_lock(&wk2xxxs_reg_lock);
     ret=0;
	 imx8_uart_clean_buf();
     wk_command=0x40|greg;
     imx8_uart_putc(wk_command);
     ret= imx8_uart_getc(dat);
	 if(ret==1){
	 	printk(KERN_ALERT "%s! wk_command==%x!!!\n",__func__,wk_command);
	}
   	 mutex_unlock(&wk2xxxs_reg_lock);
	 return ret;
}
/*
* This function write wk2xxx of Global register:
*/
static int wk2xxx_write_global_reg(uint8_t greg,uint8_t dat)
{
	uint8_t wk_command;
	mutex_lock(&wk2xxxs_reg_lock);
	wk_command= greg;
    imx8_uart_putc(wk_command);
    imx8_uart_putc(dat);
	mutex_unlock(&wk2xxxs_reg_lock);
    return 0;
}
/*
* This function read wk2xxx of slave register:
*/
static int wk2xxx_read_slave_reg(uint8_t port,uint8_t reg,uint8_t *dat)
{
     int ret=0;
     uint8_t wk_command;
     mutex_lock(&wk2xxxs_reg_lock);
     ret=0;
	 imx8_uart_clean_buf();
     wk_command=0x40|(((port-1)<<4)|reg);
     imx8_uart_putc(wk_command);
     ret= imx8_uart_getc(dat);
	 if(ret==1){
	 	printk(KERN_ALERT "%s! wk_command==%x!!!\n",__func__,wk_command);
	 }
   	 mutex_unlock(&wk2xxxs_reg_lock);
	 return ret;
}
/*
* This function write wk2xxx of Slave register:
*/
static int wk2xxx_write_slave_reg(uint8_t port,uint8_t reg,uint8_t dat)
{
       
	uint8_t wk_command;
	mutex_lock(&wk2xxxs_reg_lock);
	wk_command= (((port-1)<<4)|reg);
    imx8_uart_putc(wk_command);
    imx8_uart_putc(dat);
	mutex_unlock(&wk2xxxs_reg_lock);
    return 0;
}

#define MAX_RFCOUNT_SIZE 256

/*
* This function read wk2xxx of fifo:
*/
#if 0

static int wk2xxx_read_fifo(uint8_t port,uint8_t fifolen,uint8_t *dat)
{
	uint8_t wk_command;
	int i,ret=0;
    mutex_lock(&wk2xxxs_reg_lock);  
	imx8_uart_clean_buf();
	if(fifolen>0){
		wk_command=0xc0|(((port-1)<<4)|(fifolen-1));
		imx8_uart_putc(wk_command);
		for(i=0;i<fifolen;i++){
			ret=imx8_uart_getc(dat+i);
			if(ret==1){
			    printk(KERN_ALERT "%s! command=%x,i=%d,ret=0x%d!!!\n",__func__,wk_command,i,ret);
			}
		}
	}
    mutex_unlock(&wk2xxxs_reg_lock);
    return 0;
}

#endif 
/*
* This function write wk2xxx of fifo:
*/
static int wk2xxx_write_fifo(uint8_t port,uint8_t fifolen,uint8_t *dat)
{
	uint8_t wk_command;
	int i;
	mutex_lock(&wk2xxxs_reg_lock);
	if(fifolen>0){
		wk_command= (0x80|((port-1)<<4)|(fifolen-1));
		imx8_uart_putc(wk_command);
		for(i=0;i<fifolen;i++)
			imx8_uart_putc(*(dat+i));
	}
	mutex_unlock(&wk2xxxs_reg_lock);
    return 0;      
}

static void conf_wk2xxx_subport(struct uart_port *port);
static void wk2xxx_stop_tx(struct uart_port *port);
static u_int wk2xxx_tx_empty(struct uart_port *port);


static void wk2xxx_rx_chars(struct uart_port *port)
{   
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    uint8_t fsr,rx_dat[256]={0};
    uint8_t  rfcnt=0,rfcnt2=0;
    unsigned int flg, status = 0,rx_count=0;
    int rx_num=0,rxlen=0;
	//int len_rfcnt,len_limit,len_p=0;
	//len_limit=SPI_LEN_LIMIT;
    #ifdef _DEBUG_WK_FUNCTION
		printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif
    wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_FSR_REG,&fsr);
    if (fsr& WK2XXX_FSR_RDAT_BIT){
        wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt);
        if(rfcnt==0){
            wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt);   
        }
		wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt2);
        if(rfcnt2==0){
            wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_RFCNT_REG,&rfcnt2);
        }
        rfcnt=(rfcnt2>=rfcnt)?rfcnt:rfcnt2;
	    rxlen=(rfcnt==0)?256:rfcnt;	
    }
    #ifdef _DEBUG_WK_RX
     printk(KERN_ALERT "rx_chars()-port:%lx--fsr:0x%x--rxlen:%d--\n",one->port.iobase,fsr,rxlen);
    #endif
    flg = TTY_NORMAL;
        //#ifdef WK_FIFO_FUNCTION  
        #if 0   
                int len_rfcnt,len_limit,len_p=0;
                len_limit=SPI_LEN_LIMIT;

	            len_rfcnt=rxlen;
	            while(len_rfcnt){
			        if(len_rfcnt>len_limit){
				        wk2xxx_read_fifo(one->port.iobase,len_limit,rx_dat+len_p);
				        len_rfcnt=len_rfcnt-len_limit;
				        len_p=len_p+len_limit;
			        }else{
				        wk2xxx_read_fifo(one->port.iobase,len_rfcnt,rx_dat+len_p);//
				        len_rfcnt=0;
			        }
	            }
        #else
	        for(rx_num=0;rx_num<rxlen;rx_num++){
		        wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_FDAT_REG,&rx_dat[rx_num]);
		    }
        #endif

            one->port.icount.rx+=rxlen;
            for(rx_num=0;rx_num<rxlen;rx_num++){

                if(fsr&WK2XXX_FSR_ERR_MASK){ 
                    fsr &= WK2XXX_FSR_ERR_MASK ;
                        if (fsr&(WK2XXX_FSR_RFOE_BIT |WK2XXX_FSR_RFBI_BIT|WK2XXX_FSR_RFFE_BIT|WK2XXX_FSR_RFPE_BIT)){
                            if(fsr & WK2XXX_FSR_RFPE_BIT){
                                one->port.icount.parity++;
                                status |= WK2XXX_STATUS_PE;
                                flg = TTY_PARITY;
                            }

                            if (fsr & WK2XXX_FSR_RFFE_BIT){
                                one->port.icount.frame++;
                                status |= WK2XXX_STATUS_FE;
                                flg = TTY_FRAME;
                            }

                            if(fsr & WK2XXX_FSR_RFOE_BIT){
                                one->port.icount.overrun++;
                                status |= WK2XXX_STATUS_OE;
                                flg = TTY_OVERRUN;
                            }
                            if(fsr & WK2XXX_FSR_RFBI_BIT){
                                one->port.icount.brk++;
                                status |= WK2XXX_STATUS_BRK;
                                flg = TTY_BREAK;
                            }        
                        }
                } 
                if (uart_handle_sysrq_char(port,rx_dat[rx_num]))
                    continue;//
                #ifdef _DEBUG_WK_RX
                    printk(KERN_ALERT "rx_chars:0x%x----\n",rx_dat[rx_num]);
                #endif
                uart_insert_char(port, status, WK2XXX_STATUS_OE, rx_dat[rx_num], flg);
                rx_count++;
            }
            if(rx_count > 0){
  		        #ifdef _DEBUG_WK_RX
                    printk(KERN_ALERT  "push buffer tty flip port = :%lx count =:%d\n",one->port.iobase,rx_count);
  		        #endif
                tty_flip_buffer_push(&port->state->port);
                rx_count = 0;
            }
    #ifdef _DEBUG_WK_FUNCTION
	    printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif 	
}

static void wk2xxx_tx_chars(struct uart_port *port)
{   
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    uint8_t fsr,tfcnt,dat[1],txbuf[256]={0};
    int count,tx_count,i;
	int len_tfcnt,len_limit,len_p=0;
	len_limit=SPI_LEN_LIMIT;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif
    if (one->port.x_char) {   
        #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "wk2xxx_tx_chars   one->port.x_char:%x,port = %ld\n",one->port.x_char,one->port.iobase);
       	#endif
        wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FDAT_REG,one->port.x_char);
        one->port.icount.tx++;
        one->port.x_char = 0;
        goto out;
    }

    if(uart_circ_empty(&one->port.state->xmit) || uart_tx_stopped(&one->port)){
        goto out;
    }

    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_FSR_REG,&fsr);
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_TFCNT_REG,&tfcnt); 
	#ifdef _DEBUG_WK_TX
		printk(KERN_ALERT "wk2xxx_tx_chars   fsr:0x%x,tfcnt:0x%x,port = %ld\n",fsr,tfcnt,one->port.iobase);
	#endif
    if(tfcnt==0){
	    tx_count=(fsr & WK2XXX_FSR_TFULL_BIT)?0:256;
        #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "wk2xxx_tx_chars2   tx_count:%x,port = %ld\n",tx_count,one->port.iobase);
		#endif
    }else{
		tx_count=256-tfcnt;
	    #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "wk2xxx_tx_chars2   tx_count:%x,port = %ld\n",tx_count,one->port.iobase);
		#endif 
    } 
    if(tx_count>200){
        tx_count=200;
    } 
	count = tx_count;
	i=0;
	while(count){
  		if(uart_circ_empty(&one->port.state->xmit))
     		break;
	   	txbuf[i]=one->port.state->xmit.buf[one->port.state->xmit.tail];
	   	one->port.state->xmit.tail = (one->port.state->xmit.tail + 1) & (UART_XMIT_SIZE - 1);
	   	one->port.icount.tx++;
	   	i++;
		count=count-1;
        #ifdef _DEBUG_WK_TX
            printk(KERN_ALERT "tx_chars:0x%x--\n",txbuf[i-1]);
        #endif
    };

    #ifdef WK_FIFO_FUNCTION 
	len_tfcnt=i;    
	while(len_tfcnt){
		if(len_tfcnt>len_limit){
            wk2xxx_write_fifo( one->port.iobase,len_limit,txbuf+len_p);	
			len_p=len_p+len_limit;
			len_tfcnt=len_tfcnt-len_limit;
        }else{
            wk2xxx_write_fifo( one->port.iobase,len_tfcnt,txbuf+len_p);
			len_p=len_p+len_tfcnt;
			len_tfcnt=0;
		}
	}
    #else
	    for(count=0;count<i;count++){
            wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FDAT,txbuf[count]);	
        }
    #endif
    out:wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_FSR_REG,dat);
        fsr = dat[0];
    #ifdef _DEBUG_WK_VALUE
        printk(KERN_ALERT "%s!!-port:%ld;--FSR:0X%X--\n", __func__,one->port.iobase,fsr);
	#endif
    if(((fsr&WK2XXX_FSR_TDAT_BIT)==0)&&((fsr&WK2XXX_FSR_TBUSY_BIT)==0)){
        if (uart_circ_chars_pending(&one->port.state->xmit) < WAKEUP_CHARS){
            uart_write_wakeup(&one->port); 
        }
        if (uart_circ_empty(&one->port.state->xmit)){
            wk2xxx_stop_tx(&one->port);
        }
    }
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}






static void wk2xxx_port_irq(struct wk2xxx_port *s, int portno)//
{   
    //struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_one *one = &s->p[portno];
    unsigned int  pass_counter = 0;
    uint8_t sifr,sier;

    #ifdef _DEBUG_WK_IRQ
        uint8_t gier,sifr0,sifr1,sifr2,sifr3,sier1,sier0,sier2,sier3,gifr;

    #endif

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif


    #ifdef _DEBUG_WK_IRQ
        wk2xxx_read_global_reg(WK2XXX_GIFR_REG ,&gifr);
        wk2xxx_read_global_reg(WK2XXX_GIER_REG ,&gier);
        wk2xxx_read_slave_reg(1,WK2XXX_SIFR_REG,&sifr0);
        wk2xxx_read_slave_reg(2,WK2XXX_SIFR_REG,&sifr1);
        wk2xxx_read_slave_reg(3,WK2XXX_SIFR_REG,&sifr2);
        wk2xxx_read_slave_reg(4,WK2XXX_SIFR_REG,&sifr3);
        wk2xxx_read_slave_reg(1,WK2XXX_SIER_REG,&sier0);
        wk2xxx_read_slave_reg(2,WK2XXX_SIER_REG,&sier1);
        wk2xxx_read_slave_reg(3,WK2XXX_SIER_REG,&sier2);
        wk2xxx_read_slave_reg(4,WK2XXX_SIER_REG,&sier3);
        printk(KERN_ALERT "irq_app....gifr:%x  gier:%x  sier1:%x  sier2:%x sier3:%x sier4:%x   sifr1:%x sifr2:%x sifr3:%x sifr4:%x \n",gifr,gier,sier0,sier1,sier2,sier3,sifr0,sifr1,sifr2,sifr3);
    #endif           
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIFR_REG,&sifr);
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,&sier);
    #ifdef _DEBUG_WK_IRQ
        printk(KERN_ALERT "irq_app....port:%ld......sifr:%x sier:%x \n",one->port.iobase,sifr,sier);
    #endif

    do {
        if ((sifr&WK2XXX_SIFR_RFTRIG_INT_BIT)||(sifr&WK2XXX_SIFR_RXOVT_INT_BIT)){
            wk2xxx_rx_chars(&one->port);
        }
        
        if ((sifr & WK2XXX_SIFR_TFTRIG_INT_BIT)&&(sier & WK2XXX_SIER_TFTRIG_IEN_BIT)){
            wk2xxx_tx_chars(&one->port);
            return;
        }
        if (pass_counter++ > WK2XXX_ISR_PASS_LIMIT)
            break;
        wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIFR_REG,&sifr);
        wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,&sier);
        #ifdef _DEBUG_WK_VALUE
            printk(KERN_ALERT "irq_app...........rx............tx  sifr:%x sier:%x port:%ld\n",sifr,sier,one->port.iobase);
        #endif
    } while ((sifr&(WK2XXX_SIFR_RXOVT_INT_BIT|WK2XXX_SIFR_RFTRIG_INT_BIT))||((sifr & WK2XXX_SIFR_TFTRIG_INT_BIT)&&(sier & WK2XXX_SIER_TFTRIG_IEN_BIT)));
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}


static void wk2xxx_ist(struct kthread_work *ws)
{  
    struct wk2xxx_port *s = container_of(ws, struct wk2xxx_port, irq_work);

    uint8_t gifr,i;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif

    wk2xxx_read_global_reg(WK2XXX_GIFR_REG ,&gifr);
    while(1){

        for (i = 0; i < s->devtype->nr_uart; ++i){
            if(gifr&(0x01<<i)){
               wk2xxx_port_irq(s,i);
            }
        }

        wk2xxx_read_global_reg(WK2XXX_GIFR_REG ,&gifr);
        if(!(gifr&0x0f)){
            break;
        }
        
			
    }

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---exit--\n", __func__);
    #endif

}

static irqreturn_t wk2xxx_irq(int irq, void *dev_id)//
{
    struct wk2xxx_port *s = (struct wk2xxx_port *)dev_id;
    bool ret;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif  
#ifdef WK_WORK_KTHREAD
    ret=kthread_queue_work(&s->kworker, &s->irq_work); 
#else
    ret=queue_kthread_work(&s->kworker, &s->irq_work);  
#endif

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!ret:%d---exit--\n", __func__,ret);
    #endif
    return IRQ_HANDLED;
}

/*
 *   Return TIOCSER_TEMT when transmitter is not busy.
 */

static u_int wk2xxx_tx_empty(struct uart_port *port)// or query the tx fifo is not empty?
{
    uint8_t fsr=0;
    // struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    #ifdef _DEBUG_WK_FUNCTION
	    printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif
	mutex_lock(&wk2xxxs_lock);
  
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_FSR_REG,&fsr);
    while((fsr & WK2XXX_FSR_TDAT_BIT)|(fsr&WK2XXX_FSR_TBUSY_BIT)){
	   	wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_FSR_REG,&fsr);
	}
    s->tx_empty=((fsr&(WK2XXX_FSR_TBUSY_BIT|WK2XXX_FSR_TDAT_BIT))==0) ? TIOCSER_TEMT : 0;
	mutex_unlock(&wk2xxxs_lock);
      
    #ifdef _DEBUG_WK_FUNCTION
       printk(KERN_ALERT "%s!!-port:%ld;tx_empty:0x%x,fsr:0x%x--exit--\n", __func__,one->port.iobase,s->tx_empty,fsr);
    #endif
    return s->tx_empty;
}

static void wk2xxx_set_mctrl(struct uart_port *port, u_int mctrl)
{
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif
}
static u_int wk2xxx_get_mctrl(struct uart_port *port)
{       
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
    #endif
    return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}


static void wk2xxx_stop_tx(struct uart_port *port)//
{

    uint8_t sier;
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif

    mutex_lock(&wk2xxxs_lock);
	wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,&sier);
    sier&=~WK2XXX_SIER_TFTRIG_IEN_BIT;
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SIER_REG,sier);
	mutex_unlock(&wk2xxxs_lock); 
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
	#endif
  
}

static void wk2xxx_start_tx_proc(struct kthread_work *ws)
{
    struct wk2xxx_one *one = to_wk2xxx_one(ws, start_tx_work);
   //struct uart_port *port = &(to_wk2xxx_one(ws, start_tx_work)->port);
   // struct wk2xxx_port *s = dev_get_drvdata(port->dev);

    uint8_t rx;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif
    mutex_lock(&wk2xxxs_lock);
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,&rx);
    rx |= WK2XXX_SIER_TFTRIG_IEN_BIT|WK2XXX_SIER_RFTRIG_IEN_BIT|WK2XXX_SIER_RXOUT_IEN_BIT; 
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SIER_REG,rx);
    mutex_unlock(&wk2xxxs_lock); 
}

/*
 *  * 
*/
static void wk2xxx_start_tx(struct uart_port *port)
{
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    bool ret;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif
#ifdef WK_WORK_KTHREAD
    ret=kthread_queue_work(&s->kworker, &one->start_tx_work);
#else
    ret=queue_kthread_work(&s->kworker, &one->start_tx_work);
#endif

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;ret=%d--exit--\n", __func__,one->port.iobase,ret);
	#endif



}


static void wk2xxx_stop_rx_proc(struct kthread_work *ws)
{   
    struct wk2xxx_one *one = to_wk2xxx_one(ws, stop_rx_work);
    //struct uart_port *port = &(to_wk2xxx_one(ws, stop_rx_work)->port);
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    uint8_t rx;  
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
	#endif  
    mutex_lock(&wk2xxxs_lock); 
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,&rx);
    rx &=~WK2XXX_SIER_RFTRIG_IEN_BIT;
    rx &=~WK2XXX_SIER_RXOUT_IEN_BIT;
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SIER_REG,rx);

    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SCR_REG,&rx);
    rx &=~WK2XXX_SCR_RXEN_BIT;
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SCR_REG,rx);
    mutex_unlock(&wk2xxxs_lock); 

    

}

static void wk2xxx_stop_rx(struct uart_port *port)
{
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    bool ret;
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
       
	#endif
#ifdef WK_WORK_KTHREAD
    ret=kthread_queue_work(&s->kworker, &one->stop_rx_work);
#else
   ret=queue_kthread_work(&s->kworker, &one->stop_rx_work);
#endif

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;ret:%d--exit--\n", __func__,one->port.iobase,ret);
	#endif
}


/*
 *  * No modem control lines
 *   */
static void wk2xxx_enable_ms(struct uart_port *port)    //nothing
{
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif


}
/*
 *  * Interrupts always disabled.
*/   
static void wk2xxx_break_ctl(struct uart_port *port, int break_state)
{
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
}


static int wk2xxx_startup(struct uart_port *port)//i
{
    uint8_t gena,grst,gier,sier,scr,dat[1];
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
        //printk(KERN_ALERT "wk2xxx_start(iobase) port1:%ld,port2:%ld,port3:%ld,port4:%ld\n",s->p[0].port.iobase,s->p[1].port.iobase,s->p[2].port.iobase,s->p[3].port.iobase );
        //printk(KERN_ALERT "wk2xxx_start(iobase) line1:%d,line2:%d,line3:%d,line4:%d\n",s->p[0].line,s->p[1].line,s->p[2].line,s->p[3].line );  
	#endif

    mutex_lock(&wk2xxxs_global_lock);  
    wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
    gena=dat[0];
    switch (one->port.iobase){
        case 1:
            gena|=WK2XXX_GENA_UT1EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        case 2:
            gena|=WK2XXX_GENA_UT2EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        case 3:
            gena|=WK2XXX_GENA_UT3EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        case 4:
            gena|=WK2XXX_GENA_UT4EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        default:
		    printk(KERN_ALERT ":%s！！ bad iobase1: %d.\n", __func__,(uint8_t)one->port.iobase);
            break;
    }
 
    //wk2xxx_read_global_reg(,WK2XXX_GRST_REG,dat);
    grst=0;
    switch (one->port.iobase){
        case 1:
            grst|=WK2XXX_GRST_UT1RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        case 2:
            grst|=WK2XXX_GRST_UT2RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        case 3:
            grst|=WK2XXX_GRST_UT3RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        case 4:
            grst|=WK2XXX_GRST_UT4RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        default:
            printk(KERN_ALERT ":%s！！ bad iobase2: %d.\n", __func__,(uint8_t)one->port.iobase);
            break;
    }

	//enable the sub port interrupt
	wk2xxx_read_global_reg(WK2XXX_GIER_REG,dat);
	gier = dat[0];		
	switch (one->port.iobase)
	{
		case 1:
			gier|=WK2XXX_GIER_UT1IE_BIT;
			wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
			break;
		case 2:
			gier|=WK2XXX_GIER_UT2IE_BIT;
			wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
			break;
		case 3:
			gier|=WK2XXX_GIER_UT3IE_BIT;
			wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
			break;
		case 4:
			gier|=WK2XXX_GIER_UT4IE_BIT;
			wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
			break;
		default:
			printk(KERN_ALERT ":%s！！bad iobase3: %d.\n",__func__, (uint8_t)one->port.iobase);
			break;
	}   
    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,dat);
    sier = dat[0];
    sier &= ~WK2XXX_SIER_TFTRIG_IEN_BIT;
    sier |= WK2XXX_SIER_RFTRIG_IEN_BIT;
    sier |= WK2XXX_SIER_RXOUT_IEN_BIT;
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SIER_REG,sier);

    wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SCR_REG,dat);
    scr = dat[0] | WK2XXX_SCR_TXEN_BIT|WK2XXX_SCR_RXEN_BIT;
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SCR_REG,scr);

    //initiate the fifos
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FCR_REG,0xff);//initiate the fifos
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FCR_REG,0xfc);
    //set rx/tx interrupt 
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG,1);  
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_RFTL_REG,WK2XXX_RXFIFO_LEVEL);
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_TFTL_REG,WK2XXX_TXFIFO_LEVEL);
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG,0);  

	/*enable rs485*/
	#ifdef WK_RS485_FUNCTION
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_RS485_REG,0X02);//default  high
	//wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_RS485,0X03);//default low
	wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG,0X01);
	wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_RRSDLY_REG,0X10);
	wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG,0X00);
	#endif
        /*****************************test**************************************/
    #ifdef _DEBUG_WK_TEST
        wk2xxx_read_global_reg(WK2XXX_GENA_REG,&gena);
		wk2xxx_read_global_reg(WK2XXX_GIER_REG,&gier);
		wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SIER_REG,&sier);
		wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_SCR_REG,&scr);
		wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_FCR_REG,dat);
		printk(KERN_ALERT "%s!!-port:%ld;gena:0x%x;gier:0x%x;sier:0x%x;scr:0x%x;fcr:0x%x----\n", __func__,one->port.iobase,gena,gier,sier,scr,dat[0]);	
	#endif
		/**********************************************************************/

    mutex_unlock(&wk2xxxs_global_lock);
    uart_circ_clear(&one->port.state->xmit);
    wk2xxx_enable_ms(&one->port);

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
    return 0;
}

static void wk2xxx_shutdown(struct uart_port *port)
{

    uint8_t gena,grst,gier,dat[1];
    //struct wk2xxx_port *s = container_of(port,struct wk2xxx_port,port);
    struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif

    mutex_lock(&wk2xxxs_global_lock);
    wk2xxx_read_global_reg(WK2XXX_GIER_REG,&gier);
    switch (one->port.iobase){
        case 1:
            gier&=~WK2XXX_GIER_UT1IE_BIT;
            wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
            break;
        case 2:
            gier&=~WK2XXX_GIER_UT2IE_BIT;;
            wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
            break;
        case 3:
            gier&=~WK2XXX_GIER_UT3IE_BIT;;
            wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
            break;
        case 4:
            gier&=~WK2XXX_GIER_UT4IE_BIT;;
            wk2xxx_write_global_reg(WK2XXX_GIER_REG,gier);
            break;
        default:
            printk(KERN_ALERT "%s!! (GIER)bad iobase %d\n",__func__, (uint8_t)one->port.iobase);;
            break;
    }

    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SIER_REG,0x0);
	mutex_unlock(&wk2xxxs_global_lock);

    #ifdef WK_WORK_KTHREAD
        kthread_flush_work(&one->start_tx_work);
        kthread_flush_work(&one->stop_rx_work);
        kthread_flush_work(&s->irq_work);
        //kthread_flush_worker(&s->kworker);
    #else
        flush_kthread_work(&one->start_tx_work);
        flush_kthread_work(&one->stop_rx_work);
        flush_kthread_work(&s->irq_work);
        //flush_kthread_worker(&s->kworker);
    #endif


    mutex_lock(&wk2xxxs_global_lock);
    wk2xxx_read_global_reg(WK2XXX_GRST_REG,dat);
    grst=dat[0];
    switch (one->port.iobase){
        case 1:
            grst|=WK2XXX_GRST_UT1RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        case 2:
            grst|=WK2XXX_GRST_UT2RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        case 3:
            grst|=WK2XXX_GRST_UT3RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        case 4:
            grst|=WK2XXX_GRST_UT4RST_BIT;
            wk2xxx_write_global_reg(WK2XXX_GRST_REG,grst);
            break;
        default:
            printk(KERN_ALERT "%s!! bad iobase %d\n",__func__, (uint8_t)one->port.iobase);
            break;
    }

    wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
    gena=dat[0];
    switch (one->port.iobase){
        case 1:
            gena&=~WK2XXX_GENA_UT1EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        case 2:
            gena&=~WK2XXX_GENA_UT2EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        case 3:
            gena&=~WK2XXX_GENA_UT3EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        case 4:
            gena&=~WK2XXX_GENA_UT4EN_BIT;
            wk2xxx_write_global_reg(WK2XXX_GENA_REG,gena);
            break;
        default:
            printk(KERN_ALERT "%s!! bad iobase %d\n",__func__, (uint8_t)one->port.iobase);;
            break;
    }

	mutex_unlock(&wk2xxxs_global_lock);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}

static void conf_wk2xxx_subport(struct uart_port *port)//i
{   
    //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
	uint8_t sier=0,fwcr=0,lcr=0,scr=0,dat[1],baud0=0,baud1=0,pres=0,count=200;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
    #endif
	lcr = one->new_lcr_reg;
	//scr = s->new_scr_reg;
	baud0=one->new_baud0_reg;
	baud1=one->new_baud1_reg;
	pres=one->new_pres_reg;
	fwcr=one->new_fwcr_reg;
    /* Disable Uart all interrupts */
	wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_SIER_REG ,dat);
	sier = dat[0];
	wk2xxx_write_slave_reg(one->port.iobase,WK2XXX_SIER_REG,0X0);

	do{
        wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_FSR_REG,dat);
	} while ((dat[0] & WK2XXX_FSR_TBUSY_BIT)&&(count--));
    // then, disable tx and rx
    wk2xxx_read_slave_reg(one->port.iobase,WK2XXX_SCR_REG,dat);
    scr = dat[0];
    wk2xxx_write_slave_reg(one->port.iobase,WK2XXX_SCR_REG,scr&(~(WK2XXX_SCR_RXEN_BIT|WK2XXX_SCR_TXEN_BIT)));
    // set the parity, stop bits and data size //
    wk2xxx_write_slave_reg(one->port.iobase,WK2XXX_LCR_REG,lcr);
    #ifdef WK_FlowControl_FUNCTION
	if(fwcr>0){  
        printk(KERN_ALERT "%s!!---Flow Control  fwcr=0x%X\n",__func__,fwcr);
        // Configure flow control levels 
		wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FWCR_REG,fwcr);
        //Flow control halt level 0XF0, resume level 0X80 
		wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG ,1);
		wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FWTH_REG,0XF0);
		wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_FWTL_REG,0X80);
		wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG ,0);
    }
    #endif
    /* Setup baudrate generator */
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG ,1);
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_BAUD0_REG ,baud0);
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_BAUD1_REG ,baud1);
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_PRES_REG ,pres);
	#ifdef _DEBUG_WK_FUNCTION
        wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_BAUD0_REG,&baud1);
        wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_BAUD1_REG,&baud0);
        wk2xxx_read_slave_reg( one->port.iobase,WK2XXX_PRES_REG,&pres);
        printk(KERN_ALERT "%s!!---baud1:0x%x;baud0:0x%x;pres=0x%X.---\n", __func__,baud1,baud0,pres);
    #endif
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SPAGE_REG ,0);
    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SCR_REG ,scr|(WK2XXX_SCR_RXEN_BIT|WK2XXX_SCR_TXEN_BIT));

    wk2xxx_write_slave_reg( one->port.iobase,WK2XXX_SIER_REG ,sier);

    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
    #endif
}


static void wk2xxx_termios( struct uart_port *port, struct ktermios *termios,struct ktermios *old)
{

    struct wk2xxx_one *one = to_wk2xxx_one(port, port);
	int baud = 0;
    uint32_t temp=0,freq=0;
	uint8_t lcr=0,fwcr=0,baud1=0,baud0=0,pres=0,bParityType=0;

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--in--\n", __func__,one->port.iobase);
        printk(KERN_ALERT "%s!!---c_cflag:0x%x,c_iflag:0x%x.\n",__func__,termios->c_cflag,termios->c_iflag);
	#endif
	baud1=0;
	baud0=0;
	pres=0;
	baud = tty_termios_baud_rate(termios);

    freq=one->port.uartclk;
    if(freq>=(baud*16)){
		temp=(freq)/(baud*16);
		temp=temp-1;
		baud1=(uint8_t)((temp>>8)&0xff);
		baud0=(uint8_t)(temp&0xff);
		temp=(((freq%(baud*16))*100)/(baud));
		pres=(temp+100/2)/100;
		printk(KERN_ALERT "%s!!---freq:%d,baudrate:%d\n",__func__,freq,baud);
		printk(KERN_ALERT "%s!!---baud1:%x,baud0:%x,pres:%x\n",__func__,baud1,baud0,pres);
	}else{
		printk(KERN_ALERT "the baud rate:%d is too high！ \n",baud);
	}
	tty_termios_encode_baud_rate(termios, baud, baud);

    lcr =0;
    if (termios->c_cflag & CSTOPB)
        lcr|=WK2XXX_LCR_STPL_BIT;//two  stop_bits
    else
        lcr&=~WK2XXX_LCR_STPL_BIT;//one  stop_bits

    
    bParityType = termios->c_cflag & PARENB ?(termios->c_cflag & PARODD ? 1 : 2) +(termios->c_cflag & CMSPAR ? 2 : 0) : 0;
    if (termios->c_cflag & PARENB) {
        lcr|=WK2XXX_LCR_PAEN_BIT;//enbale spa
        switch (bParityType) {
            case 0x01:                  //ODD
                lcr |= WK2XXX_LCR_PAM0_BIT;  
                lcr &= ~WK2XXX_LCR_PAM1_BIT;
                break;
            case 0x02:                 //EVEN
		        lcr |= WK2XXX_LCR_PAM1_BIT;
                lcr &= ~WK2XXX_LCR_PAM0_BIT;
                break;
            case 0x03:                 //MARK--1
		        lcr |= WK2XXX_LCR_PAM1_BIT|WK2XXX_LCR_PAM0_BIT;
                break;
            case 0x04:                //SPACE--0
		        lcr &= ~WK2XXX_LCR_PAM1_BIT;
                lcr &= ~WK2XXX_LCR_PAM0_BIT;
                break;
            default:
		        lcr &= ~WK2XXX_LCR_PAEN_BIT;
                break;
	    }
    }


	/* Set read status mask */
	port->read_status_mask = WK2XXX_LSR_OE_BIT;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= WK2XXX_LSR_PE_BIT |
					  WK2XXX_LSR_FE_BIT;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= WK2XXX_LSR_BI_BIT;

    	/* Set status ignore mask */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNBRK)
		port->ignore_status_mask |= WK2XXX_LSR_BI_BIT;
	if (!(termios->c_cflag & CREAD))
		port->ignore_status_mask |= WK2XXX_LSR_BRK_ERROR_MASK;

    #ifdef WK_FlowControl_FUNCTION
	/* Configure flow control */
    if (termios->c_cflag & CRTSCTS){
        fwcr=0X30;
        printk(KERN_ALERT "wk2xxx_termios(2)----port:%lx;lcr:0x%x;fwcr:0x%x---\n",one->port.iobase,lcr,fwcr);
    }
    
	if (termios->c_iflag & IXON){
        printk(KERN_ALERT "%s!!---c_cflag:0x%x,IXON:0x%x.\n",__func__,termios->c_cflag,IXON);

    }
	if (termios->c_iflag & IXOFF){
        printk(KERN_ALERT "%s!!---c_cflag:0x%x,IXOFF:0x%x.\n",__func__,termios->c_cflag,IXOFF);
 
    }
    #endif


	one->new_baud1_reg=baud1;
	one->new_baud0_reg=baud0;	
	one->new_pres_reg=pres;
	one->new_lcr_reg = lcr;
	one->new_fwcr_reg = fwcr;

    #ifdef _DEBUG_WK_VALUE
        printk(KERN_ALERT "wk2xxx_termios()----port:%lx;lcr:0x%x;fwcr:0x%x---\n",one->port.iobase,lcr,fwcr);
    #endif

	conf_wk2xxx_subport(&one->port);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!-port:%ld;--exit--\n", __func__,one->port.iobase);
	#endif
}


static const char *wk2xxx_type(struct uart_port *port)
{

	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
    return port->type == PORT_WK2XXX ? "wk2xxx" : NULL;
}


static void wk2xxx_release_port(struct uart_port *port)
{
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif

}


static int wk2xxx_request_port(struct uart_port *port)
{
	#ifdef _DEBUG_WK_FUNCTION
    printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
    return 0;
}


static void wk2xxx_config_port(struct uart_port *port, int flags)
{
   //struct wk2xxx_port *s = dev_get_drvdata(port->dev);
   struct wk2xxx_one *one = to_wk2xxx_one(port, port);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!\n", __func__);
    #endif

    if (flags & UART_CONFIG_TYPE && wk2xxx_request_port(port) == 0)
        one->port.type = PORT_WK2XXX;
}


static int wk2xxx_verify_port(struct uart_port *port, struct serial_struct *ser)
{

    int ret = 0;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!---in--\n", __func__);
	#endif
    if (ser->type != PORT_UNKNOWN && ser->type != PORT_WK2XXX)
        ret = -EINVAL;
    if (port->irq != ser->irq)
        ret = -EINVAL;
    if (ser->io_type != SERIAL_IO_PORT)
        ret = -EINVAL;
    //if (port->uartclk / 16 != ser->baud_base)
    //     ret = -EINVAL;
    if (port->iobase != ser->port)
        ret = -EINVAL;
    if (ser->hub6 != 0)

        ret = -EINVAL;
    return ret;
}


static struct uart_ops wk2xxx_pops = {
    tx_empty:       wk2xxx_tx_empty,
    set_mctrl:      wk2xxx_set_mctrl,
    get_mctrl:      wk2xxx_get_mctrl,
    stop_tx:        wk2xxx_stop_tx,
    start_tx:       wk2xxx_start_tx,
    stop_rx:        wk2xxx_stop_rx,
    enable_ms:      wk2xxx_enable_ms,
    break_ctl:      wk2xxx_break_ctl,
    startup:        wk2xxx_startup,
    shutdown:       wk2xxx_shutdown,
    set_termios:    wk2xxx_termios,
    type:           wk2xxx_type,
    release_port:   wk2xxx_release_port,
    request_port:   wk2xxx_request_port,
    config_port:    wk2xxx_config_port,
    verify_port:    wk2xxx_verify_port,

};
static struct uart_driver wk2xxx_uart_driver = {

    owner:                  THIS_MODULE,
    major:                  SERIAL_WK2XXX_MAJOR,

    driver_name:            "ttySWK",
    dev_name:               "ttysWK",

    minor:                  MINOR_START,
    nr:                     NR_PORTS,
    cons:                   NULL
};

static int uart_driver_registered=0;
static struct platform_driver wk2xxx_driver;

#ifdef WK_RSTGPIO_FUNCTION
static int wk2xxx_spi_rstgpio_parse_dt(struct device *dev,int *rst_gpio)
{

	enum of_gpio_flags rst_flags; 
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif
	*rst_gpio = of_get_named_gpio_flags(dev->of_node, "reset-gpio", 0,&rst_flags);
    if (!gpio_is_valid(*rst_gpio)){
		printk(KERN_ERR"invalid wk2xxx_rst_gpio: %d\n", *rst_gpio);
		return -1;
    }
   
    if(	*rst_gpio){
		if (gpio_request(*rst_gpio , "rst_gpio")){
            printk(KERN_ERR"gpio_request failed!! rst_gpio: %d!\n",*rst_gpio);
		    gpio_free(*rst_gpio);
		    return  IRQ_NONE;
        }
    }
    gpio_direction_output(*rst_gpio,1);// output high
	printk(KERN_ERR"wk2xxx_rst_gpio: %d", *rst_gpio);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
	return 0;
}

#endif


static int wk2xxx_spi_irq_parse_dt(struct device *dev,int *irq_gpio)
{

	enum of_gpio_flags irq_flags; 
    int irq; 
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif
	*irq_gpio = of_get_named_gpio_flags(dev->of_node, "irq-gpio", 0,&irq_flags);
    if (!gpio_is_valid(*irq_gpio)){
		printk(KERN_ERR"invalid wk2xxx_irq_gpio: %d\n", *irq_gpio);
		return -1;
    }
   
    irq = gpio_to_irq(*irq_gpio);

    if(irq){
		if (gpio_request(*irq_gpio , "irq_gpio")){
            printk(KERN_ERR"gpio_request failed!! irq_gpio: %d!\n", irq);
		    gpio_free(*irq_gpio);
		    return  IRQ_NONE;
        }
    }else{
        printk(KERN_ERR"gpio_to_irq failed! irq: %d !\n", irq);
        return -ENODEV;
    }

	printk(KERN_ERR"wk2xxx_irq_gpio: %d, irq: %d", *irq_gpio, irq);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
	return irq;
}



static int wk2xxx_probe(struct platform_device *pdev)
{
    const struct sched_param sched_param = { .sched_priority = MAX_RT_PRIO / 2 };
    //const struct sched_param sched_param = { .sched_priority = 100 / 2 };
    uint8_t i;
    int ret, irq;
    uint8_t dat[1];
	static struct wk2xxx_port *s;

    void __iomem *base;
    struct resource *res;
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
    #endif
    
    /* Alloc port structure */
    
	s = devm_kzalloc(&pdev->dev, sizeof(*s) +sizeof(struct wk2xxx_one) * NR_PORTS,GFP_KERNEL);
	if (!s) {
	    printk(KERN_ALERT "wk2xxx_probe(devm_kzalloc) fail.\n");
		return -ENOMEM;
	}
    s->devtype=&wk2114_devtype;
    dev_set_drvdata(&pdev->dev, s);
    
    /*IMX8 uart set ************************ start*/
    /*IMX8 CPU-UART SET*/
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    //printk(KERN_ALERT "%s!!--,satrt:%x;end:%llx;name:%s\n", __func__,res->start,res->end,res->name);
    base = devm_ioremap_resource(&pdev->dev, res);
    //printk(KERN_ALERT "%s!!--base :%p--\n", __func__,base);
    if (IS_ERR(base))
		return PTR_ERR(base);
    s->sport.port.dev = &pdev->dev;
	s->sport.port.mapbase = res->start;
	s->sport.port.membase = base;
	s->sport.port.type = PORT_IMX,
	s->sport.port.iotype = UPIO_MEM;
	s->sport.port.fifosize = 32;
	s->sport.port.flags = UPF_BOOT_AUTOCONF;
	s->sport.clk_ipg = devm_clk_get(&pdev->dev, "ipg");
	if (IS_ERR(s->sport.clk_ipg)) {
		ret = PTR_ERR(s->sport.clk_ipg);
		dev_err(&pdev->dev, "failed to get ipg clk: %d\n", ret);
		return ret;
	}
	s->sport.clk_per = devm_clk_get(&pdev->dev, "per");
	if (IS_ERR(s->sport.clk_per)) {
		ret = PTR_ERR(s->sport.clk_per);
		dev_err(&pdev->dev, "failed to get per clk: %d\n", ret);
		return ret;
	}
	s->sport.port.uartclk = clk_get_rate(s->sport.clk_per);
	if (s->sport.port.uartclk > IMX_MODULE_MAX_CLK_RATE) {
		ret = clk_set_rate(s->sport.clk_per, IMX_MODULE_MAX_CLK_RATE);
		if (ret < 0) {
			dev_err(&pdev->dev, "clk_set_rate() failed\n");
			return ret;
		}
	}

	//sport->port.uartclk = clk_get_rate(sport->clk_per);
	/* For register access, we only need to enable the ipg clock. */
	ret = clk_prepare_enable(s->sport.clk_ipg);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable per clk: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(s->sport.clk_per);
	if (ret)
		return ret;
    sport=&s->sport;
	//uart_imx8_reg_printf();
	/*init imx8_uart register*/
	imx8_uart_init();
	//uart_imx8_reg_printf();
    /*IMX8 uart set ************************ end*/


     //Obtain the GPIO number of RST signal
    ret=wk2xxx_spi_rstgpio_parse_dt(&pdev->dev,&s->rst_gpio_num);
    if(ret!=0){
        printk(KERN_ALERT "wk2xxx_probe(rst_gpio_num)  rst_gpio_num= 0x%d\n",s->rst_gpio_num);
        ret=s->rst_gpio_num;
        goto out_gpio;

    }


    //Obtain the IRQ signal GPIO number and interrupt number
    irq = wk2xxx_spi_irq_parse_dt(&pdev->dev,&s->irq_gpio_num);
	if(irq<0){
        printk(KERN_ALERT "wk2xxx_probe(irq_gpio)  irq = 0x%d\n",irq);
        ret=irq;
        goto out_gpio;
	}
    s->irq_gpio = irq;
    
    /*Hardware resets the WK2114 chip*/
    mdelay(10);
    gpio_set_value(s->rst_gpio_num, 0); 	
	mdelay(10);
    gpio_set_value(s->rst_gpio_num, 1); 
    mdelay(10);

    /**put 0x55 to wk2114**/  
	printk(KERN_ALERT "%s!	imx8_uart_putc(0x55); sucess!!!\n",__func__);
    imx8_uart_putc(0x55);
	udelay(10);
    /**********************test spi **************************************/

	do{
	    wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
        wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
		printk(KERN_ERR "wk2xxx_probe(0xf0)  GENA = 0x%X\n",dat[0]);//GENA=0Xf0
		wk2xxx_write_global_reg(WK2XXX_GENA_REG,0xf5);
		wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
		printk(KERN_ERR "wk2xxx_probe(0xf5)  GENA = 0x%X\n",dat[0]);//GENA=0Xf5
		wk2xxx_write_global_reg(WK2XXX_GENA_REG,0xff);
		wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
		printk(KERN_ERR "wk2xxx_probe(0xff)  GENA = 0x%X\n",dat[0]);//GENA=0Xff
        wk2xxx_write_global_reg(WK2XXX_GENA_REG,0xf0);
	}while(0);
    /*Get interrupt number*/
    wk2xxx_write_global_reg(WK2XXX_GENA_REG,0x0);
    wk2xxx_read_global_reg(WK2XXX_GENA_REG,dat);
    if((dat[0]&0xf0)!=0xf0){ 
        printk(KERN_ALERT "wk2xxx_probe(0xf0)  GENA = 0x%X\n",dat[0]);
        printk(KERN_ALERT "The spi failed to read the register.!!!!\n");
        ret=-1;
        goto out_gpio;
    }

    /*Init kthread_worker  and kthread_work */  
#ifdef WK_WORK_KTHREAD
    kthread_init_worker(&(s->kworker));
	kthread_init_work(&s->irq_work, wk2xxx_ist);
#else
    init_kthread_worker(&(s->kworker));
	init_kthread_work(&s->irq_work, wk2xxx_ist);
#endif
	s->kworker_task = kthread_run(kthread_worker_fn, &s->kworker,
				      "wk2xxx");
	if (IS_ERR(s->kworker_task)) {
		ret = PTR_ERR(s->kworker_task);
		goto out_clk;
	}
	sched_setscheduler(s->kworker_task, SCHED_FIFO, &sched_param);

    /**/
    mutex_lock(&wk2xxxs_lock);
    if(!uart_driver_registered){
        uart_driver_registered = 1;
        ret = uart_register_driver(&wk2xxx_uart_driver);
        if (ret){
            printk(KERN_ERR "Couldn't register Wk2xxx uart driver\n");
            mutex_unlock(&wk2xxxs_lock);
            goto out_clk;
        }
    }

    printk(KERN_ALERT "wk2xxx_serial_init.\n");
    for(i =0;i<NR_PORTS;i++){
        s->p[i].line          = i;
        s->p[i].port.dev	= &pdev->dev;
		s->p[i].port.line     = i;
		s->p[i].port.ops      = &wk2xxx_pops;
		s->p[i].port.uartclk  = WK_CRASTAL_CLK;
		s->p[i].port.fifosize = 256;
		s->p[i].port.iobase   = i+1;
		//s->p[i].port.irq      = irq;
		s->p[i].port.iotype   = SERIAL_IO_PORT;
        s->p[i].port.flags    = UPF_BOOT_AUTOCONF;
		//s->p[i].port.flags    = ASYNC_BOOT_AUTOCONF;
        //s->p[i].port.iotype   = UPIO_PORT;
		//s->p[i].port.flags    = UPF_FIXED_TYPE | UPF_LOW_LATENCY;
#ifdef WK_WORK_KTHREAD
        kthread_init_work(&s->p[i].start_tx_work,wk2xxx_start_tx_proc);
	    kthread_init_work(&s->p[i].stop_rx_work, wk2xxx_stop_rx_proc);
#else
        init_kthread_work(&s->p[i].start_tx_work, wk2xxx_start_tx_proc);
	    init_kthread_work(&s->p[i].stop_rx_work, wk2xxx_stop_rx_proc);
#endif
        /* Register uart port */
		ret = uart_add_one_port(&wk2xxx_uart_driver, &s->p[i].port);
       	if(ret<0){
            printk(KERN_ALERT "uart_add_one_port failed for line i:= %d with error %d\n",i,ret);
		    mutex_unlock(&wk2xxxs_lock);
            goto out_port;
        }

        printk(KERN_ALERT "uart_add_one_port：%ld. status= 0x%d\n",s->p[i].port.iobase,ret);
    }
 
    mutex_unlock(&wk2xxxs_lock);

   /* Setup interrupt */
	ret = devm_request_irq(&pdev->dev, irq, wk2xxx_irq,IRQF_TRIGGER_FALLING, dev_name(&pdev->dev), s);
  
	if (!ret){
        printk(KERN_ALERT "devm_request_irq success. ret=%d.\n",ret);
        return 0;
    }
   
out_port:
	for (i=0; i<NR_PORTS; i++) {
        printk(KERN_ALERT "uart_remove_one_port：%ld. status= 0x%d\n",s->p[i].port.iobase,ret);
		uart_remove_one_port(&wk2xxx_uart_driver, &s->p[i].port);
	}
out_clk:
    kthread_stop(s->kworker_task); 
out_gpio:
    if(s->irq_gpio_num>0){
        printk(KERN_ALERT "gpio_free(s->irq_gpio_num)= 0x%d,ret=0x%d\n",s->irq_gpio_num,ret);
        gpio_free(s->irq_gpio_num);
        s->irq_gpio_num=0;
    }
    if(s->rst_gpio_num>0){
        printk(KERN_ALERT "gpio_free(s->rst_gpio_num)= 0x%d,ret=0x%d\n",s->rst_gpio_num,ret);
        gpio_free(s->rst_gpio_num); 
        s->rst_gpio_num=0; 
    }
	return ret;
}


static int wk2xxx_remove(struct platform_device *pdev)
{

    int i;
    struct wk2xxx_port *s = dev_get_drvdata(&pdev->dev);
	#ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--in--\n", __func__);
	#endif
    imx8_uart_remove();

    mutex_lock(&wk2xxxs_lock);
    for(i =0;i<NR_PORTS;i++){
        uart_remove_one_port(&wk2xxx_uart_driver, &s->p[i].port);
        printk(KERN_ALERT "%s!--uart_remove_one_port：%d.\n", __func__,i);
    }
#ifdef WK_WORK_KTHREAD
    kthread_flush_worker(&s->kworker);
#else
    flush_kthread_worker(&s->kworker);
#endif
	kthread_stop(s->kworker_task);
    /*
    if (s->irq_gpio){    
        free_irq(s->irq_gpio, s);
        printk(KERN_ALERT "%s!--,free_irq(s->irq_gpio, s);\n", __func__);
    }
    */
    if(s->irq_gpio_num>0){
        gpio_free(s->irq_gpio_num);
        printk(KERN_ALERT "%s!--,gpio_free(s->irq_gpio_num);\n", __func__);
    }
    if(s->rst_gpio_num>0){
        gpio_free(s->rst_gpio_num);
        printk(KERN_ALERT "%s!--,gpio_free(s->rst_gpio_num);\n", __func__);  
    }

    printk( KERN_ERR"removing wk2xxx_uart_driver\n");
    uart_unregister_driver(&wk2xxx_uart_driver);
    mutex_unlock(&wk2xxxs_lock);
    devm_kfree(&pdev->dev,s);
    #ifdef _DEBUG_WK_FUNCTION
        printk(KERN_ALERT "%s!!--exit--\n", __func__);
	#endif
    return 0;
}

static const struct of_device_id wk2xxx_of_match[] = {
	{ .compatible = "wkmic,wk2114_uart" },
	{}
};
MODULE_DEVICE_TABLE(of, wk2xxx_of_match);

static struct platform_driver  wk2xxx_driver = {
        .driver = {
                .name           = "wk2xxxuart",
                .owner          = THIS_MODULE,
		        .of_match_table = of_match_ptr(wk2xxx_of_match),
        },

        .probe          = wk2xxx_probe,
        .remove         = wk2xxx_remove,
};

static int __init wk2xxx_init(void)
{

    int ret;
    printk(KERN_ALERT"%s: " DRIVER_DESC "\n",__func__);
	printk(KERN_ALERT "%s: " VERSION_DESC "\n",__func__);
    ret= platform_driver_register(&wk2xxx_driver);
    if(ret<0){
        printk(KERN_ALERT "%s,failed to init wk2xxx spi;ret= :%d\n",__func__,ret);
    }
    return ret;
}

static void __exit wk2xxx_exit(void)
{

    printk(KERN_ALERT "%s!!--in--\n", __func__);
    platform_driver_unregister(&wk2xxx_driver);
}
module_init(wk2xxx_init);
module_exit(wk2xxx_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");






