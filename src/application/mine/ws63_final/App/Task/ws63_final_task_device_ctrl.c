/**
 * @file ws63_final_task_device_ctrl.c
 * @brief Task 层设备控制子模块（电机/编码器/蜂鸣器）。
 */

#include "ws63_final_task_internal.h"

#include "osal_debug.h"

#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_motor.h"
#include "ws63_encoder.h"
#include "ws63_buzzer.h"

/* 设备能力就绪标记：任务层只通过能力状态对外提供控制接口。 */
static uint8_t g_ws63_motor_encoder_ready = 0U;
static uint8_t g_ws63_buzzer_ready = 0U;

/* 蜂鸣器独立任务状态：通过队列串行化硬件访问。 */
static unsigned long g_ws63_beep_ctrl_queue = 0UL;
static uint8_t g_ws63_beep_task_started = 0U;
static uint16_t g_ws63_buzzer_freq_hz_cache = WS63_BEEP_DEFAULT_FREQ_HZ;
static uint8_t g_ws63_buzzer_volume_cache = WS63_BEEP_DEFAULT_VOLUME_PERCENT;
static uint8_t g_ws63_buzzer_on_cache = 0U;

/**
 * @brief 蜂鸣器状态更新加锁。
 */
static unsigned int ws63_beep_state_lock(void)
{
    return ws63_os_irq_lock();
}

/**
 * @brief 蜂鸣器状态更新解锁。
 */
static void ws63_beep_state_unlock(unsigned int irq_status)
{
    ws63_os_irq_unlock(irq_status);
}

/**
 * @brief 尝试初始化蜂鸣器驱动。
 */
static errcode_t ws63_beep_try_init(void)
{
    errcode_t ret;
    unsigned int irq_status;

    if (g_ws63_buzzer_ready == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_buzzer_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] buzzer init fail, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    irq_status = ws63_beep_state_lock();
    g_ws63_buzzer_ready = 1U;
    g_ws63_buzzer_on_cache = 0U;
    g_ws63_buzzer_freq_hz_cache = WS63_BEEP_DEFAULT_FREQ_HZ;
    g_ws63_buzzer_volume_cache = WS63_BEEP_DEFAULT_VOLUME_PERCENT;
    ws63_beep_state_unlock(irq_status);

    osal_printk("[wk2114 final task] buzzer init ok (GPIO9/PWM1)\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 执行一条蜂鸣器控制命令。
 */
static void ws63_beep_process_ctrl_msg(const ws63_beep_ctrl_msg_t *msg)
{
    errcode_t ret;
    unsigned int irq_status;

    if (msg == NULL) {
        return;
    }

    if (ws63_beep_try_init() != ERRCODE_SUCC) {
        return;
    }

    switch (msg->type) {
        case WS63_BEEP_CMD_ON:
            ret = ws63_buzzer_start(msg->freq_hz);
            if (ret == ERRCODE_SUCC) {
                irq_status = ws63_beep_state_lock();
                g_ws63_buzzer_freq_hz_cache = msg->freq_hz;
                g_ws63_buzzer_on_cache = 1U;
                ws63_beep_state_unlock(irq_status);
            }
            break;
        case WS63_BEEP_CMD_OFF:
            ret = ws63_buzzer_stop();
            if (ret == ERRCODE_SUCC) {
                irq_status = ws63_beep_state_lock();
                g_ws63_buzzer_on_cache = 0U;
                ws63_beep_state_unlock(irq_status);
            }
            break;
        case WS63_BEEP_CMD_SET_VOLUME:
            ret = ws63_buzzer_set_volume(msg->volume_percent);
            if (ret == ERRCODE_SUCC) {
                irq_status = ws63_beep_state_lock();
                g_ws63_buzzer_volume_cache = msg->volume_percent;
                ws63_beep_state_unlock(irq_status);
            }
            break;
        default:
            break;
    }
}

/**
 * @brief 蜂鸣器独立任务入口：阻塞等待控制消息并串行执行。
 */
static void *ws63_beep_task_entry(const char *arg)
{
    ws63_beep_ctrl_msg_t msg;

    (void)arg;
    (void)ws63_beep_try_init();

    while (1) {
        uint32_t size = (uint32_t)sizeof(msg);

        if (ws63_os_msg_queue_recv(g_ws63_beep_ctrl_queue, &msg, &size, WS63_OS_WAIT_FOREVER) != ERRCODE_SUCC) {
            continue;
        }

        ws63_beep_process_ctrl_msg(&msg);
    }

    return NULL;
}

/**
 * @brief 启动蜂鸣器独立任务。
 */
errcode_t ws63_beep_task_start(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    errcode_t ret;

    if (g_ws63_beep_task_started == 1U) {
        return ERRCODE_SUCC;
    }

    ret = ws63_os_msg_queue_create("ws63_beep_q",
        WS63_BEEP_CTRL_QUEUE_DEPTH,
        (uint16_t)sizeof(ws63_beep_ctrl_msg_t),
        &g_ws63_beep_ctrl_queue);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] beep queue create fail\r\n");
        return ret;
    }

    ret = ws63_os_start_task("ws63_beep_task",
        ws63_beep_task_entry,
        0U,
        WS63_BEEP_TASK_STACK_SIZE,
        WS63_BEEP_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        ws63_os_msg_queue_delete(g_ws63_beep_ctrl_queue);
        g_ws63_beep_ctrl_queue = 0UL;
        osal_printk("[wk2114 final task] beep task start fail\r\n");
        return ret;
    }

    g_ws63_beep_task_started = 1U;
    osal_printk("[wk2114 final task] beep task start ok\r\n");
    return ERRCODE_SUCC;
#endif
}

/**
 * @brief 初始化电机与编码器能力。
 */
void ws63_motor_encoder_init(void)
{
    errcode_t ret;

    g_ws63_motor_encoder_ready = 0U;

    ret = ws63_motor_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] motor init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    ret = ws63_encoder_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final task] encoder init fail, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    g_ws63_motor_encoder_ready = 1U;
    osal_printk("[wk2114 final task] motor+encoder init ok (IA=GPIO2 IB=GPIO3 ENC=GPIO11/12)\r\n");
}

/**
 * @brief 查询电机/编码器能力是否可用。
 */
uint8_t ws63_task_motor_encoder_is_ready(void)
{
    return g_ws63_motor_encoder_ready;
}

/**
 * @brief 初始化蜂鸣器能力。
 */
void ws63_task_buzzer_init(void)
{
#if (WS63_BEEP_ENABLE == 1U)
    (void)ws63_beep_task_start();
#else
    g_ws63_buzzer_ready = 0U;
#endif
}

/**
 * @brief 控制电机正转（IA=0，IB=PWM）。
 */
errcode_t ws63_task_motor_forward(uint8_t duty_percent)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_forward(duty_percent);
}

/**
 * @brief 控制电机反转（IA=PWM，IB=0）。
 */
errcode_t ws63_task_motor_reverse(uint8_t duty_percent)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_reverse(duty_percent);
}

/**
 * @brief 电机停止（滑行，IA=0，IB=0）。
 */
errcode_t ws63_task_motor_coast_stop(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_coast_stop();
}

/**
 * @brief 电机刹车（急停，IA=1，IB=1）。
 */
errcode_t ws63_task_motor_brake_stop(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_brake_stop();
}

/**
 * @brief 动态调整当前运行方向占空比。
 */
errcode_t ws63_task_motor_set_duty(uint8_t duty_percent)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return ERRCODE_FAIL;
    }

    return ws63_motor_set_duty(duty_percent);
}

/**
 * @brief 获取编码器最新 RPM。
 */
int32_t ws63_task_get_motor_rpm(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return 0;
    }

    return ws63_encoder_get_rpm();
}

/**
 * @brief 获取编码器累计计数值。
 */
int32_t ws63_task_get_encoder_total_count(void)
{
    if (g_ws63_motor_encoder_ready == 0U) {
        return 0;
    }

    return ws63_encoder_get_total_count();
}

/**
 * @brief 打开蜂鸣器并设置频率。
 */
errcode_t ws63_task_buzzer_on(uint16_t freq_hz)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)freq_hz;
    return ERRCODE_FAIL;
#else
    ws63_beep_ctrl_msg_t msg;

    if (g_ws63_beep_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    msg.type = WS63_BEEP_CMD_ON;
    msg.freq_hz = freq_hz;
    msg.volume_percent = 0U;
    return ws63_os_msg_queue_send(g_ws63_beep_ctrl_queue,
        &msg,
        (uint16_t)sizeof(msg),
        WS63_OS_NO_WAIT);
#endif
}

/**
 * @brief 设置蜂鸣器音量。
 */
errcode_t ws63_task_buzzer_set_volume(uint8_t volume_percent)
{
#if (WS63_BEEP_ENABLE != 1U)
    (void)volume_percent;
    return ERRCODE_FAIL;
#else
    ws63_beep_ctrl_msg_t msg;

    if (g_ws63_beep_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    msg.type = WS63_BEEP_CMD_SET_VOLUME;
    msg.freq_hz = 0U;
    msg.volume_percent = volume_percent;
    return ws63_os_msg_queue_send(g_ws63_beep_ctrl_queue,
        &msg,
        (uint16_t)sizeof(msg),
        WS63_OS_NO_WAIT);
#endif
}

/**
 * @brief 关闭蜂鸣器。
 */
errcode_t ws63_task_buzzer_off(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return ERRCODE_FAIL;
#else
    ws63_beep_ctrl_msg_t msg;

    if (g_ws63_beep_task_started == 0U) {
        return ERRCODE_FAIL;
    }

    msg.type = WS63_BEEP_CMD_OFF;
    msg.freq_hz = 0U;
    msg.volume_percent = 0U;
    return ws63_os_msg_queue_send(g_ws63_beep_ctrl_queue,
        &msg,
        (uint16_t)sizeof(msg),
        WS63_OS_NO_WAIT);
#endif
}

/**
 * @brief 查询蜂鸣器是否正在发声。
 */
uint8_t ws63_task_buzzer_is_on(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t on;

    irq_status = ws63_beep_state_lock();
    on = g_ws63_buzzer_on_cache;
    ws63_beep_state_unlock(irq_status);
    return on;
#endif
}

/**
 * @brief 获取蜂鸣器当前频率。
 */
uint16_t ws63_task_buzzer_get_freq_hz(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint16_t freq;

    irq_status = ws63_beep_state_lock();
    freq = g_ws63_buzzer_freq_hz_cache;
    ws63_beep_state_unlock(irq_status);
    return freq;
#endif
}

/**
 * @brief 获取蜂鸣器当前音量。
 */
uint8_t ws63_task_buzzer_get_volume(void)
{
#if (WS63_BEEP_ENABLE != 1U)
    return 0U;
#else
    unsigned int irq_status;
    uint8_t volume;

    irq_status = ws63_beep_state_lock();
    volume = g_ws63_buzzer_volume_cache;
    ws63_beep_state_unlock(irq_status);
    return volume;
#endif
}
