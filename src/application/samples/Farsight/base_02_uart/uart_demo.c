/*
 * Copyright (c) 2024 Beijing HuaQingYuanJian Education Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pinctrl.h"
#include "uart.h"
#include "osal_debug.h"
#include "soc_osal.h"
#include "app_init.h"
#include "string.h"
#include "cmsis_os2.h"
osThreadId_t task1_ID; // 任务1 ID

#define DELAY_TIME_MS 1
#define UART_RECV_SIZE 10

uint8_t uart_recv[UART_RECV_SIZE] = {0};

#define UART_INT_MODE 1
#if (UART_INT_MODE)
static uint8_t uart_rx_flag = 0;
#endif
uart_buffer_config_t g_app_uart_buffer_config = {.rx_buffer = uart_recv, .rx_buffer_size = UART_RECV_SIZE};
/**
 * @brief 初始化UART相关GPIO引脚
 * @note 配置UART TX/RX引脚到对应的复用模式
 */
void uart_gpio_init(void)
{
    // 设置GPIO17为模式1 (UART TX功能)
    uapi_pin_set_mode(GPIO_17, PIN_MODE_1);
    // 设置GPIO18为模式1 (UART RX功能)
    uapi_pin_set_mode(GPIO_18, PIN_MODE_1);
}

/**
 * @brief 初始化UART控制器配置
 * @note 配置UART通信参数：波特率、数据位、停止位、校验位等
 */
void uart_init_config(void)
{
    // 定义UART属性配置结构体并初始化
    uart_attr_t attr = {
        .baud_rate = 115200,     // 波特率：115200bps
        .data_bits = UART_DATA_BIT_8,  // 数据位：8位
        .stop_bits = UART_STOP_BIT_1,  // 停止位：1位
        .parity = UART_PARITY_NONE     // 校验位：无校验
    };

    // 定义UART引脚配置结构体
    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO0,   // 发送引脚
        .rx_pin = S_MGPIO1,   // 接收引脚
        .cts_pin = PIN_NONE,  // 清除发送引脚（未使用）
        .rts_pin = PIN_NONE   // 请求发送引脚（未使用）
    };
    
    // 先反初始化UART0，确保处于已知状态
    uapi_uart_deinit(UART_BUS_0);
    
    // 初始化UART0控制器
    // 参数：UART端口号、引脚配置、属性配置、DMA配置（NULL表示不使用）、缓冲区配置
    int ret = uapi_uart_init(UART_BUS_0, &pin_config, &attr, NULL, &g_app_uart_buffer_config);
    
    // 检查初始化结果
    if (ret != 0) {
        // 初始化失败，打印错误码
        printf("uart init failed ret = %02x\n", ret);
    }
}
/**
 * @brief UART中断模式接收回调函数
 * @note 此函数在UART中断模式下，当接收到数据时被硬件中断调用
 *       函数在中断上下文中执行，应保持简短高效，避免阻塞操作
 * @param buffer 接收数据缓冲区指针，指向硬件接收到的原始数据
 * @param length 接收到的数据长度（字节数）
 * @param error 接收错误标志，true表示接收过程中发生错误
 */
#if (UART_INT_MODE)  // 条件编译：仅在启用UART中断模式时包含此函数
void uart_read_handler(const void *buffer, uint16_t length, bool error)
{
    unused(error);  // 标记未使用参数，避免编译器警告
    
    // 参数合法性检查：确保缓冲区有效且数据长度不为零
    if (buffer == NULL || length == 0) {
        return;  // 非法参数，直接返回
    }
    
    // 安全地将接收到的数据拷贝到应用层缓冲区
    // 使用memcpy_s确保不会发生缓冲区溢出
    if (memcpy_s(uart_recv,         // 目标缓冲区（应用层缓冲区）
                 length,            // 目标缓冲区大小
                 buffer,            // 源缓冲区（硬件接收缓冲区）
                 length) != EOK) {  // 拷贝数据长度
        return;  // 数据拷贝失败，直接返回
    }
    
    // 设置接收完成标志，通知主任务有新的数据需要处理
    uart_rx_flag = 1;
}
#endif  // 结束条件编译

/**
 * @brief UART通信任务主函数
 * @param arg 任务参数（未使用）
 * @return NULL
 */
void *uart_task(const char *arg)
{
    unused(arg);  // 避免未使用参数警告
    
    /* 硬件初始化阶段 */
    uart_gpio_init();      // 初始化UART引脚复用配置
    uart_init_config();    // 初始化UART控制器参数
    
    /* 中断模式特定配置 */
#if (UART_INT_MODE)  // 条件编译：中断模式
    // 注册UART接收中断回调函数
    if (uapi_uart_register_rx_callback(0,                         // UART端口号
                                      UART_RX_CONDITION_MASK_IDLE, // 触发条件：检测到空闲中断
                                      1,                         // 使能回调
                                      uart_read_handler) == ERRCODE_SUCC) {
        // 注册成功，可在此添加成功日志
    }
#endif

    /* 任务主循环 */
    while (1) {
#if (UART_INT_MODE)  // 中断模式处理流程
        // 等待接收完成标志置位（非忙等待方式）
        while (!uart_rx_flag) {
            osDelay(DELAY_TIME_MS);  // 短暂延时，避免CPU占用率过高
        }
        
        // 清除接收标志，准备接收下一帧数据
        uart_rx_flag = 0;
        
        // 处理接收到的数据：打印内容
        printf("uart int rx = [%s]\n", uart_recv);
        
        // 清空接收缓冲区，防止旧数据影响下一次接收
        memset(uart_recv, 0, UART_RECV_SIZE);

#else  // 轮询模式处理流程
        // 尝试读取UART数据（非阻塞模式）
        if (uapi_uart_read(UART_BUS_0,      // UART端口号
                          uart_recv,        // 接收缓冲区
                          UART_RECV_SIZE,   // 缓冲区大小
                          0)) {             // 超时时间0：非阻塞模式
            // 读取成功，打印接收到的数据
            printf("uart poll rx = ");
            
            // 将接收到的数据回环发送（echo功能）
            uapi_uart_write(UART_BUS_0,     // UART端口号
                           uart_recv,       // 发送数据缓冲区
                           UART_RECV_SIZE,  // 发送数据长度
                           0);              // 超时时间0：非阻塞模式
        }
        // 轮询模式下可添加适当延时，控制采样频率
        osDelay(10);
#endif
    }
    
    return NULL;
}

static void uart_entry(void)
{
    printf("Enter uart_entry()!\r\n");

    osThreadAttr_t attr;
    attr.name = "UartTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x1000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)uart_task, NULL, &attr);
    if (task1_ID != NULL) {
        printf("ID = %d, Create task1_ID is OK!\r\n", task1_ID);
    }
}

/* Run the uart_entry. */
app_run(uart_entry);