/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: UART2 物理短接回环测试实现（TX<->RX）。
 */

#include "mine_uart2_loopback_test.h"

#include <stdarg.h>
#include <stdbool.h>

#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"

#define MINE_UART2_TEST_UART_BUS UART_BUS_2
#define MINE_UART2_TEST_TX_PIN 8
#define MINE_UART2_TEST_RX_PIN 7
#define MINE_UART2_TEST_PIN_MODE 2
#define MINE_UART2_TEST_ALT_PIN_MODE 1

#define MINE_UART2_TEST_DEFAULT_BAUD 115200U
#define MINE_UART2_TEST_DEFAULT_ROUNDS 10U
#define MINE_UART2_TEST_DEFAULT_TIMEOUT_MS 100U
#define MINE_UART2_TEST_PAYLOAD_LEN 16U
#define MINE_UART2_TEST_RX_BUF_SIZE 128U

/**
 * @brief 回环测试日志接口。
 */
static void mine_uart2_test_log(const char *fmt, ...)
{
    char log_buf[160] = {0};
    va_list args;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    if (vsnprintf_s(log_buf, sizeof(log_buf), sizeof(log_buf) - 1, fmt, args) <= 0) {
        va_end(args);
        return;
    }
    va_end(args);

    osal_printk("%s", log_buf);
}

/**
 * @brief 初始化 UART2（单模式）。
 */
static errcode_t mine_uart2_loopback_uart_init(uint32_t baud, uint8_t pin_mode, uint8_t *rx_buf, uint16_t rx_buf_len)
{
    uart_attr_t attr = {
        .baud_rate = baud,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE,
    };
    uart_pin_config_t pin_cfg = {
        .tx_pin = MINE_UART2_TEST_TX_PIN,
        .rx_pin = MINE_UART2_TEST_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE,
    };
    uart_buffer_config_t buf_cfg = {
        .rx_buffer = rx_buf,
        .rx_buffer_size = rx_buf_len,
    };

    (void)uapi_uart_deinit(MINE_UART2_TEST_UART_BUS);
    (void)uapi_pin_set_pull(pin_cfg.rx_pin, PIN_PULL_TYPE_UP);
    (void)uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
    (void)uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

    return uapi_uart_init(MINE_UART2_TEST_UART_BUS, &pin_cfg, &attr, NULL, &buf_cfg);
}

/**
 * @brief 清理 RX FIFO 中的残留数据。
 */
static void mine_uart2_loopback_drain_rx(void)
{
    uint8_t byte = 0;

    while (uapi_uart_read(MINE_UART2_TEST_UART_BUS, &byte, 1, 0) > 0) {
        ;
    }
}

/**
 * @brief 执行单轮收发校验。
 */
static errcode_t mine_uart2_loopback_one_round(const uint8_t *tx, uint16_t len, uint32_t timeout_ms)
{
    uint8_t rx[MINE_UART2_TEST_PAYLOAD_LEN] = {0};
    uint16_t got = 0;
    uint32_t start_ms;
    int32_t ret;

    ret = uapi_uart_write(MINE_UART2_TEST_UART_BUS, tx, len, 0);
    if (ret != (int32_t)len) {
        mine_uart2_test_log("[uart2 loopback] write failed ret=%ld len=%u\r\n", (long)ret, (unsigned int)len);
        return ERRCODE_FAIL;
    }

    start_ms = (uint32_t)uapi_systick_get_ms();
    while (((uint32_t)(uapi_systick_get_ms() - start_ms) < timeout_ms) && (got < len)) {
        ret = uapi_uart_read(MINE_UART2_TEST_UART_BUS, &rx[got], (uint16_t)(len - got), 0);
        if (ret > 0) {
            got = (uint16_t)(got + (uint16_t)ret);
            continue;
        }
        osal_msleep(1);
    }

    if (got != len) {
        mine_uart2_test_log("[uart2 loopback] timeout got=%u expect=%u\r\n", (unsigned int)got, (unsigned int)len);
        return ERRCODE_FAIL;
    }

    if (memcmp(tx, rx, len) != 0) {
        mine_uart2_test_log("[uart2 loopback] compare failed\r\n");
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

errcode_t mine_uart2_loopback_test_run(uint32_t baud_rate, uint16_t rounds, uint32_t timeout_ms)
{
    static uint8_t rx_buf[MINE_UART2_TEST_RX_BUF_SIZE] = {0};
    uint8_t tx[MINE_UART2_TEST_PAYLOAD_LEN] = {
        0x55U, 0xAAU, 0x5AU, 0xA5U, 0x11U, 0x22U, 0x33U, 0x44U,
        0x77U, 0x88U, 0x99U, 0x00U, 0x0FU, 0xF0U, 0xC3U, 0x3CU,
    };
    uint8_t mode_try[2] = {MINE_UART2_TEST_PIN_MODE, MINE_UART2_TEST_ALT_PIN_MODE};
    uint16_t i;
    uint8_t mode_idx;
    errcode_t ret = ERRCODE_FAIL;

    if (baud_rate == 0U) {
        baud_rate = MINE_UART2_TEST_DEFAULT_BAUD;
    }
    if (rounds == 0U) {
        rounds = MINE_UART2_TEST_DEFAULT_ROUNDS;
    }
    if (timeout_ms == 0U) {
        timeout_ms = MINE_UART2_TEST_DEFAULT_TIMEOUT_MS;
    }

    mine_uart2_test_log("[uart2 loopback] start baud=%lu rounds=%u timeout=%lu\r\n",
        (unsigned long)baud_rate, (unsigned int)rounds, (unsigned long)timeout_ms);

    /*
     * 关键流程注释：优先用常规 pin mode，失败后自动回退到备用 mode，
     * 兼容不同板级复用配置。
     */
    for (mode_idx = 0; mode_idx < sizeof(mode_try); mode_idx++) {
        ret = mine_uart2_loopback_uart_init(baud_rate, mode_try[mode_idx], rx_buf, sizeof(rx_buf));
        if (ret != ERRCODE_SUCC) {
            mine_uart2_test_log("[uart2 loopback] init mode=%u failed ret=%x\r\n",
                (unsigned int)mode_try[mode_idx], ret);
            continue;
        }

        mine_uart2_loopback_drain_rx();
        ret = ERRCODE_SUCC;
        for (i = 0; i < rounds; i++) {
            tx[0] = (uint8_t)(0x55U + (uint8_t)i);
            tx[1] = (uint8_t)(0xAAU - (uint8_t)i);
            ret = mine_uart2_loopback_one_round(tx, MINE_UART2_TEST_PAYLOAD_LEN, timeout_ms);
            if (ret != ERRCODE_SUCC) {
                mine_uart2_test_log("[uart2 loopback] round %u failed\r\n", (unsigned int)(i + 1U));
                break;
            }
        }

        (void)uapi_uart_deinit(MINE_UART2_TEST_UART_BUS);
        if (ret == ERRCODE_SUCC) {
            mine_uart2_test_log("[uart2 loopback] pass mode=%u\r\n", (unsigned int)mode_try[mode_idx]);
            return ERRCODE_SUCC;
        }
    }

    mine_uart2_test_log("[uart2 loopback] failed in all modes\r\n");
    return ERRCODE_FAIL;
}

errcode_t mine_uart2_loopback_test_default(void)
{
    return mine_uart2_loopback_test_run(0U, 0U, 0U);
}
