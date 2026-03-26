/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd.
 * 描述: Mine WK2114 UART2 扩展模块主流程与协议发送实现。
 */

#include "mine_wk2114_uart2_ext_module.h"

#include <stdbool.h>
#include <stdarg.h>

#include "app_init.h"
#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"
#include "securec.h"
#include "soc_osal.h"
#include "systick.h"
#include "uart.h"

#ifndef UART_RX_CONDITION_MASK_IDLE
#define UART_RX_CONDITION_MASK_IDLE 1
#endif

#ifndef UART_RX_CONDITION_MASK_SUFFICIENT_DATA
#define UART_RX_CONDITION_MASK_SUFFICIENT_DATA 2
#endif

#ifndef UART_RX_CONDITION_MASK_FULL
#define UART_RX_CONDITION_MASK_FULL 4
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
static uint8_t g_mine_wk2114_uart_profile_index = 0;
static uint32_t g_mine_wk2114_subuart_baud[MINE_WK2114_SUBUART_COUNT] = {
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
    MINE_WK2114_HOST_UART_BAUD,
};
static bool g_mine_wk2114_subuart_ready[MINE_WK2114_SUBUART_COUNT] = { false, false, false, false };

/* GENA 低 4 位用于 UT1~UT4 使能，保留位按手册保持为 1。 */
static uint8_t g_mine_wk2114_gena_shadow = MINE_WK2114_GENA_RESERVED_MASK;

/* UART2 活性诊断统计：用于确认主口是否真实发生收发。 */
typedef struct {
    uint32_t tx_frame_count;
    uint32_t tx_byte_count;
    uint32_t tx_fail_count;
    uint32_t rx_callback_byte_count;
    uint32_t rx_poll_byte_count;
    uint32_t rx_drain_byte_count;
    uint32_t read_cmd_count;
    uint32_t read_timeout_count;
} mine_wk2114_uart_diag_t;

static mine_wk2114_uart_diag_t g_mine_wk2114_uart_diag = {0};

/**
 * @brief OLED 已按需求移除，这里保留空实现以兼容原流程调用。
 */
static void mine_wk2114_oled_init(void)
{
}

/**
 * @brief OLED 已移除：空实现。
 */
static void mine_wk2114_oled_flush_pending(void)
{
}

/**
 * @brief OLED 已移除：空实现。
 *
 * @param text 未使用参数。
 */
static void mine_wk2114_oled_push_state(const char *text)
{
    unused(text);
}

/**
 * @brief OLED 已移除：空实现。
 *
 * @param prefix 未使用参数。
 * @param data   未使用参数。
 * @param len    未使用参数。
 */
static void mine_wk2114_oled_push_data_event(const char *prefix, const uint8_t *data, uint16_t len)
{
    unused(prefix);
    unused(data);
    unused(len);
}

/**
 * @brief OLED 已移除：空实现。
 *
 * @param channel   未使用参数。
 * @param baud_rate 未使用参数。
 */
static void mine_wk2114_oled_set_channel(uint8_t channel, uint32_t baud_rate)
{
    unused(channel);
    unused(baud_rate);
}

/* WK2114 IRQ 事件统计：中断回调置位，任务线程消费并记录。 */
static volatile bool g_mine_wk2114_irq_pending = false;
static volatile uint32_t g_mine_wk2114_irq_trigger_count = 0;
static uint32_t g_mine_wk2114_irq_handle_count = 0;

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
 * @brief 记录 UART2 发送成功统计。
 *
 * @param byte_count 成功发送字节数。
 */
static void mine_wk2114_uart_diag_record_tx_success(uint32_t byte_count)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.tx_frame_count++;
    g_mine_wk2114_uart_diag.tx_byte_count += byte_count;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 记录 UART2 发送失败统计。
 */
static void mine_wk2114_uart_diag_record_tx_fail(void)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.tx_fail_count++;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 记录 RX 回调路径接收字节数。
 *
 * @param byte_count 回调接收字节数。
 */
static void mine_wk2114_uart_diag_record_rx_callback(uint32_t byte_count)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.rx_callback_byte_count += byte_count;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 记录轮询兜底路径接收字节数。
 *
 * @param byte_count 轮询接收字节数。
 */
static void mine_wk2114_uart_diag_record_rx_poll(uint32_t byte_count)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.rx_poll_byte_count += byte_count;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 记录清理历史 RX 字节统计。
 *
 * @param byte_count 清理字节数。
 */
static void mine_wk2114_uart_diag_record_rx_drain(uint32_t byte_count)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.rx_drain_byte_count += byte_count;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 记录一次读寄存器请求。
 */
static void mine_wk2114_uart_diag_record_read_cmd(void)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.read_cmd_count++;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 记录一次读寄存器超时。
 */
static void mine_wk2114_uart_diag_record_read_timeout(void)
{
    unsigned int irq_status = mine_wk2114_irq_lock();

    g_mine_wk2114_uart_diag.read_timeout_count++;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 读取 UART2 活性诊断快照。
 *
 * @param snapshot 输出统计快照。
 */
static void mine_wk2114_uart_diag_snapshot(mine_wk2114_uart_diag_t *snapshot)
{
    unsigned int irq_status;

    if (snapshot == NULL) {
        return;
    }

    irq_status = mine_wk2114_irq_lock();
    *snapshot = g_mine_wk2114_uart_diag;
    mine_wk2114_irq_unlock(irq_status);
}

/**
 * @brief 周期打印 UART2 收发活性统计。
 *
 * 该日志用于快速确认：
 * - 发送路径是否持续有数据输出；
 * - 接收路径是否通过回调或轮询获取到数据；
 * - 读寄存器超时是否持续增长。
 */
static void mine_wk2114_uart_diag_report_periodic(void)
{
    static uint32_t last_report_ms = 0;
    static uint32_t tx_without_rx_windows = 0;
    static mine_wk2114_uart_diag_t last_snapshot = {0};
    mine_wk2114_uart_diag_t cur_snapshot = {0};
    uint32_t now_ms;
    uint32_t delta_tx;
    uint32_t delta_rx;
    uint32_t delta_timeout;

    now_ms = (uint32_t)uapi_systick_get_ms();
    if ((uint32_t)(now_ms - last_report_ms) < MINE_WK2114_UART_DIAG_REPORT_MS) {
        return;
    }
    last_report_ms = now_ms;

    mine_wk2114_uart_diag_snapshot(&cur_snapshot);
    delta_tx = cur_snapshot.tx_byte_count - last_snapshot.tx_byte_count;
    delta_rx = (cur_snapshot.rx_callback_byte_count + cur_snapshot.rx_poll_byte_count) -
        (last_snapshot.rx_callback_byte_count + last_snapshot.rx_poll_byte_count);
    delta_timeout = cur_snapshot.read_timeout_count - last_snapshot.read_timeout_count;

    mine_wk2114_log("[mine wk2114] uart2 diag total(tx=%luB/%luF fail=%lu rx_cb=%lu rx_poll=%lu drain=%lu rreg=%lu rto=%lu) delta(tx=%lu rx=%lu rto=%lu)\r\n",
        (unsigned long)cur_snapshot.tx_byte_count,
        (unsigned long)cur_snapshot.tx_frame_count,
        (unsigned long)cur_snapshot.tx_fail_count,
        (unsigned long)cur_snapshot.rx_callback_byte_count,
        (unsigned long)cur_snapshot.rx_poll_byte_count,
        (unsigned long)cur_snapshot.rx_drain_byte_count,
        (unsigned long)cur_snapshot.read_cmd_count,
        (unsigned long)cur_snapshot.read_timeout_count,
        (unsigned long)delta_tx,
        (unsigned long)delta_rx,
        (unsigned long)delta_timeout);

    if ((delta_tx > 0U) && (delta_rx == 0U)) {
        tx_without_rx_windows++;
    } else {
        tx_without_rx_windows = 0;
    }

    if (tx_without_rx_windows >= 3U) {
        mine_wk2114_log("[mine wk2114] uart2 warn: tx active but rx silent (%lu windows)\r\n",
            (unsigned long)tx_without_rx_windows);
    }

    last_snapshot = cur_snapshot;
}

/**
 * @brief WK2114 IRQ 中断回调，仅置位标记并累计次数。
 *
 * @param pin   触发中断的 GPIO 引脚。
 * @param param 回调预留参数。
 */
static void mine_wk2114_irq_gpio_handler(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);

    g_mine_wk2114_irq_trigger_count++;
    g_mine_wk2114_irq_pending = true;
}

/**
 * @brief 初始化 WK2114 IRQ 引脚（GPIO13，上拉输入，低电平触发中断）。
 *
 * @return errcode_t
 */
static errcode_t mine_wk2114_irq_gpio_init(void)
{
    errcode_t ret;

    uapi_gpio_init();
    (void)uapi_pin_set_mode(MINE_WK2114_IRQ_GPIO_PIN, MINE_WK2114_IRQ_PIN_MODE);
    (void)uapi_pin_set_pull(MINE_WK2114_IRQ_GPIO_PIN, PIN_PULL_TYPE_UP);

    ret = uapi_gpio_set_dir(MINE_WK2114_IRQ_GPIO_PIN, GPIO_DIRECTION_INPUT);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] irq pin dir failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK IRQ DIR FAIL");
        return ret;
    }

    /* 重复初始化时先清旧回调，避免注册冲突。 */
    (void)uapi_gpio_unregister_isr_func(MINE_WK2114_IRQ_GPIO_PIN);
    ret = uapi_gpio_register_isr_func(MINE_WK2114_IRQ_GPIO_PIN,
        MINE_WK2114_IRQ_EDGE_MODE,
        mine_wk2114_irq_gpio_handler);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] irq register failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK IRQ REG FAIL");
        return ret;
    }

    ret = uapi_gpio_enable_interrupt(MINE_WK2114_IRQ_GPIO_PIN);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] irq enable failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK IRQ EN FAIL");
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 在任务线程处理 IRQ 事件，输出状态与计数。
 */
static void mine_wk2114_process_irq_event(void)
{
    bool irq_pending;
    uint32_t irq_total;
    gpio_level_t irq_level;
    unsigned int irq_status;

    irq_status = mine_wk2114_irq_lock();
    irq_pending = g_mine_wk2114_irq_pending;
    irq_total = g_mine_wk2114_irq_trigger_count;
    g_mine_wk2114_irq_pending = false;
    mine_wk2114_irq_unlock(irq_status);

    if (!irq_pending) {
        return;
    }

    g_mine_wk2114_irq_handle_count++;
    irq_level = uapi_gpio_get_val(MINE_WK2114_IRQ_GPIO_PIN);

    /* IRQ 为低电平有效：进入处理时若仍为低，说明设备仍有待处理中断源。 */
    if (irq_level == GPIO_LEVEL_LOW) {
        mine_wk2114_oled_push_state("WK IRQ ACTIVE");
    } else {
        mine_wk2114_oled_push_state("WK IRQ EDGE");
    }

    mine_wk2114_log("[mine wk2114] irq evt total=%lu handled=%lu level=%u\r\n",
        (unsigned long)irq_total,
        (unsigned long)g_mine_wk2114_irq_handle_count,
        (unsigned int)irq_level);
}

/**
 * @brief 对 WK2114 执行硬件复位脉冲（GPIO10）。
 *
 * 复位流程：
 * 1) GPIO10 先拉高并保持 10ms；
 * 2) GPIO10 拉低并保持 10ms，触发芯片复位；
 * 3) GPIO10 拉高并保持 20ms，完成复位释放。
 *
 * @return errcode_t
 */
static errcode_t mine_wk2114_hw_reset_chip(void)
{
    errcode_t ret;

    uapi_gpio_init();
    (void)uapi_pin_set_mode(MINE_WK2114_RESET_GPIO_PIN, HAL_PIO_FUNC_GPIO);
    (void)uapi_pin_set_pull(MINE_WK2114_RESET_GPIO_PIN, PIN_PULL_TYPE_UP);

    ret = uapi_gpio_set_dir(MINE_WK2114_RESET_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pin dir failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK RST DIR FAIL");
        return ret;
    }

    /* 按手册示例：先拉高 10ms，再拉低 10ms，最后拉高 20ms。 */
    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pin pre-high failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK RST PREHI FAIL");
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_HOLD_MS);

    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pin pull low failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK RST LOW FAIL");
        return ret;
    }
    osal_msleep(MINE_WK2114_RESET_HOLD_MS);

#if (MINE_WK2114_RESET_FORCE_LOW_ONLY == 1U)
    /* 调试期间保持 RESET 低电平，供外部仪器确认复位脚状态。 */
    mine_wk2114_log("[mine wk2114] reset pin forced low for debug\r\n");
    mine_wk2114_oled_push_state("WK RST HOLD LO");
    return ERRCODE_SUCC;
#endif

    ret = uapi_gpio_set_val(MINE_WK2114_RESET_GPIO_PIN, GPIO_LEVEL_HIGH);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] reset pin release failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK RST HI FAIL");
        return ret;
    }

    osal_msleep(MINE_WK2114_RESET_RELEASE_WAIT_MS);
    mine_wk2114_log("[mine wk2114] hw reset pulse done on gpio10\r\n");
    return ERRCODE_SUCC;
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
 * @brief 清理 UART2 硬件 RX FIFO 的历史字节，避免旧数据干扰当前命令回读。
 *
 * @note
 * 仅在发送新命令前调用；若无数据可读会立即退出，不阻塞主流程。
 */
static void mine_wk2114_host_hw_rx_drain(void)
{
    uint8_t drain_byte = 0;
    uint16_t drain_count = 0;

    while (drain_count < MINE_WK2114_HOST_RESP_FIFO_SIZE) {
        if (uapi_uart_read(MINE_WK2114_HOST_UART_BUS, &drain_byte, 1, 0) <= 0) {
            break;
        }
        drain_count++;
    }

    if (drain_count > 0U) {
        mine_wk2114_uart_diag_record_rx_drain((uint32_t)drain_count);
        mine_wk2114_log("[mine wk2114] drain stale host rx=%u\r\n", (unsigned int)drain_count);
    }
}

/**
 * @brief 获取主口回读字节：优先软件 FIFO，失败后轮询硬件 FIFO 兜底。
 *
 * @param byte 输出字节。
 * @return true  获取成功。
 * @return false 暂无数据。
 */
static bool mine_wk2114_host_resp_try_fetch(uint8_t *byte)
{
    if (byte == NULL) {
        return false;
    }

    if (mine_wk2114_host_resp_fifo_pop(byte)) {
        return true;
    }

    /*
     * 某些场景下中断回调可能延后触发，轮询硬件 FIFO 作为兜底路径。
     * 这样即使回调未及时入队，也能在读寄存器窗口内拿到返回值。
     */
    if (uapi_uart_read(MINE_WK2114_HOST_UART_BUS, byte, 1, 0) > 0) {
        mine_wk2114_uart_diag_record_rx_poll(1U);
        return true;
    }

    return false;
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
    if ((write_ret < 0) || ((uint32_t)write_ret != len)) {
        mine_wk2114_uart_diag_record_tx_fail();
        mine_wk2114_log("[mine wk2114] host tx fail/short, want=%u, ret=%ld\r\n",
            (unsigned int)len, (long)write_ret);
        mine_wk2114_oled_push_state("HOST TX FAIL");
        return ERRCODE_FAIL;
    }

    mine_wk2114_uart_diag_record_tx_success((uint32_t)write_ret);

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
    uint8_t rx_byte = 0;
    uint8_t last_byte = 0;
    errcode_t ret;
    uint32_t start_ms;
    uint32_t stable_wait_ms = 0;
    bool got_data = false;
    bool got_new_data;

    if (value == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    cmd = (uint8_t)(MINE_WK2114_HOST_CMD_READ_REG | (addr6 & 0x3FU));
    mine_wk2114_host_resp_fifo_reset();
    mine_wk2114_host_hw_rx_drain();
    mine_wk2114_uart_diag_record_read_cmd();

    ret = mine_wk2114_send_host_frame(&cmd, 1, "HOST TX RREG");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    start_ms = (uint32_t)uapi_systick_get_ms();
    while ((uint32_t)(uapi_systick_get_ms() - start_ms) <= MINE_WK2114_HOST_READ_TIMEOUT_MS) {
        got_new_data = false;
        while (mine_wk2114_host_resp_try_fetch(&rx_byte)) {
            /*
             * 读取阶段采用“最后一个字节为准”的策略：
             * - 若总线上存在回显，首字节可能是命令字；
             * - 真正的寄存器返回值通常在其后到达。
             */
            got_new_data = true;
            got_data = true;
            last_byte = rx_byte;
            stable_wait_ms = 0;
        }

        if (got_data && (!got_new_data)) {
            stable_wait_ms++;
            if (stable_wait_ms >= MINE_WK2114_HOST_RESP_STABLE_WAIT_MS) {
                *value = last_byte;
                return ERRCODE_SUCC;
            }
        }

        osal_msleep(1);
    }

    if (got_data) {
        *value = last_byte;
        return ERRCODE_SUCC;
    }

    mine_wk2114_uart_diag_record_read_timeout();
    mine_wk2114_oled_push_state("HOST RREG TO");
    return ERRCODE_FAIL;
}

/**
 * @brief 对读寄存器流程执行短重试，过滤启动阶段瞬态失败。
 *
 * @param addr6     6bit 地址。
 * @param retry_max 最大重试次数。
 * @param value     输出读取值。
 * @return errcode_t
 */
static errcode_t mine_wk2114_read_addr6_retry(uint8_t addr6, uint8_t retry_max, uint8_t *value)
{
    uint8_t retry_idx;
    uint8_t temp_value = 0;
    errcode_t ret;

    if ((value == NULL) || (retry_max == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    for (retry_idx = 0; retry_idx < retry_max; retry_idx++) {
        ret = mine_wk2114_read_addr6(addr6, &temp_value);
        if (ret == ERRCODE_SUCC) {
            *value = temp_value;
            return ERRCODE_SUCC;
        }

        osal_msleep(1);
    }

    return ERRCODE_FAIL;
}

/**
 * @brief 读取寄存器并校验固定值，用于主口链路有效性确认。
 *
 * @param addr6    6bit 寄存器地址。
 * @param expected 期望值。
 * @param reg_name 寄存器名称（用于日志）。
 * @return errcode_t
 */
static errcode_t mine_wk2114_verify_register_value(uint8_t addr6, uint8_t expected, const char *reg_name)
{
    uint8_t read_value = 0;
    errcode_t ret;

    ret = mine_wk2114_read_addr6_retry(addr6, MINE_WK2114_LINK_CHECK_READ_RETRY, &read_value);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] verify %s read timeout\r\n", (reg_name == NULL) ? "REG" : reg_name);
        return ret;
    }

    if (read_value != expected) {
        mine_wk2114_log("[mine wk2114] verify %s mismatch, got=0x%02X expect=0x%02X\r\n",
            (reg_name == NULL) ? "REG" : reg_name,
            (unsigned int)read_value,
            (unsigned int)expected);
        return ERRCODE_FAIL;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 写寄存器后立即回读校验，确保写操作真实生效。
 *
 * @param addr6    6bit 寄存器地址。
 * @param value    写入值。
 * @param reg_name 寄存器名称（用于日志）。
 * @return errcode_t
 */
static errcode_t mine_wk2114_write_readback_verify(uint8_t addr6, uint8_t value, const char *reg_name)
{
    errcode_t ret;

    ret = mine_wk2114_write_addr6(addr6, value);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] write %s failed, ret=%x\r\n",
            (reg_name == NULL) ? "REG" : reg_name,
            ret);
        return ret;
    }

    ret = mine_wk2114_verify_register_value(addr6, value, reg_name);
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_log("[mine wk2114] write-readback %s failed\r\n", (reg_name == NULL) ? "REG" : reg_name);
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 执行 WK2114 主口自动波特率锁定序列。
 *
 * 按手册 9.1 的 0x55 机制实现，并增加多次发送以提升冷启动锁定稳定性。
 *
 * @return errcode_t
 */
static errcode_t mine_wk2114_send_autobaud_sync_sequence(void)
{
    uint8_t sync_byte = MINE_WK2114_HOST_AUTOBAUD_SYNC_BYTE;
    uint8_t sync_idx;
    errcode_t ret;

    for (sync_idx = 0; sync_idx < MINE_WK2114_HOST_AUTOBAUD_SYNC_RETRY; sync_idx++) {
        ret = mine_wk2114_send_host_frame(&sync_byte, 1, "HOST TX SYNC");
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        osal_msleep(MINE_WK2114_HOST_AUTOBAUD_SYNC_INTERVAL_MS);
    }

    /* 按手册示例：发 0x55 后等待 100ms 完成主口波特率锁定。 */
    osal_msleep(MINE_WK2114_HOST_AUTOBAUD_LOCK_WAIT_MS);
    return ERRCODE_SUCC;
}

/**
 * @brief 执行 WK2114 上电连通性检查。
 *
 * 检查流程基于规格书：
 * 1) 先执行硬复位（reset 低电平保持 10ms，再拉高）；
 * 2) 发送 0x55 完成主口波特率自适应锁定；
 * 3) 读取 GENA（0x00）并校验默认值 0xF0。
 *
 * 说明：
 * 这里采用“默认值直读校验”而不是“写后读探测”，
 * 目的是完全对齐手册时序，降低现场联调的不确定性。
 *
 * @return errcode_t
 */
static errcode_t mine_wk2114_check_link_ready(void)
{
    uint8_t gena_origin = 0;
    uint8_t gena_test = 0;
    errcode_t ret;

    ret = mine_wk2114_hw_reset_chip();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK RST FAIL");
        return ret;
    }

#if (MINE_WK2114_RESET_FORCE_LOW_ONLY == 1U)
    /*
     * 强制低电平调试模式下仅验证引脚状态，不执行后续串口链路检查，
     * 避免在芯片复位期间持续发送主口命令干扰现场测试。
     */
    mine_wk2114_log("[mine wk2114] force-low mode enabled, skip link check\r\n");
    mine_wk2114_oled_push_state("WK RST HOLD LO");
    return ERRCODE_FAIL;
#endif

    mine_wk2114_host_resp_fifo_reset();
    mine_wk2114_host_hw_rx_drain();

    ret = mine_wk2114_send_autobaud_sync_sequence();
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK SYNC FAIL");
        return ret;
    }

    /* C: 先读固定寄存器默认值，确认主口通信已打通。 */
    ret = mine_wk2114_verify_register_value(MINE_WK2114_ADDR_GENA, MINE_WK2114_GENA_RESERVED_MASK, "GENA");
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] link check read GENA timeout\r\n");
        mine_wk2114_oled_push_state("WK REG FAIL");
        return ERRCODE_FAIL;
    }

    /* D: 再做一次 GENA 写读回校验，确认写寄存器链路正常。 */
    gena_test = (uint8_t)(MINE_WK2114_GENA_RESERVED_MASK | 0x01U);
    ret = mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GENA, gena_test, "GENA");
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK WREG FAIL");
        return ERRCODE_FAIL;
    }

    ret = mine_wk2114_write_readback_verify(MINE_WK2114_ADDR_GENA, MINE_WK2114_GENA_RESERVED_MASK, "GENA");
    if (ret != ERRCODE_SUCC) {
        mine_wk2114_oled_push_state("WK WREG FAIL");
        return ERRCODE_FAIL;
    }

    gena_origin = MINE_WK2114_GENA_RESERVED_MASK;

    /* 影子值按默认复位值初始化，后续 enable 流程仅更新低 4 位。 */
    g_mine_wk2114_gena_shadow = MINE_WK2114_GENA_RESERVED_MASK;
    osal_printk("[mine wk2114] link ok, GENA=0x%02X\r\n", (unsigned int)gena_origin);
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
    /* 保留位始终置 1，仅改变低 4 位通道使能位。 */
    g_mine_wk2114_gena_shadow = (uint8_t)((g_mine_wk2114_gena_shadow | MINE_WK2114_GENA_RESERVED_MASK) |
        (uint8_t)(1U << bit_index));
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

    ret = mine_wk2114_verify_register_value(sub_addr, 0x01U, "SPAGE(P1)");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_BAUD1);
    ret = mine_wk2114_write_addr6(sub_addr, (uint8_t)((baud_reg >> 8) & 0xFFU));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_verify_register_value(sub_addr, (uint8_t)((baud_reg >> 8) & 0xFFU), "BAUD1");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_BAUD0);
    ret = mine_wk2114_write_addr6(sub_addr, (uint8_t)(baud_reg & 0xFFU));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_verify_register_value(sub_addr, (uint8_t)(baud_reg & 0xFFU), "BAUD0");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_PRES);
    ret = mine_wk2114_write_addr6(sub_addr, (uint8_t)(pres & 0x0FU));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_verify_register_value(sub_addr, (uint8_t)(pres & 0x0FU), "PRES");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 2) 切回 PAGE0，开启 RX/TX 与 FIFO。 */
    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SPAGE);
    ret = mine_wk2114_write_addr6(sub_addr, 0x00);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_verify_register_value(sub_addr, 0x00U, "SPAGE(P0)");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_SCR);
    ret = mine_wk2114_write_addr6(sub_addr, 0x03);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_verify_register_value(sub_addr, 0x03U, "SCR");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    sub_addr = mine_wk2114_make_sub_addr(channel, MINE_WK2114_SUBREG_FCR);
    ret = mine_wk2114_write_addr6(sub_addr, 0x0F);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* FCR 的复位位会自动清零，回读校验时仅比较高 2 位触点配置。 */
    ret = mine_wk2114_verify_register_value(sub_addr, 0x0CU, "FCR");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* 3) 使能全局对应通道时钟。 */
    ret = mine_wk2114_enable_global_channel(channel);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mine_wk2114_verify_register_value(MINE_WK2114_ADDR_GENA, g_mine_wk2114_gena_shadow, "GENA");
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

    mine_wk2114_uart_diag_record_rx_callback((uint32_t)length);

    /*
     * 打印回调收到的首/尾字节，确认主口到底收到了什么值。
     * 该日志可用于判断是否只收到了 0x55 同步字节。
     */
    mine_wk2114_log("[mine wk2114] host rx cb len=%u first=0x%02X last=0x%02X\r\n",
        (unsigned int)length,
        (unsigned int)data_ptr[0],
        (unsigned int)data_ptr[length - 1U]);

    mine_wk2114_oled_push_data_event("HOST RX", data_ptr, length);
}

/**
 * @brief 按配置索引初始化 UART2 引脚映射，支持现场自动探测。
 *
 * 配置说明：
 * 0: TX=8 RX=7 MODE=2（默认板级配置）
 * 1: TX=8 RX=7 MODE=1（同引脚不同复用）
 * 2: TX=7 RX=8 MODE=2（收发交换）
 * 3: TX=7 RX=8 MODE=1（收发交换+复用切换）
 *
 * @param profile_idx 配置索引。
 * @param pin_cfg     输出 UART 引脚配置。
 * @param pin_mode    输出引脚复用模式。
 */
static void mine_wk2114_build_uart_profile(uint8_t profile_idx, uart_pin_config_t *pin_cfg, uint8_t *pin_mode)
{
    if ((pin_cfg == NULL) || (pin_mode == NULL)) {
        return;
    }

    switch (profile_idx) {
        case 1:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            *pin_mode = 1;
            break;
        case 2:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            *pin_mode = MINE_WK2114_HOST_UART_PIN_MODE;
            break;
        case 3:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            *pin_mode = 1;
            break;
        case 0:
        default:
            pin_cfg->tx_pin = MINE_WK2114_HOST_UART_TX_PIN;
            pin_cfg->rx_pin = MINE_WK2114_HOST_UART_RX_PIN;
            *pin_mode = MINE_WK2114_HOST_UART_PIN_MODE;
            break;
    }

    pin_cfg->cts_pin = PIN_NONE;
    pin_cfg->rts_pin = PIN_NONE;
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
    uint8_t profile_try;
    uint8_t profile_idx;
    uint8_t pin_mode = MINE_WK2114_HOST_UART_PIN_MODE;
    errcode_t ret;

    if (g_mine_wk2114_uart_ready) {
        return ERRCODE_SUCC;
    }

    uart_buffer_cfg.rx_buffer = g_mine_wk2114_uart_rx_buffer;
    uart_buffer_cfg.rx_buffer_size = sizeof(g_mine_wk2114_uart_rx_buffer);

    /* 先完成 IRQ 引脚初始化，确保芯片一上电就能捕获中断边沿。 */
    ret = mine_wk2114_irq_gpio_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] irq init failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK IRQ INIT FAIL");
        return ret;
    }

    for (profile_try = 0; profile_try < 4U; profile_try++) {
        profile_idx = (uint8_t)((g_mine_wk2114_uart_profile_index + profile_try) % 4U);
        mine_wk2114_build_uart_profile(profile_idx, &pin_cfg, &pin_mode);

        uapi_pin_set_mode(pin_cfg.tx_pin, pin_mode);
        uapi_pin_set_mode(pin_cfg.rx_pin, pin_mode);

        (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
        ret = uapi_uart_init(MINE_WK2114_HOST_UART_BUS, &pin_cfg, &attr, NULL, &uart_buffer_cfg);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[mine wk2114] uart init profile=%u failed, ret=%x\r\n",
                (unsigned int)profile_idx,
                ret);
            continue;
        }

#if defined(CONFIG_UART_SUPPORT_RX)
        ret = uapi_uart_register_rx_callback(MINE_WK2114_HOST_UART_BUS,
            (UART_RX_CONDITION_MASK_FULL |
            UART_RX_CONDITION_MASK_SUFFICIENT_DATA |
            UART_RX_CONDITION_MASK_IDLE),
            1,
            mine_wk2114_uart_rx_handler);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[mine wk2114] rx cb profile=%u failed, ret=%x\r\n",
                (unsigned int)profile_idx,
                ret);
            (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
            continue;
        }
#else
        mine_wk2114_log("[mine wk2114] uart rx callback api unavailable\r\n");
        mine_wk2114_oled_push_state("UART2 RX UNSUP");
        return ERRCODE_FAIL;
#endif

        /* 先允许主口收发，再执行规格书要求的链路就绪检查。 */
        g_mine_wk2114_uart_ready = true;
        ret = mine_wk2114_check_link_ready();
        if (ret == ERRCODE_SUCC) {
            g_mine_wk2114_uart_profile_index = profile_idx;
            mine_wk2114_log("[mine wk2114] uart profile ok idx=%u tx=%u rx=%u mode=%u\r\n",
                (unsigned int)profile_idx,
                (unsigned int)pin_cfg.tx_pin,
                (unsigned int)pin_cfg.rx_pin,
                (unsigned int)pin_mode);
            break;
        }

        osal_printk("[mine wk2114] link fail profile=%u, ret=%x\r\n",
            (unsigned int)profile_idx,
            ret);
        g_mine_wk2114_uart_ready = false;
        (void)uapi_uart_deinit(MINE_WK2114_HOST_UART_BUS);
    }

    if (ret != ERRCODE_SUCC) {
        osal_printk("[mine wk2114] link check stage failed, ret=%x\r\n", ret);
        mine_wk2114_oled_push_state("WK2114 LINK FAIL");
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
        mine_wk2114_uart_diag_report_periodic();
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
        mine_wk2114_uart_diag_report_periodic();
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
        /* 周期线程中消费 IRQ 置位事件，避免仅注册不处理。 */
        mine_wk2114_process_irq_event();
        mine_wk2114_uart_diag_report_periodic();
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
