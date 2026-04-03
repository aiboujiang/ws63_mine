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
#define MPR121_REG_SOFT_RESET            0x80U

/* 仅低 12 位有效，对应 ELE0~ELE11。 */
#define MPR121_TOUCH_VALID_MASK          0x0FFFU
#define MPR121_ELECTRODE_COUNT           12U
#define MPR121_ELE_CFG_RUN_12            0x0CU

/* I2C 通信容错：读写失败后进行短延时重试。 */
#define MPR121_I2C_RW_RETRY_MAX          3U
#define MPR121_I2C_RW_RETRY_DELAY_MS     2U
#define MPR121_CFG_RETRY_MAX             2U
#define MPR121_SOFT_RESET_CMD            0x63U
#define MPR121_SOFT_RESET_DELAY_MS       2U

/* 任务参数：本任务逻辑较轻，适当降低优先级并缩小栈占用。 */
#define MPR121_TASK_PRIO                 27
#define MPR121_TASK_STACK_SIZE           0x0C00
#define MPR121_TASK_POLL_MS              10U
#define MPR121_BOOT_DELAY_MS             100U
#define MPR121_INIT_RETRY_MS             3000U

/* IRQ 回调与任务线程之间的事件标志。 */
static volatile uint8_t g_mpr121_irq_pending = 0U;

/* 记录上次状态用于边沿检测，避免长按重复刷屏。 */
static uint16_t g_mpr121_last_status = 0U;

/* 保存任务句柄，避免误释放内核对象导致异常。 */
static osal_task *g_mpr121_task_handle = NULL;

/**
 * @brief 置位 IRQ 待处理标志（带发布屏障）。
 */
static inline void mpr121_irq_pending_set(void)
{
    g_mpr121_irq_pending = 1U;
    osal_wmb();
}

/**
 * @brief 原子读取并清零 IRQ 待处理标志（带获取屏障）。
 *
 * @return uint8_t 1 表示有待处理 IRQ，0 表示无事件。
 */
static inline uint8_t mpr121_irq_pending_test_and_clear(void)
{
    uint8_t pending;
    unsigned int irq_state;

    /*
     * 通过关中断保护“读并清零”临界区，避免 ISR 与线程并发导致事件丢失。
     * 配合 rmb/mb 保证跨上下文的可见性顺序。
     */
    irq_state = osal_irq_lock();
    osal_rmb();
    pending = g_mpr121_irq_pending;
    g_mpr121_irq_pending = 0U;
    osal_mb();
    osal_irq_restore(irq_state);
    return pending;
}

/**
 * @brief I2C 写事务（带重试）。
 *
 * @param data 发送数据描述。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_i2c_write_retry(i2c_data_t *data)
{
    errcode_t ret = ERRCODE_FAIL;

    for (uint8_t attempt = 0U; attempt < MPR121_I2C_RW_RETRY_MAX; attempt++) {
        ret = uapi_i2c_master_write(I2C_BUS_1, MPR121_I2C_ADDR, data);
        if (ret == ERRCODE_SUCC) {
            return ERRCODE_SUCC;
        }

        if ((attempt + 1U) < MPR121_I2C_RW_RETRY_MAX) {
            (void)osal_msleep(MPR121_I2C_RW_RETRY_DELAY_MS);
        }
    }

    return ret;
}

/**
 * @brief I2C 写后读事务（带重试）。
 *
 * @param data 收发数据描述。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_i2c_writeread_retry(i2c_data_t *data)
{
    errcode_t ret = ERRCODE_FAIL;

    for (uint8_t attempt = 0U; attempt < MPR121_I2C_RW_RETRY_MAX; attempt++) {
        ret = uapi_i2c_master_writeread(I2C_BUS_1, MPR121_I2C_ADDR, data);
        if (ret == ERRCODE_SUCC) {
            return ERRCODE_SUCC;
        }

        if ((attempt + 1U) < MPR121_I2C_RW_RETRY_MAX) {
            (void)osal_msleep(MPR121_I2C_RW_RETRY_DELAY_MS);
        }
    }

    return ret;
}

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

    mpr121_irq_pending_set();
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

    return mpr121_i2c_write_retry(&data);
}

/**
 * @brief 写寄存器并在失败时输出寄存器上下文，方便定位 ACK 错误位置。
 *
 * @param reg_addr 寄存器地址。
 * @param reg_val 写入值。
 * @param stage   配置阶段标识字符串。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_write_reg_checked(uint8_t reg_addr, uint8_t reg_val, const char *stage)
{
    errcode_t ret = mpr121_write_reg(reg_addr, reg_val);

    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] %s write reg 0x%02x val 0x%02x failed, ret=0x%x\r\n",
            stage,
            (unsigned int)reg_addr,
            (unsigned int)reg_val,
            (unsigned int)ret);
    }

    return ret;
}

/**
 * @brief 从指定寄存器起始地址连续读取多个字节。
 *
 * @param start_reg 起始寄存器地址。
 * @param read_buf  读取缓冲区。
 * @param read_len  读取长度。
 * @return errcode_t ERRCODE_SUCC 表示成功。
 */
static errcode_t mpr121_read_regs(uint8_t start_reg, uint8_t *read_buf, uint8_t read_len)
{
    i2c_data_t data = {0};

    if ((read_buf == NULL) || (read_len == 0U)) {
        return ERRCODE_INVALID_PARAM;
    }

    data.send_buf = &start_reg;
    data.send_len = 1U;
    data.receive_buf = read_buf;
    data.receive_len = read_len;

    return mpr121_i2c_writeread_retry(&data);
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
    uint8_t ele_idx;

    /* 软复位后等待器件稳定，再进入 stop 模式执行参数下载。 */
    ret = mpr121_write_reg_checked(MPR121_REG_SOFT_RESET, MPR121_SOFT_RESET_CMD, "reset");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    (void)osal_msleep(MPR121_SOFT_RESET_DELAY_MS);

    ret = mpr121_write_reg_checked(ELE_CFG, 0x00U, "stop");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section A: data > baseline 时的滤波参数。 */
    ret = mpr121_write_reg_checked(MHD_R, 0x01U, "A");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg_checked(NHD_R, 0x01U, "A");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg_checked(NCL_R, 0x00U, "A");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg_checked(FDL_R, 0x00U, "A");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section B: data < baseline 时的滤波参数。 */
    ret = mpr121_write_reg_checked(MHD_F, 0x01U, "B");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg_checked(NHD_F, 0x01U, "B");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg_checked(NCL_F, 0xFFU, "B");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = mpr121_write_reg_checked(FDL_F, 0x02U, "B");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /*
     * Section C: 每路电极触摸/释放阈值。
     * ELE0_T/ELE0_R 起始后按 2 字节步进连续排布，可用循环减少冗余。
     */
    for (ele_idx = 0U; ele_idx < MPR121_ELECTRODE_COUNT; ele_idx++) {
        uint8_t touch_reg = (uint8_t)(ELE0_T + (ele_idx * 2U));
        uint8_t release_reg = (uint8_t)(touch_reg + 1U);

        ret = mpr121_write_reg_checked(touch_reg, TOU_THRESH, "C");
        if (ret != ERRCODE_SUCC) {
            return ret;
        }

        ret = mpr121_write_reg_checked(release_reg, REL_THRESH, "C");
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    /* Section D: Filter Configuration。 */
    ret = mpr121_write_reg_checked(FIL_CFG, 0x04U, "D");
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Section E: 使能 12 路电极，切到 run 模式。 */
    ret = mpr121_write_reg_checked(ELE_CFG, MPR121_ELE_CFG_RUN_12, "E");
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
    uint8_t status_buf[2] = {0};

    if (touch_status == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    /*
     * 连续读 0x00/0x01，保证同一事务内获取状态快照，
     * 避免分两次读导致低/高字节跨采样窗口产生撕裂。
     */
    ret = mpr121_read_regs(MPR121_REG_TOUCH_STATUS_L, status_buf, sizeof(status_buf));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    *touch_status = (((uint16_t)status_buf[1] << 8U) | (uint16_t)status_buf[0]) & MPR121_TOUCH_VALID_MASK;
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

    /*
     * 初始化可能被重入（例如前一次 quick config 失败后重试），
     * 先清理旧的中断回调，避免 ERRCODE_GPIO_ALREADY_SET_CALLBACK。
     */
    (void)uapi_gpio_disable_interrupt(MPR121_IRQ_PIN);
    (void)uapi_gpio_unregister_isr_func(MPR121_IRQ_PIN);

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
    errcode_t cfg_ret = ERRCODE_FAIL;
    uint8_t cfg_attempt;

    uapi_gpio_init();

    ret = mpr121_i2c_bus_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] i2c bus init failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    /*
     * 先完成器件参数配置，再打开 IRQ。
     * 这样即便配置阶段失败，也不会留下已注册的 GPIO 回调，避免下次重试冲突。
     */
    for (cfg_attempt = 0U; cfg_attempt < MPR121_CFG_RETRY_MAX; cfg_attempt++) {
        cfg_ret = mpr121_quick_config();
        if (cfg_ret == ERRCODE_SUCC) {
            break;
        }

        osal_printk("[mpr121] quick config attempt %u failed, ret=0x%x\r\n",
            (unsigned int)(cfg_attempt + 1U),
            (unsigned int)cfg_ret);

        if ((cfg_ret == ERRCODE_I2C_ACK_ERR) || (cfg_ret == ERRCODE_I2C_TIMEOUT)) {
            /* ACK/超时场景主动做总线恢复，提升现场偶发 NACK 的自愈能力。 */
            (void)uapi_i2c_deinit(I2C_BUS_1);
            (void)osal_msleep(2);
            ret = uapi_i2c_master_init(I2C_BUS_1, MPR121_I2C_SPEED, MPR121_I2C_HIGH_SPEED_CODE);
            if (ret != ERRCODE_SUCC) {
                osal_printk("[mpr121] recover i2c1 failed, ret=0x%x\r\n", (unsigned int)ret);
                return ret;
            }
        }
    }

    if (cfg_ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] quick config failed, ret=0x%x\r\n", (unsigned int)cfg_ret);
        return cfg_ret;
    }

    ret = mpr121_irq_pin_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[mpr121] irq pin init failed, ret=0x%x\r\n", (unsigned int)ret);
        return ret;
    }

    /* 若上电瞬间已经有触摸，确保线程能在下一轮读取到状态。 */
    if (uapi_gpio_get_val(MPR121_IRQ_PIN) == GPIO_LEVEL_LOW) {
        mpr121_irq_pending_set();
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
        uint8_t irq_pending;

        /* IRQ 事件优先，IRQ 线低电平作为兜底，防止极端场景漏中断。 */
        irq_pending = mpr121_irq_pending_test_and_clear();
        if ((irq_pending != 0U) || (uapi_gpio_get_val(MPR121_IRQ_PIN) == GPIO_LEVEL_LOW)) {
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
    osal_kthread_lock();
    if (g_mpr121_task_handle == NULL) {
        g_mpr121_task_handle = osal_kthread_create((osal_kthread_handler)mpr121_keypad_task,
            0,
            "Mpr121Task",
            MPR121_TASK_STACK_SIZE);
        if (g_mpr121_task_handle != NULL) {
            if (osal_kthread_set_priority(g_mpr121_task_handle, MPR121_TASK_PRIO) != OSAL_SUCCESS) {
                osal_printk("[mpr121] set task priority failed\r\n");
            }
        } else {
            osal_printk("[mpr121] create task failed\r\n");
        }
    }
    osal_kthread_unlock();
}

/* 注册到系统启动流程。 */
app_run(mpr121_keypad_entry);
