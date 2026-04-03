/**
 * @file mpr121_keypad_demo.c
 * @brief MPR121 触摸键盘示例（WS63 版，驱动与业务一体化实现）。
 */

#include "app_init.h"
#include "gpio.h"
#include <i2c.h>
#include "mpr121.h"
#include "osal_debug.h"
#include "pinctrl.h"
#include "platform_core_rom.h"
#include "soc_osal.h"

/* MPR121 器件地址：7-bit 0x5A。 */
#define MPR121_I2C_ADDR                  0x5AU
#define MPR121_I2C_SPEED                 100000U
#define MPR121_I2C_HIGH_SPEED_CODE       0U

/* 按用户要求固定 I2C1 引脚复用。 */
#define MPR121_I2C_SDA_PIN               GPIO_15
#define MPR121_I2C_SCL_PIN               GPIO_16
#define MPR121_I2C_PIN_MODE              2

/* 按用户确认：IRQ 走 GPIO5，低电平有效。 */
#define MPR121_IRQ_PIN                   GPIO_05
#define MPR121_IRQ_PIN_MODE              HAL_PIO_FUNC_GPIO

/* 触摸状态寄存器：低字节 0x00，高字节 0x01。 */
#define MPR121_REG_TOUCH_STATUS_L        0x00U
#define MPR121_REG_TOUCH_STATUS_H        0x01U

/* 仅低 12 位有效，对应 ELE0~ELE11。 */
#define MPR121_TOUCH_VALID_MASK          0x0FFFU

/* 任务参数：与 mine 现有模块保持一致的优先级区间。 */
#define MPR121_TASK_PRIO                 26
#define MPR121_TASK_STACK_SIZE           0x1000
#define MPR121_TASK_POLL_MS              10U
#define MPR121_BOOT_DELAY_MS             100U
#define MPR121_INIT_RETRY_MS             3000U

/* IRQ 回调与任务线程之间的事件标志。 */
static volatile uint8_t g_mpr121_irq_pending = 0U;

/* 记录上次状态用于边沿检测，避免长按重复刷屏。 */
static uint16_t g_mpr121_last_status = 0U;

/**
 * @brief GPIO5 中断回调。
 *
 * 中断上下文中只置位事件，I2C 读取放在线程中执行，避免中断里阻塞。
 */
static void mpr121_irq_callback(pin_t pin, uintptr_t param)
{
    (void)param;

    if (pin != MPR121_IRQ_PIN) {
        return;
    }

    g_mpr121_irq_pending = 1U;
}

/**
 * @brief 向 MPR121 写单个寄存器。
 *
 * @param reg_addr 寄存器地址。
 * @param reg_val 写入值。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_write_reg(uint8_t reg_addr, uint8_t reg_val)
{
    uint8_t tx_buf[2] = {reg_addr, reg_val};
    i2c_data_t data = {0};

    data.send_buf = tx_buf;
    data.send_len = sizeof(tx_buf);
    data.receive_buf = NULL;
    data.receive_len = 0;

    return uapi_i2c_master_write(I2C_BUS_1, MPR121_I2C_ADDR, &data);
}

/**
 * @brief 读取 MPR121 单个寄存器。
 *
 * @param reg_addr 寄存器地址。
 * @param reg_val 输出参数，返回寄存器值。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_read_reg(uint8_t reg_addr, uint8_t *reg_val)
{
    i2c_data_t data = {0};

    if (reg_val == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    data.send_buf = &reg_addr;
    data.send_len = 1U;
    data.receive_buf = reg_val;
    data.receive_len = 1U;

    return uapi_i2c_master_writeread(I2C_BUS_1, MPR121_I2C_ADDR, &data);
}

/**
 * @brief 下发原示例中的 MPR121 快速配置序列。
 *
 * 该序列复用示例的 A/B/C/D/E 段寄存器配置，目标是快速稳定拉起 12 路触摸检测。
 *
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_quick_config(void)
{
    errcode_t ret;

    /* Section A: data > baseline 时的滤波参数。 */
    ret = mpr121_write_reg(MHD_R, 0x01U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(NHD_R, 0x01U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(NCL_R, 0x00U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(FDL_R, 0x00U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section B: data < baseline 时的滤波参数。 */
    ret = mpr121_write_reg(MHD_F, 0x01U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(NHD_F, 0x01U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(NCL_F, 0xFFU);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(FDL_F, 0x02U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section C: 每路电极触摸/释放阈值。 */
    ret = mpr121_write_reg(ELE0_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE0_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE1_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE1_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE2_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE2_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE3_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE3_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE4_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE4_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE5_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE5_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE6_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE6_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE7_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE7_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE8_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE8_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE9_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE9_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE10_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE10_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE11_T, TOU_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg(ELE11_R, REL_THRESH);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section D: Filter Configuration。 */
    ret = mpr121_write_reg(FIL_CFG, 0x04U);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section E: 使能 12 路电极，切到 run 模式。 */
    ret = mpr121_write_reg(ELE_CFG, 0x0CU);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return ERRCODE_SUCC;
}

/**
 * @brief 读取 MPR121 触摸状态位图。
 *
 * @param touch_status 输出参数，bit0~bit11 对应 12 路电极。
 * @return errcode_t ERRCODE_SUCC 表示读取成功。
 */
static errcode_t mpr121_read_touch_status(uint16_t *touch_status)
{
    errcode_t ret;
    uint8_t status_l;
    uint8_t status_h;

    if (touch_status == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = mpr121_read_reg(MPR121_REG_TOUCH_STATUS_L, &status_l);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = mpr121_read_reg(MPR121_REG_TOUCH_STATUS_H, &status_h);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    *touch_status = (((uint16_t)status_h << 8U) | (uint16_t)status_l) & MPR121_TOUCH_VALID_MASK;
    return ERRCODE_SUCC;
}

/**
 * @brief 初始化 I2C1 与 pinmux。
 *
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_i2c_bus_init(void)
{
    errcode_t ret;
    uint8_t attempt;

    ret = uapi_pin_set_mode(MPR121_I2C_SCL_PIN, MPR121_I2C_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] set SCL pin mode failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = uapi_pin_set_mode(MPR121_I2C_SDA_PIN, MPR121_I2C_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] set SDA pin mode failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    /*
     * 某些板级配置下，GPIO15/16 的 pull 能力可能不可配或被硬件固定，
     * 这里降级为“打印告警但继续初始化”，避免无谓阻断 I2C 总线启动。
     */
    ret = uapi_pin_set_pull(MPR121_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] warn: set SCL pull-up failed, ret=0x%x\r\n", (unsigned int)ret);
    }

    ret = uapi_pin_set_pull(MPR121_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] warn: set SDA pull-up failed, ret=0x%x\r\n", (unsigned int)ret);
    }

    /*
     * 若总线曾被其它路径占用或上次初始化残留，先 deinit 再 init，
     * 并做一次重试，提升现场冷启动稳定性。
     */
    for (attempt = 0U; attempt < 2U; attempt++) {
        if (attempt > 0U) {
            (void)uapi_i2c_deinit(I2C_BUS_1);
            (void)osal_msleep(5);
        }

        ret = uapi_i2c_master_init(I2C_BUS_1, MPR121_I2C_SPEED, MPR121_I2C_HIGH_SPEED_CODE);
        if (ret == ERRCODE_SUCC) {
            return ERRCODE_SUCC;
        }

        if (ret == ERRCODE_I2C_ALREADY_INIT) {
            (void)uapi_i2c_deinit(I2C_BUS_1);
            ret = uapi_i2c_master_init(I2C_BUS_1, MPR121_I2C_SPEED, MPR121_I2C_HIGH_SPEED_CODE);
            if (ret == ERRCODE_SUCC) {
                return ERRCODE_SUCC;
            }
        }

        osal_printk("[mpr121] i2c1 init attempt %u failed, ret=0x%x\r\n",
            (unsigned int)(attempt + 1U),
            (unsigned int)ret);
    }

    return ret;
}

/**
 * @brief 初始化 MPR121 的 IRQ 引脚。
 *
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_irq_pin_init(void)
{
    errcode_t ret;

    ret = uapi_pin_set_mode(MPR121_IRQ_PIN, MPR121_IRQ_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_pull(MPR121_IRQ_PIN, PIN_PULL_TYPE_UP);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_set_dir(MPR121_IRQ_PIN, GPIO_DIRECTION_INPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_gpio_register_isr_func(MPR121_IRQ_PIN, GPIO_INTERRUPT_FALLING_EDGE, mpr121_irq_callback);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    return uapi_gpio_enable_interrupt(MPR121_IRQ_PIN);
}

/**
 * @brief 初始化 MPR121 驱动。
 *
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_init(void)
{
    errcode_t ret;

    uapi_gpio_init();

    ret = mpr121_i2c_bus_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] i2c bus init failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = mpr121_irq_pin_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] irq pin init failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    ret = mpr121_quick_config();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] quick config failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    /* 若上电瞬间已经有触摸，确保线程能在下一轮读取到状态。 */
    if (uapi_gpio_get_val(MPR121_IRQ_PIN) == GPIO_LEVEL_LOW) {
        g_mpr121_irq_pending = 1U;
    }

    osal_printk("[mpr121] init ok: I2C1 SDA=GPIO15(mode2), SCL=GPIO16(mode2), IRQ=GPIO5\r\n");
    return ERRCODE_SUCC;
}

/**
 * @brief 计算 16bit 位图中置位数量。
 *
 * @param value 输入位图。
 * @return uint8_t 置位 bit 数。
 */
static uint8_t mpr121_count_set_bits(uint16_t value)
{
    uint8_t cnt = 0U;

    while (value != 0U) {
        if ((value & 0x1U) != 0U) {
            cnt++;
        }
        value >>= 1U;
    }

    return cnt;
}

/**
 * @brief 处理一次触摸状态采样并输出原始状态位图。
 *
 * 关键策略：
 * 1) 仅在状态变化时处理，抑制重复日志；
 * 2) 输出当前状态、按下边沿、释放边沿，便于上层自定义映射；
 * 3) 不在此处做按键字符映射，避免业务耦合。
 */
static void mpr121_process_touch_once(void)
{
    errcode_t ret;
    uint16_t curr_status;
    uint16_t new_pressed;
    uint16_t released;
    uint8_t active_bits;

    ret = mpr121_read_touch_status(&curr_status);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] read touch status failed, ret=0x%x\r\n", (unsigned int)ret);
        return;
    }

    if (curr_status == g_mpr121_last_status) {
        return;
    }

    new_pressed = curr_status & (uint16_t)(~g_mpr121_last_status);
    released = g_mpr121_last_status & (uint16_t)(~curr_status);
    active_bits = mpr121_count_set_bits(curr_status);

    osal_printk("[mpr121] status=0x%03x, press=0x%03x, release=0x%03x, active=%u\r\n",
        (unsigned int)curr_status,
        (unsigned int)new_pressed,
        (unsigned int)released,
        (unsigned int)active_bits);

    g_mpr121_last_status = curr_status;
}

/**
 * @brief MPR121 演示任务。
 *
 * @param arg 任务参数。
 * @return void* 固定返回 NULL。
 */
static void *mpr121_keypad_task(const char *arg)
{
    errcode_t ret;

    (void)arg;

    (void)osal_msleep(MPR121_BOOT_DELAY_MS);

    while (1) {
        ret = mpr121_init();
        if (ret == ERRCODE_SUCC) {
            break;
        }

        /* 初始化失败不退出任务，后台重试，避免一次失败导致功能永久不可用。 */
        osal_printk("[mpr121] init failed, retry after %u ms, ret=0x%x\r\n",
            (unsigned int)MPR121_INIT_RETRY_MS,
            (unsigned int)ret);
        (void)osal_msleep(MPR121_INIT_RETRY_MS);
    }

    osal_printk("[mpr121] keypad task started, output mode=raw status\r\n");

    while (1) {
        /* IRQ 事件优先，IRQ 线低电平作为兜底，防止极端场景漏中断。 */
        if ((g_mpr121_irq_pending != 0U) || (uapi_gpio_get_val(MPR121_IRQ_PIN) == GPIO_LEVEL_LOW)) {
            g_mpr121_irq_pending = 0U;
            mpr121_process_touch_once();
        }

        (void)osal_msleep(MPR121_TASK_POLL_MS);
    }

    return NULL;
}

/**
 * @brief 应用入口：创建 MPR121 任务。
 */
static void mpr121_keypad_entry(void)
{
    osal_task *task_handle = NULL;

    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)mpr121_keypad_task,
        0,
        "Mpr121Task",
        MPR121_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, MPR121_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* 注册到系统启动流程。 */
app_run(mpr121_keypad_entry);
