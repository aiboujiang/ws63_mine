#include "remoteInfrared.h"

/************************************************************************
-------------------------红外接收协议--------------------------
开始拉低9ms,接着是一个4.5ms的高脉冲,通知器件开始传送数据了
接着是发送4个8位二进制码,第一二个是遥控识别码(REMOTE_ID),第一个为
正码(0),第二个为反码(255),接着两个数据是键值,第一个为正码
第二个为反码.发送完后40ms,遥控再发送一个9ms低,2ms高的脉冲,
表示按键的次数,出现一次则证明只按下了一次,如果出现多次,则可
以认为是持续按下该键.

*名称: Remote_Infrared_KEY_ISR(INT11_vect )
*功能: INT0中断服务程序
*参数: 无
*返回: 无
*************************************************************************/
uint32_t GlobalTimingDelay100us;

uint32_t FlagGotKey = 0;

Remote_Infrared_data_union RemoteInfrareddata;

#define RELOAD_CNT 100
#define TIMER_IRQ_PRIO 3 /* 中断优先级范围，从高到低: 0~7 */

timer_handle_t timer1_handle = 0;
void timer1_callback(uintptr_t data)
{
    unused(data);
    if (GlobalTimingDelay100us != 0) {
        GlobalTimingDelay100us--;
    }

    /* 开启下一次timer中断 */
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}
void ir_timer_init(void)
{
    uapi_timer_init();
    /* 设置 timer1 硬件初始化，设置中断号，配置优先级 */
    uapi_timer_adapter(TIMER_INDEX_1, TIMER_1_IRQN, TIMER_IRQ_PRIO);
    /* 创建 timer1 软件定时器控制句柄 */
    uapi_timer_create(TIMER_INDEX_1, &timer1_handle);
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}

// 当触发中断时的函数
void ir_callback_func(pin_t pin, uintptr_t param)
{
    unused(pin);
    unused(param);
    Remote_Infrared_KEY_ISR();
}
void user_ir_init(void)
{
    uapi_pin_set_mode(IR_IO, PIN_MODE_0);
    uapi_pin_set_pull(IR_IO, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(IR_IO, GPIO_DIRECTION_INPUT);

    /* 注册指定GPIO双边沿中断，回调函数为gpio_callback_func */
    if (uapi_gpio_register_isr_func(IR_IO, GPIO_INTERRUPT_DEDGE, ir_callback_func) != 0) {
        printf("register_isr_func fail!\r\n");
        uapi_gpio_unregister_isr_func(IR_IO);
    }
    uapi_gpio_enable_interrupt(IR_IO);
}

// 检测脉冲宽度最长脉宽为5ms
const uint32_t TIME_DELAY_6MS = 60;
const uint32_t TIME_DELAY_10MS = 100;
void Remote_Infrared_KEY_ISR(void)
{
    static uint8_t bBitCounter = 0; // 中断计数器，用于记录当前处理的数据位位置
    static uint32_t bKeyCode = 0;   // 存储正在接收的32位键值数据
    bBitCounter++;                  // 中断计数加1

    // 第1次中断：检测起始9ms低电平
    if (bBitCounter == 1) {
        if (uapi_gpio_get_val(IR_IO)) // 当前为高电平（不符合起始低电平预期）
        {
            bBitCounter = 0; // 重置计数器，重新开始检测
        } else {
            GlobalTimingDelay100us = TIME_DELAY_10MS; // 设置10ms超时，开始测量9ms低电平
        }
    }
    // 第2次中断：检测4.5ms高电平（引导码第二部分）
    else if (bBitCounter == 2) {
        if (uapi_gpio_get_val(IR_IO)) // 当前为高电平（符合预期）
        {
            // 检查低电平持续时间是否在0.2-2ms范围内（约9ms）
            if ((GlobalTimingDelay100us > 2) && (GlobalTimingDelay100us < 20)) {
                GlobalTimingDelay100us = TIME_DELAY_6MS; // 设置6ms超时，开始测量4.5ms高电平
            } else {
                bBitCounter = 0; // 持续时间不符合要求，重置
            }
        } else // 当前为低电平（不符合预期）
        {
            bBitCounter = 0; // 重置计数器
        }
    }
    // 第3次中断：确认引导码完成，准备接收数据
    else if (bBitCounter == 3) {
        if (uapi_gpio_get_val(IR_IO)) // 当前为高电平（不符合数据起始低电平预期）
        {
            bBitCounter = 0;
        } else {
            // 检查高电平持续时间：4.5ms（正常引导码）或2.25ms（重复码）
            if ((GlobalTimingDelay100us > 5) && (GlobalTimingDelay100us < 20)) // 4.5ms高电平，正常引导码
            {
                GlobalTimingDelay100us = TIME_DELAY_6MS; // 准备接收数据位
                // printf("引导码");
            } else if ((GlobalTimingDelay100us > 32) && (GlobalTimingDelay100us < 46)) // 2.25ms高电平，重复码
            {
                bBitCounter = 0;
                RemoteInfrareddata.uiRemoteInfraredData = bKeyCode; // 使用上一次的键值
                FlagGotKey = 1;                                     // 设置按键标志（按键保持）
            } else                                                  // 时间参数异常
            {
                bBitCounter = 0;
            }
        }
    }
    // 第4-67次中断：接收32位数据（4字节 × 8位）
    else if (bBitCounter > 3 && bBitCounter < 68) {
        // 上升沿处理（高电平开始）：测量高电平持续时间前的准备
        if (uapi_gpio_get_val(IR_IO)) {
            // 检查低电平持续时间应为560us±10%
            if ((GlobalTimingDelay100us > 50) && (GlobalTimingDelay100us < 59)) {
                GlobalTimingDelay100us = TIME_DELAY_6MS; // 开始测量高电平持续时间
            } else {
                bBitCounter = 0; // 低电平时间不符合要求
            }
        }
        // 下降沿处理（低电平开始）：根据高电平持续时间判断数据位0/1
        else {
            // 判断数据位：高电平持续时间560us为0，1.685ms为1
            if ((GlobalTimingDelay100us > 52) && (GlobalTimingDelay100us < 59)) // 560us → 逻辑'0'
            {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
                bKeyCode <<= 1;   // 左移一位（MSB First高位在前）
                bKeyCode += 0x00; // 添加0

            } else if ((GlobalTimingDelay100us > 40) && (GlobalTimingDelay100us < 53)) // 1.685ms → 逻辑'1'
            {
                GlobalTimingDelay100us = TIME_DELAY_6MS;
                bKeyCode <<= 1;   // 左移一位
                bKeyCode += 0x01; // 添加1

            } else {
                bBitCounter = 0; // 时间参数异常，重新开始
            }
        }

        if (bBitCounter == 67) {
            RemoteInfrareddata.uiRemoteInfraredData = bKeyCode; // 存储接收到的键值
            bBitCounter = 0;                                    // 重置计数器
            FlagGotKey = 1;                                     // 设置按键收到标志
        }
    }
    // 异常情况处理：超出预期中断次数
    else {
        bBitCounter = 0; // 强制重置计数器
    }
}

/**
 * @brief  红外遥控按键解码函数
 * @note   对接收到的红外数据进行校验和解码，返回有效的按键值
 *         使用NEC协议的反码校验机制确保数据可靠性
 * @param  无
 * @retval uint8_t: 解码后的有效按键值（如果校验失败返回原始数据）
 */
uint8_t Remote_Infrared_KeyDeCode(void)
{
    // 计算反码校验值：对接收到的反码字段再次取反，应该等于正码字段
    uint8_t temp1 = ~RemoteInfrareddata.RemoteInfraredDataStruct.bIDNot;      // 用户码反码的逆值
    uint8_t temp2 = ~RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCodeNot; // 按键码反码的逆值

    // 检查是否有新的按键数据到达
    if (FlagGotKey == 1) // 通码（新按键或重复码）
    {
        FlagGotKey = 0; // 清除按键标志，准备接收下一次按键

        // NEC协议校验：正码字段应该等于反码字段取反后的值
        if ((RemoteInfrareddata.RemoteInfraredDataStruct.bID == temp1) &&
            (RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCode == temp2)) {
            // 校验成功，打印详细的解码信息（调试用）
            printf("用户码：%x\t用户码反码: %x\t按键码: %x\t按键码反码: %x\n",
                   (RemoteInfrareddata.uiRemoteInfraredData >> 24) & 0xFF, // 提取用户码（第1字节）
                   (RemoteInfrareddata.uiRemoteInfraredData >> 16) & 0xFF, // 提取用户码反码（第2字节）
                   (RemoteInfrareddata.uiRemoteInfraredData >> 8) & 0xFF,  // 提取按键码（第3字节）
                   (RemoteInfrareddata.uiRemoteInfraredData & 0xFF));      // 提取按键码反码（第4字节）

            // 此处可以添加按键处理逻辑，如调用按键回调函数等
        }
        // 如果校验失败，此处可以添加错误处理逻辑
        // else {
        //     printf("红外数据校验失败！\n");
        // }
    }

    // 返回按键码（无论校验是否成功都返回，调用者需自行判断数据有效性）
    return RemoteInfrareddata.RemoteInfraredDataStruct.bKeyCode;
}
