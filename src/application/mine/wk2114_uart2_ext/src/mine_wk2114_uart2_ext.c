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

#define MINE_UART_RX_BUFFER_SIZE 256
static uint8_t g_wk2114_uart_rx_buf[MINE_UART_RX_BUFFER_SIZE];

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
    osal_printk("[wk2114] Wk_BaudAdaptive start, pulling RST high 10ms...\r\n");
    // 初始化拉高RST引脚10ms
    uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_HIGH);
    osal_msleep(10); 
    
    osal_printk("[wk2114] pulling RST low 10ms for reset...\r\n");
    // 然后拉低RST引脚10ms，复位WK芯片
    uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_LOW);
    osal_msleep(10); 
    
    osal_printk("[wk2114] pulling RST high...\r\n");
    // 再拉高RST引脚，完成复位，wk进入正常工作状态
    uapi_gpio_set_val(WK_RST_PIN, GPIO_LEVEL_HIGH); 
    // 延时20ms
    osal_msleep(20);

    osal_printk("[wk2114] sending 0x55 for baud match...\r\n");
    // 发送0x55
    uart_sendByte(0x55); 
    // 并延迟100ms
    osal_msleep(100); 
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
    uint8_t baud1, baud0, pres; 

    /* 基于外部时钟 11.0592MHz 计算: 
       整减1写入BAUD1,BAUD0，小数乘16写入PRES。
       11.0592MHz 是串口通信的完美晶振，算出来的分频系数均为整数，没有小数误差。
    */
    switch(baud) { 
        case B9600:
            baud1 = 0x00;
            baud0 = 0x47; // 11.0592M / (16*9600) = 72 -> (72-1)=71(0x47)
            pres = 0x00;  // 0小数部分
            break;
        case B115200:
            baud1 = 0x00;
            baud0 = 0x05; // 11.0592M / (16*115200) = 6 -> (6-1)=5(0x05)
            pres = 0x00;  // 0小数部分
            break;
        default: 
            baud1 = 0x00;
            baud0 = 0x47; // 默认9600
            pres = 0x00;
            break; 
    }

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
    osal_printk("[wk2114] mine_wk2114_uart2_ext_init start.\r\n");

    // 配置IRQ GPIO13为输入，上拉由外部硬件保障，下降沿触发中断
    uapi_pin_set_mode(WK_IRQ_PIN, WK_IRQ_PIN_MODE);
    uapi_gpio_set_dir(WK_IRQ_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_register_isr_func(WK_IRQ_PIN, GPIO_INTERRUPT_FALLING_EDGE, wk_irq_callback);
    uapi_gpio_enable_interrupt(WK_IRQ_PIN);

    // 1. 初始化RST引脚及UART2通信线
    WK_RstInit();

    // 2. 波特率自适应匹配
    Wk_BaudAdaptive();

    // 3. 验证通信是否正常
    uint8_t gena = WkReadGReg(WK2XXX_GENA);
    osal_printk("[wk2114] verify GENA=0x%x (expect non-zero on success or 0 if uninitialized).\r\n", gena);

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
