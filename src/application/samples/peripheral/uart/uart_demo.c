/**
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023-2023. All rights reserved.
 *
 * Description: UART2 TX fixed-pattern test sample. \n
 *
 * History: \n
 * 2023-06-29, Create file. \n
 * 2026-03-28, Rework for UART2 TX 0x55 continuous test. \n
 */
#include "pinctrl.h"
#include "uart.h"
#include "soc_osal.h"
#include "app_init.h"

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
