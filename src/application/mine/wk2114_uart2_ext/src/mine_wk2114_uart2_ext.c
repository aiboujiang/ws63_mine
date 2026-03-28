/**
 * @file mine_wk2114_uart2_ext.c
 * @brief WK2114芯片业务代码，严格按照《WK2114 应用笔记.pdf》第5.2节及时序适配Hi3863
 */

#include "mine_wk2114_uart2_ext.h"
#include "pinctrl.h"
#include "gpio.h"
#include "uart.h"
#include "osal_debug.h"
#include "osal_task.h"
#include "osal_addr.h"

/* ---------------- 硬件配置宏定义 ---------------- */
#define WK_UART_BUS          UART_BUS_2
#define WK_UART_TX_PIN       8
#define WK_UART_RX_PIN       7
#define WK_UART_PIN_MODE     2

// RST引脚配置: GPIO10
#define WK_RST_PIN           10
#define WK_RST_PIN_MODE      0

// IRQ引脚配置: GPIO13
#define WK_IRQ_PIN           13
#define WK_IRQ_PIN_MODE      0

/*
 * 晶振频率配置：
 * 1) WS63板接12MHz晶振时使用12000000U（默认值）
 * 2) 独立WK2114 demo板接11.0592MHz时改成11059200U
 */
#ifndef WK_XTAL_FREQ_HZ
#define WK_XTAL_FREQ_HZ      12000000U
#endif

/* 主口自动匹配重试策略：复位后连续发送多个0x55，再读GENA判定是否锁定成功 */
#define WK_MATCH_MAX_RETRY   3
#define WK_MATCH_SEND_55_CNT 5
#define WK_RESET_LOW_MS      10
#define WK_RESET_READY_MS    20
#define WK_MATCH_BYTE_GAP_MS 5
#define WK_MATCH_LOCK_MS     20

#define MINE_UART_RX_BUFFER_SIZE 256
static uint8_t g_wk2114_uart_rx_buf[MINE_UART_RX_BUFFER_SIZE];
static uint8_t g_wk_main_uart_matched = 0;
static uint8_t g_wk_last_gena = 0xFF;

/**
 * @brief 将枚举波特率转换成数值
 * @param baud 枚举波特率
 * @return 波特率数值，返回0表示不支持
 */
static uint32_t wk_baud_enum_to_value(enum WKBaud baud)
{
    switch (baud) {
        case B600:
            return 600U;
        case B1200:
            return 1200U;
        case B2400:
            return 2400U;
        case B4800:
            return 4800U;
        case B9600:
            return 9600U;
        case B19200:
            return 19200U;
        case B38400:
            return 38400U;
        case B57600:
            return 57600U;
        case B115200:
            return 115200U;
        case B500000:
            return 500000U;
        default:
            return 0U;
    }
}

/**
 * @brief 按数据手册规则计算BAUD1/BAUD0/PRES
 * @param baud_val 目标波特率数值
 * @param baud1 高字节输出
 * @param baud0 低字节输出
 * @param pres 小数部分输出
 * @return ERRCODE_SUCC表示计算成功
 */
static errcode_t wk_calc_baud_regs(uint32_t baud_val, uint8_t *baud1, uint8_t *baud0, uint8_t *pres)
{
    uint64_t denom;
    uint64_t reg_x100;
    uint32_t reg_int;
    uint8_t reg_first_decimal;
    uint16_t baud_reg;

    if ((baud_val == 0U) || (baud1 == NULL) || (baud0 == NULL) || (pres == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    /*
     * Reg = Fosc / (16 * baud)
     * 这里先保留2位小数并四舍五入（满足手册“Reg通常精确到小数点后两位”）：
     * - 整数部分减1写入BAUD1/BAUD0
     * - 小数第一位写入PRES
     */
    denom = (uint64_t)baud_val * 16U;
    reg_x100 = ((uint64_t)WK_XTAL_FREQ_HZ * 100U + (denom / 2U)) / denom;
    reg_int = (uint32_t)(reg_x100 / 100U);

    if ((reg_int == 0U) || (reg_int > 0x10000U)) {
        return ERRCODE_FAIL;
    }

    /* 取小数点后第一位作为PRES，和手册给出的12MHz/115200=>PRES=5规则一致。 */
    reg_first_decimal = (uint8_t)((reg_x100 / 10U) % 10U);

    baud_reg = (uint16_t)(reg_int - 1U);
    *baud1 = (uint8_t)((baud_reg >> 8) & 0xFFU);
    *baud0 = (uint8_t)(baud_reg & 0xFFU);
    *pres = reg_first_decimal;
    return ERRCODE_SUCC;
}

/**
 * @brief 初始化底层主串口(Hi3863的UART2)
 */
static void q_uart2_init(void)
{
    // 配置引脚复用为UART2
    uapi_pin_set_mode(WK_UART_TX_PIN, WK_UART_PIN_MODE);
    uapi_pin_set_mode(WK_UART_RX_PIN, WK_UART_PIN_MODE);

    // 串口属性配置，默认波特率115200进行匹配
    uart_attr_t attr = {
        .baud_rate = 115200,   
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    uart_pin_config_t pin_cfg = {
        .tx_pin = WK_UART_TX_PIN,
        .rx_pin = WK_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };

    uart_buffer_config_t rx_buf_cfg = {
        .rx_buffer = g_wk2114_uart_rx_buf,
        .rx_buffer_size = sizeof(g_wk2114_uart_rx_buf)
    };

    (void)uapi_uart_deinit(WK_UART_BUS);
    if (uapi_uart_init(WK_UART_BUS, &pin_cfg, &attr, NULL, &rx_buf_cfg) != ERRCODE_SUCC) {
        osal_printk("[wk2114] uart2 init failed\r\n");
    } else {
        osal_printk("[wk2114] uart2 init success\r\n");
    }
}

/**
 * @brief 发送单字节给WK2114
 * @param dat 要发送的数据
 */
static void uart_sendByte(uint8_t dat)
{
    uapi_uart_write(WK_UART_BUS, &dat, 1, 1000);
}

/**
 * @brief 从WK2114接收单字节
 * @return 接收到的字节
 */
static uint8_t uart_recByte(void)
{
    uint8_t dat = 0;
    int retry = 200; // 200ms timeout max
    // 轮询阻塞式读取，这里为了保证时序采用一定超时，没有读到则重试
    while(uapi_uart_read(WK_UART_BUS, (const uint8_t *)&dat, 1, 10) <= 0) {
        osal_msleep(1); // 让出CPU，防止死锁
        if (--retry <= 0) {
            osal_printk("[wk2114] uart_recByte timeout!\r\n");
            return 0xFF; // timeout fallback
        }
    }
    return dat;
}

/**
 * @brief 复位引脚初始化
 * WK芯片需要用MCU的GPIO去控制RST引脚
 */
void WK_RstInit(void) 
{ 
    osal_printk("[wk2114] WK_RstInit start\r\n");
    // 配置GPIO10为基础GPIO模式
    uapi_pin_set_mode(WK_RST_PIN, WK_RST_PIN_MODE);
    
    // 初始化为推挽输出模式
    uapi_gpio_set_dir(WK_RST_PIN, GPIO_DIRECTION_OUTPUT);
    
    // 初始化RST为高电平(外部有上拉)
    uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_HIGH); 

    // 初始化主串口
    q_uart2_init();
    osal_printk("[wk2114] WK_RstInit end\r\n");
}

/**
 * @brief 写全局寄存器函数
 * @param greg 为全局寄存器的地址
 * @param dat 为写入寄存器的数据
 */
void WkWriteGReg(uint8_t greg, uint8_t dat) 
{ 
    uint8_t cmd; 
    cmd = 0 | greg; 
    uart_sendByte(cmd); // 写指令 
    uart_sendByte(dat); // 写数据 
}

/**
 * @brief 读全局寄存器
 * @param greg 为全局寄存器的地址
 * @return 返回的寄存器值
 */
uint8_t WkReadGReg(uint8_t greg) 
{ 
    uint8_t cmd, rec; 
    cmd = 0x40 | greg; 
    uart_sendByte(cmd); // 写指令 
    rec = uart_recByte(); // 接收返回的寄存器值 
    osal_printk("[wk2114] WkReadGReg 0x%02x = 0x%02x\r\n", greg, rec);
    return rec; 
}

/**
 * @brief 写子串口寄存器
 * @param port 为子串口
 * @param sreg 为子串口寄存器
 * @param dat 为写入数据
 */
void WkWriteSReg(uint8_t port, uint8_t sreg, uint8_t dat) 
{ 
    uint8_t cmd; 
    cmd = 0x0 | ((port - 1) << 4) | sreg; 
    uart_sendByte(cmd); // 写指令 
    uart_sendByte(dat); // 写数据 
}

/**
 * @brief 读子串口寄存器
 * @param port 为子串口端口号
 * @param sreg 为子串口寄存器地址
 * @return 返回的寄存器值
 */
uint8_t WkReadSReg(uint8_t port, uint8_t sreg) 
{ 
    uint8_t cmd, rec; 
    cmd = 0x40 | ((port - 1) << 4) | sreg; 
    uart_sendByte(cmd); // 写指令 
    rec = uart_recByte(); // 接收返回的寄存器值 
    return rec; 
}

/**
 * @brief 向子串口FIFO写入需要发送的数据
 * @param port 子串口号
 * @param dat 写入数据缓冲区
 * @param num 写入数据个数(最大不超过16字节)
 */
void WkWriteSFifo(uint8_t port, uint8_t *dat, uint8_t num) 
{ 
    uint8_t cmd, i; 
    cmd = 0x80 | ((port - 1) << 4) | (num - 1); 
    uart_sendByte(cmd); 
    for(i = 0; i < num; i++) { 
        uart_sendByte(*(dat + i)); 
    } 
}

/**
 * @brief 从子串口的FIFO中读出接收到的数据
 * @param port 子串口号
 * @param rec 接收的数据缓冲区
 * @param num 读出的数据个数
 */
void WkReadSFifo(uint8_t port, uint8_t *rec, uint8_t num) 
{ 
    uint8_t n, cmd; 
    cmd = 0xC0 | ((port - 1) << 4) | (num - 1); 
    uart_sendByte(cmd); 
    for(n = 0; n < num; n++) { 
        rec[n] = uart_recByte();  
    } 
}

/**
 * @brief 主串口波特率自动匹配
 */
void Wk_BaudAdaptive(void) 
{   
    uint8_t attempt;
    uint8_t i;

    g_wk_main_uart_matched = 0;
    g_wk_last_gena = 0xFF;

    for (attempt = 0; attempt < WK_MATCH_MAX_RETRY; attempt++) {
        osal_printk("[wk2114] Wk_BaudAdaptive try %u/%u\r\n", (unsigned int)(attempt + 1U),
            (unsigned int)WK_MATCH_MAX_RETRY);

        // 先拉高后拉低再拉高，保证每轮自适应前都经过完整硬复位。
        uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_HIGH);
        osal_msleep(10);
        uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_LOW);
        osal_msleep(WK_RESET_LOW_MS);
        uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_HIGH);
        osal_msleep(WK_RESET_READY_MS);

        // 连续发送0x55提升锁定概率，适配不同板级时钟与线长差异。
        for (i = 0; i < WK_MATCH_SEND_55_CNT; i++) {
            uart_sendByte(0x55);
            osal_msleep(WK_MATCH_BYTE_GAP_MS);
        }

        osal_msleep(WK_MATCH_LOCK_MS);
        g_wk_last_gena = WkReadGReg(WK2XXX_GENA);
        if (g_wk_last_gena != 0xFFU) {
            g_wk_main_uart_matched = 1;
            osal_printk("[wk2114] auto-baud lock success, GENA=0x%02x\r\n", g_wk_last_gena);
            return;
        }

        osal_printk("[wk2114] auto-baud lock fail on try %u, GENA=0xFF\r\n", (unsigned int)(attempt + 1U));
    }

    osal_printk("[wk2114] auto-baud lock failed after retries\r\n");
}

/**
 * @brief 初始化子串口
 * @param port 子串口号
 */
void Wk_Init(uint8_t port) 
{ 
    uint8_t gena, grst, gier, sier, scr; 

    // 使能子串口时钟 
    gena = WkReadGReg(WK2XXX_GENA); 
    gena = gena | (1 << (port - 1)); 
    WkWriteGReg(WK2XXX_GENA, gena); 

    // 软件复位子串口 
    grst = WkReadGReg(WK2XXX_GRST); 
    grst = grst | (1 << (port - 1)); 
    WkWriteGReg(WK2XXX_GRST, grst); 

    // 使能子串口总中断 
    gier = WkReadGReg(WK2XXX_GIER); 
    gier = gier | (1 << (port - 1)); 
    WkWriteGReg(WK2XXX_GIER, gier); 

    // 使能子串口接收触点中断和超时中断 
    sier = WkReadSReg(port, WK2XXX_SIER);  
    sier |= WK2XXX_RFTRIG_IEN | WK2XXX_RXOUT_IEN; 
    WkWriteSReg(port, WK2XXX_SIER, sier); 

    // 初始化FIFO和设置固定中断触点 
    WkWriteSReg(port, WK2XXX_FCR, 0xFF); 

    // 设置任意中断触点，如果下面的设置有效，那么上面FCR寄存器中断的固定中断触点将失效
    WkWriteSReg(port, WK2XXX_SPAGE, 1);     // 切换到page1 
    WkWriteSReg(port, WK2XXX_RFTL, 0x40);   // 设置接收触点为64个字节 
    WkWriteSReg(port, WK2XXX_TFTL, 0x10);   // 设置发送触点为16个字节 
    WkWriteSReg(port, WK2XXX_SPAGE, 0);     // 切换到page0  

    // 使能子串口的发送和接收使能 
    scr = WkReadSReg(port, WK2XXX_SCR);  
    scr |= WK2XXX_TXEN | WK2XXX_RXEN; 
    WkWriteSReg(port, WK2XXX_SCR, scr); 
}

/**
 * @brief 去初始化/关闭子串口
 * @param port 子串口号
 */
void Wk_DeInit(uint8_t port) 
{ 
    uint8_t gena, grst, gier; 

    // 关闭子串口总时钟 
    gena = WkReadGReg(WK2XXX_GENA); 
    gena = gena & (~(1 << (port - 1))); 
    WkWriteGReg(WK2XXX_GENA, gena); 

    // 关闭子串口总中断 
    gier = WkReadGReg(WK2XXX_GIER); 
    gier = gier & (~(1 << (port - 1))); 
    WkWriteGReg(WK2XXX_GIER, gier); 

    // 软件复位子串口 
    grst = WkReadGReg(WK2XXX_GRST); 
    grst = grst | (1 << (port - 1)); 
    WkWriteGReg(WK2XXX_GRST, grst); 
}

/**
 * @brief 设置子串口波特率函数
 * @param port 子串口号
 * @param baud 波特率大小
 */
void Wk_SetBaud(uint8_t port, enum WKBaud baud) 
{   
    uint8_t baud1 = 0;
    uint8_t baud0 = 0;
    uint8_t pres = 0;
    uint32_t baud_val;

    baud_val = wk_baud_enum_to_value(baud);
    if (wk_calc_baud_regs(baud_val, &baud1, &baud0, &pres) != ERRCODE_SUCC) {
        // 计算失败时回退到9600，避免寄存器写入非法值导致链路进一步失效。
        (void)wk_calc_baud_regs(9600U, &baud1, &baud0, &pres);
        osal_printk("[wk2114] Wk_SetBaud invalid enum=%u, fallback 9600\r\n", (unsigned int)baud);
    }

    osal_printk("[wk2114] Wk_SetBaud fosc=%u, baud=%u => BAUD1=0x%02x BAUD0=0x%02x PRES=0x%02x\r\n",
        (unsigned int)WK_XTAL_FREQ_HZ, (unsigned int)baud_val, baud1, baud0, pres);

    WkWriteSReg(port, WK2XXX_SPAGE, 1); 
    WkWriteSReg(port, WK2XXX_BAUD1, baud1); 
    WkWriteSReg(port, WK2XXX_BAUD0, baud0); 
    WkWriteSReg(port, WK2XXX_PRES, pres); 
    WkWriteSReg(port, WK2XXX_SPAGE, 0); 
}

/**
 * @brief IRQ 引脚中断回调
 */
static void wk_irq_callback(pin_t pin, uintptr_t param)
{
    if (pin == WK_IRQ_PIN) {
        // ...WK_IrqApp 可以从这触发事件丢给应用线程处理
        osal_printk("[wk2114] recv IRQ, param=%lu\n", (unsigned long)param);
    }
}

/**
 * @brief WK2114 应用层入口初始化
 */
errcode_t mine_wk2114_uart2_ext_init(void)
{
    osal_printk("[wk2114] mine_wk2114_uart2_ext_init start, fosc=%uHz\r\n", (unsigned int)WK_XTAL_FREQ_HZ);

    // 配置IRQ GPIO13为输入，上拉由外部硬件保障，下降沿触发中断
    uapi_pin_set_mode(WK_IRQ_PIN, WK_IRQ_PIN_MODE);
    uapi_gpio_set_dir(WK_IRQ_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_register_isr_func(WK_IRQ_PIN, GPIO_INTERRUPT_FALLING_EDGE, wk_irq_callback);
    uapi_gpio_enable_interrupt(WK_IRQ_PIN);

    // 1. 初始化RST引脚及UART2通信线
    WK_RstInit();

    // 2. 波特率自适应匹配
    Wk_BaudAdaptive();
    if (g_wk_main_uart_matched == 0U) {
        osal_printk("[wk2114] init abort: main uart auto-baud not matched\r\n");
        return ERRCODE_FAIL;
    }

    // 3. 验证通信是否正常
    osal_printk("[wk2114] verify GENA=0x%02x\r\n", g_wk_last_gena);

    // 4. 初始化子串口1 并设置波特率为115200
    Wk_Init(1);
    Wk_SetBaud(1, B115200);

    osal_printk("[wk2114] initialisation complete.\r\n");
    return ERRCODE_SUCC;
}

#include "app_init.h"

/**
 * @brief WK2114 业务任务
 */
static void *wk2114_task_func(const char *arg)
{
    (void)arg;
    // 延迟等待OS及外设全面就绪
    osal_msleep(1000);
    mine_wk2114_uart2_ext_init();

    while (1) {
        osal_msleep(1000);
    }
    return NULL;
}

/**
 * @brief 应用启动入口
 */
static void mine_wk2114_app_entry(void)
{
    osal_task *task_handle;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)wk2114_task_func,
                                      0, "wk2114_task", 2048);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, 26);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

// 注册到应用启动运行列表中
app_run(mine_wk2114_app_entry);
