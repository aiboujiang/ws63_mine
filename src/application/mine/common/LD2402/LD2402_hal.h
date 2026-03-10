#ifndef __LD2402_HAL_H__
#define __LD2402_HAL_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 硬件抽象层接口结构体
 * 用户需在应用层实现以下函数指针
 */
typedef struct {
    /**
     * @brief 发送数据到 UART
     * @param data 数据指针
     * @param len 数据长度
     * @return 0 成功，非 0 失败
     */
    int (*uart_send)(const uint8_t *data, uint16_t len);

    /**
     * @brief 获取系统毫秒 tick
     * @return 当前毫秒数
     */
    uint32_t (*get_tick_ms)(void);

    /**
     * @brief 延时函数 (ms)
     * @param ms 延时毫秒数
     */
    void (*delay_ms)(uint32_t ms);

    /**
     * @brief 开启/关闭 UART 接收中断 (可选，用于流控)
     * @param enable true 开启，false 关闭
     */
    void (*uart_rx_irq_ctrl)(bool enable);
} LD2402_HAL_t;

#endif // __LD2402_HAL_H__