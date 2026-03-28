/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: UART Sample Source. \n
 *
 * History: \n
 * 2023-06-29, Create file. \n
 */
#include "pinctrl.h"
#include "uart.h"
#include "watchdog.h"
#include "soc_osal.h"
#include "app_init.h"
#if defined(CONFIG_UART_SUPPORT_DMA)
#include "dma.h"
#include "hal_dma.h"
#endif

#define UART_BAUDRATE                      115200
#define CONFIG_UART_INT_WAIT_MS            5

#define UART_TASK_PRIO                     24
#define UART_TASK_STACK_SIZE               0x1000

static uint8_t g_app_uart_rx_buff[CONFIG_UART_TRANSFER_SIZE] = { 0 };
#if defined(CONFIG_UART_SUPPORT_INT_MODE)
static uint8_t g_app_uart_int_rx_flag = 0;
static volatile uint8_t g_app_uart_int_index = 0;
static uint8_t g_app_uart_int_rx_buff[CONFIG_UART_TRANSFER_SIZE] = { 0 };
#endif
static uart_buffer_config_t g_app_uart_buffer_config = {
    .rx_buffer = g_app_uart_rx_buff,
    .rx_buffer_size = CONFIG_UART_TRANSFER_SIZE
};

#if defined(CONFIG_UART_SUPPORT_DMA)
uart_write_dma_config_t g_app_dma_cfg = {
    .src_width = HAL_DMA_TRANSFER_WIDTH_8,
    .dest_width = HAL_DMA_TRANSFER_WIDTH_8,
    .burst_length = HAL_DMA_BURST_TRANSACTION_LENGTH_1,
    .priority = HAL_DMA_CH_PRIORITY_0
};
#endif

static void app_uart_init_pin(void)
{
#if defined(CONFIG_PINCTRL_SUPPORT_IE)
    uapi_pin_set_ie(CONFIG_UART_RXD_PIN, PIN_IE_1);
#endif /* CONFIG_PINCTRL_SUPPORT_IE */
    uapi_pin_set_mode(CONFIG_UART_TXD_PIN, CONFIG_UART_TXD_PIN_MODE);
    uapi_pin_set_mode(CONFIG_UART_RXD_PIN, CONFIG_UART_RXD_PIN_MODE);
}

static void app_uart_init_config(void)
{
    uart_attr_t attr = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    uart_pin_config_t pin_config = {
        .tx_pin = CONFIG_UART_TXD_PIN,
        .rx_pin = CONFIG_UART_RXD_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };

#if defined(CONFIG_UART_SUPPORT_DMA)
    uart_extra_attr_t extra_attr = {
        .tx_dma_enable = true,
        .tx_int_threshold = UART_FIFO_INT_TX_LEVEL_EQ_0_CHARACTER,
        .rx_dma_enable = true,
        .rx_int_threshold = UART_FIFO_INT_RX_LEVEL_1_CHARACTER
    };
    uapi_dma_init();
    uapi_dma_open();
    uapi_uart_deinit(CONFIG_UART_BUS_ID);
    uapi_uart_init(CONFIG_UART_BUS_ID, &pin_config, &attr, &extra_attr, &g_app_uart_buffer_config);
#else
    uapi_uart_deinit(CONFIG_UART_BUS_ID);
    uapi_uart_init(CONFIG_UART_BUS_ID, &pin_config, &attr, NULL, &g_app_uart_buffer_config);
#endif
}

#if defined(CONFIG_UART_SUPPORT_INT_MODE)
static void app_uart_read_int_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);
    if (buffer == NULL || length == 0) {
        osal_printk("uart%d int mode transfer illegal data!\r\n", CONFIG_UART_BUS_ID);
        return;
    }

    uint8_t *buff = (uint8_t *)buffer;
    if (memcpy_s(g_app_uart_rx_buff, length, buff, length) != EOK) {
        osal_printk("uart%d int mode data copy fail!\r\n", CONFIG_UART_BUS_ID);
        return;
    }
    if (memcpy_s(g_app_uart_int_rx_buff + g_app_uart_int_index, length, g_app_uart_rx_buff, length) != EOK) {
        g_app_uart_int_index = 0;
        osal_printk("uart%d int mode data2 copy fail!\r\n", CONFIG_UART_BUS_ID);
    }
    g_app_uart_int_index += length;
    g_app_uart_int_rx_flag = 1;
}

static void app_uart_write_int_handler(const void *buffer, uint32_t length, const void *params)
{
    unused(params);
    uint8_t *buff = (void *)buffer;
    for (uint8_t i = 0; i < length; i++) {
        osal_printk("uart%d write data[%d] = %d\r\n", CONFIG_UART_BUS_ID, i, buff[i]);
    }
}

static void app_uart_register_rx_callback(void)
{
    osal_printk("uart%d int mode register receive callback start!\r\n", CONFIG_UART_BUS_ID);
    if (uapi_uart_register_rx_callback(CONFIG_UART_BUS_ID, UART_RX_CONDITION_FULL_OR_SUFFICIENT_DATA_OR_IDLE,
                                       1, app_uart_read_int_handler) == ERRCODE_SUCC) {
        osal_printk("uart%d int mode register receive callback succ!\r\n", CONFIG_UART_BUS_ID);
    }
}
#endif

static void *uart_task(const char *arg)
{
    unused(arg);
#if defined(CONFIG_UART_SUPPORT_DMA)
    int32_t ret = CONFIG_UART_TRANSFER_SIZE;
#if defined(CONFIG_UART_USING_V151)
    ret = ERRCODE_SUCC;
#endif
#endif
    /* UART pinmux. */
    app_uart_init_pin();

    /* UART init config. */
    app_uart_init_config();

#if defined(CONFIG_UART_SUPPORT_INT_MODE)
    app_uart_register_rx_callback();
#endif

    while (1) {
#if defined(CONFIG_UART_SUPPORT_INT_MODE)
        while (g_app_uart_int_rx_flag != 1) { osal_msleep(CONFIG_UART_INT_WAIT_MS); }
        g_app_uart_int_rx_flag = 0;
        osal_printk("uart%d int mode send back!\r\n", CONFIG_UART_BUS_ID);
        if (uapi_uart_write_int(CONFIG_UART_BUS_ID, g_app_uart_int_rx_buff, CONFIG_UART_TRANSFER_SIZE, 0,
                                app_uart_write_int_handler) == ERRCODE_SUCC) {
         * Description: UART2 TX fixed-pattern test sample. \n
        }
#elif defined(CONFIG_UART_SUPPORT_DMA)
        osal_printk("uart%d dma mode receive start!\r\n", CONFIG_UART_BUS_ID);
        if (uapi_uart_read_by_dma(CONFIG_UART_BUS_ID, g_app_uart_rx_buff, CONFIG_UART_TRANSFER_SIZE,
            &g_app_dma_cfg) == ret) {
        }
        osal_printk("uart%d dma mode send back!\r\n", CONFIG_UART_BUS_ID);

        #define UART2_TEST_BAUDRATE               115200
        #define UART2_TEST_TASK_PRIO              24
        #define UART2_TEST_TASK_STACK_SIZE        0x1000
        #define UART2_TEST_SEND_INTERVAL_MS       100

        /*
         * UART2 默认引脚配置：
         * TX = GPIO8, RX = GPIO7, mode = 2。
         * 若你的板级复用不同，请按实际硬件修改这3个宏。
         */
        #define UART2_TEST_BUS                    UART_BUS_2
        #define UART2_TEST_TX_PIN                 8
        #define UART2_TEST_RX_PIN                 7
        #define UART2_TEST_PIN_MODE               2

        /* UART 发送固定测试字节：01010101b，便于示波器观测占空比和位宽。 */
        #define UART2_TEST_FIXED_BYTE             0x55

        /*
         * 即使只测试TX，驱动参数校验仍要求提供合法RX引脚与接收缓冲。
         * 这里使用最小1字节缓冲满足初始化条件。
         */
        static uint8_t g_uart2_test_rx_buffer[1] = { 0 };

        /**
         * @brief 配置UART2引脚复用。
         */
        static void uart2_test_init_pinmux(void)
        {
        #if defined(CONFIG_PINCTRL_SUPPORT_IE)
            (void)uapi_pin_set_ie(UART2_TEST_RX_PIN, PIN_IE_1);
        #endif /* CONFIG_PINCTRL_SUPPORT_IE */
            (void)uapi_pin_set_mode(UART2_TEST_TX_PIN, UART2_TEST_PIN_MODE);
            (void)uapi_pin_set_mode(UART2_TEST_RX_PIN, UART2_TEST_PIN_MODE);
        }

        /**
         * @brief 初始化UART2为8N1/115200，供固定字节发送测试。
         *
         * @return errcode_t
         * @retval ERRCODE_SUCC 初始化成功。
         * @retval 其他值 初始化失败。
         */
        static errcode_t uart2_test_init(void)
        {
            uart_attr_t attr = {
                .baud_rate = UART2_TEST_BAUDRATE,
                .data_bits = UART_DATA_BIT_8,
                .stop_bits = UART_STOP_BIT_1,
                .parity = UART_PARITY_NONE
            };

            uart_pin_config_t pin_config = {
                .tx_pin = UART2_TEST_TX_PIN,
                .rx_pin = UART2_TEST_RX_PIN,
                .cts_pin = PIN_NONE,
                .rts_pin = PIN_NONE
            };

            uart_buffer_config_t rx_buffer_config = {
                .rx_buffer = g_uart2_test_rx_buffer,
                .rx_buffer_size = sizeof(g_uart2_test_rx_buffer)
            };

            (void)uapi_uart_deinit(UART2_TEST_BUS);
            return uapi_uart_init(UART2_TEST_BUS, &pin_config, &attr, NULL, &rx_buffer_config);
        }

        /**
         * @brief UART2测试线程：持续向TX输出0x55。
         *
         * @param arg 线程参数，未使用。
         * @return void* 固定返回NULL。
         */
        static void *uart2_test_task(const char *arg)
        {
            unused(arg);
            uint8_t tx_data = UART2_TEST_FIXED_BYTE;
            uint32_t send_count = 0;

            uart2_test_init_pinmux();
            if (uart2_test_init() != ERRCODE_SUCC) {
                osal_printk("uart2 tx55 init failed!\r\n");
                return NULL;
            }

            osal_printk("uart2 tx55 test start: bus=%d tx_pin=%d baud=%d\r\n",
                        UART2_TEST_BUS, UART2_TEST_TX_PIN, UART2_TEST_BAUDRATE);

            while (1) {
                int32_t ret = uapi_uart_write(UART2_TEST_BUS, &tx_data, 1, 0);
                if (ret == 1) {
                    send_count++;
                    /* 每发送1024字节打印一次，避免日志过多影响串口时序。 */
                    if ((send_count & 0x3FFU) == 0) {
                        osal_printk("uart2 tx55 sent %u bytes\r\n", send_count);
                    }
                } else {
                    /* 发送失败时打印错误并稍作退避，避免持续忙等占满CPU。 */
                    osal_printk("uart2 tx55 write failed, ret=%d\r\n", ret);
                    osal_msleep(1000);
                }
                osal_msleep(UART2_TEST_SEND_INTERVAL_MS);
            }
        }

        /**
         * @brief UART2测试入口，创建发送线程。
         */
        static void uart2_test_entry(void)
        {
            osal_task *task_handle = NULL;

            osal_kthread_lock();
            task_handle = osal_kthread_create((osal_kthread_handler)uart2_test_task, 0,
                                              "Uart2Tx55Task", UART2_TEST_TASK_STACK_SIZE);
            if (task_handle != NULL) {
                osal_kthread_set_priority(task_handle, UART2_TEST_TASK_PRIO);
            }
            osal_kthread_unlock();
        }

        /* Run the uart2_test_entry. */
        app_run(uart2_test_entry);