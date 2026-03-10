/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "bsp/bsp_ina219.h"
#include "bsp/bsp_init.h"
#include "bsp/sle_server.h"
#include "bsp/hal_bsp_ssd1306.h"
#include "bsp/hal_bsp_dc.h"
#include "bsp/track.h"
#include "bsp/pid.h"
#include "bsp/remoteInfrared.h"
#include "sle_remote_server.h"
#include "app_init.h"
#include "watchdog.h"
#include "stdio.h"
#include "string.h"

#include "bsp/hal_bsp_aw2013.h"
#include "bsp/hal_bsp_ap3216.h"

#include "lvgl.h"
#include "bsp/hal_bsp_nfc.h"
#include "bsp/hal_bsp_sht20.h"

#define SEND_CAR_INFO "{CAR_POWER:%.1f,CAR_SPEED:%d,CAR_CONN:%d}\n"
void nfc_read(void);
/* 外设初始化 */
void my_peripheral_init(void)
{
    user_beep_init();

    INA219_Init();
    PCF8574_Init();
    uapi_watchdog_disable();
    user_pwm_init();
    car_io_init();
    user_key_init();
    PID_Init();
    motor_enc_init();
    app_uart_init_config();
}

void car_state_report(void)
{
    uint8_t send_buf[100] = {0};
    msg_data_t msg_data = {0};
    memset(send_buf, 0, sizeof(send_buf));
    sprintf((char *)send_buf, SEND_CAR_INFO, (float)(systemValue.battery_voltage) / 1000.0, systemValue.car_speed,
            g_sle_conn_flag);
    msg_data.value = (uint8_t *)send_buf;
    msg_data.value_len = strlen((char *)send_buf);
    sle_server_send_report_by_handle(msg_data);
}
void *bsp_init(const char *arg)
{
    unused(arg);
    osal_msleep(500); /* 500: 延时500ms */
    sle_server_init();
    while (1) {
        if (systemValue.car_speed != systemValue.last_speed) {
            systemValue.last_speed = systemValue.car_speed;
            target_speed = 0;
            change_flag = 0;
        }
        INA219_get_bus_voltage_mv(&systemValue.battery_voltage);
        oled_show();
        car_state_report();
        osal_msleep(500); /* 500: 延时500ms */
    }
}

void *sle_control_task(const char *arg)
{
    unused(arg);

    user_ir_init();
    ir_timer_init();
    pid_timer_init();
    while (1) {
        printf("systemValue.R_enc:%d  %d\n", systemValue.L_enc, systemValue.R_enc);
        printf("return :%d %d\n", L_Motor_PWM, R_Motor_PWM);
        Remote_Infrared_KeyDeCode();
        osal_msleep(100);
    }
}
uint16_t ir, als, ps; // 人体红外传感器 接近传感器 光照强度传感器
void main_task(void *argument)
{
    unused(argument);
    user_led_init();
    user_beep_init();
    user_key_init();
    uapi_watchdog_disable();
    osal_msleep(100);
    nfc_read();
    AP3216C_Init();
    AW2013_Init(); // 三色LED灯的初始化
    AW2013_Control_RGB(0xff, 0, 0);
    osal_msleep(200);
    AW2013_Control_RGB(0, 0xff, 0);
    osal_msleep(200);
    AW2013_Control_RGB(0, 0, 0xff);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_HIGH);
    uapi_gpio_set_val(USER_LED, GPIO_LEVEL_HIGH);
    osal_msleep(200);
    uapi_gpio_set_val(USER_BEEP, GPIO_LEVEL_LOW);
    AW2013_Control_RGB(0, 0, 0);
    app_spi_init_pin();
    app_spi_master_init_config();
    ILI9341_Init();
    LCD_ShowString(60, 0, strlen("TOUCH TEST"), 24, (uint8_t *)"TOUCH TEST");
    osDelay(50);
    FT6336_init();
    while (1) {
        AP3216C_ReadData(&ir, &als, &ps);
        SHT20_ReadData(&temperature, &humidity);
        oled_show();
        osDelay(100);
    }
}
uint32_t ALL_FLAG;
void selec_test_mode(const char *arg)
{
    unused(arg);
    uapi_pin_set_mode(GPIO_00 | GPIO_01, HAL_PIO_FUNC_GPIO);
    uapi_gpio_set_dir(GPIO_00 | GPIO_01, GPIO_DIRECTION_OUTPUT);
    uapi_gpio_set_val(GPIO_00 | GPIO_01, GPIO_LEVEL_LOW);

    // uapi_pin_set_mode(GPIO_01, HAL_PIO_FUNC_GPIO);
    // uapi_gpio_set_dir(GPIO_01, GPIO_DIRECTION_OUTPUT);
    // uapi_gpio_set_val(GPIO_01, GPIO_LEVEL_LOW);
    uint32_t result;
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    SSD1306_Init(); // OLED 显示屏初始化
    SSD1306_CLS();  // 清屏

    result = PCF8574_Write(tmp_io.all);
    if (result != ERRCODE_SUCC) {
        ALL_FLAG = 0; // 开发板测试

    } else {
        ALL_FLAG = 1; // 小车测试
    }

    osal_mdelay(500);
    if (ALL_FLAG) {
        my_peripheral_init();
        task_handle = osal_kthread_create((osal_kthread_handler)bsp_init, 0, "bsp_init", SLE_SERVER_STACK_SIZE);
        if (task_handle != NULL) {
            osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
            osal_kfree(task_handle);
        }

        task_handle =
            osal_kthread_create((osal_kthread_handler)sle_control_task, 0, "sle_control_task", SLE_SERVER_STACK_SIZE);
        if (task_handle != NULL) {
            osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
            osal_kfree(task_handle);
        }
    } else {
        task_handle = osal_kthread_create((osal_kthread_handler)main_task, 0, "main_task", SLE_SERVER_STACK_SIZE);
        if (task_handle != NULL) {
            osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
            osal_kfree(task_handle);
        }
    }

    osal_kthread_unlock();
}

static void sle_server_entry(void)
{

    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle =
        osal_kthread_create((osal_kthread_handler)selec_test_mode, 0, "selec_test_mode", SLE_SERVER_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, SLE_SERVER_TASK_PRIO);
        osal_kfree(task_handle);
    }

    osal_kthread_unlock();
}

/* Run the sle_entry. */
app_run(sle_server_entry);
uint8_t nfc_test;
void nfc_read(void)
{
    uint8_t ndefLen = 0;     // ndef包的长度
    uint8_t ndef_Header = 0; // ndef消息开始标志位-用不到
    size_t i = 0;
    // 读整个数据的包头部分，读出整个数据的长度
    if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) != true) {
        printf("NT3HReadHeaderNfc is failed.\r\n");
        return;
    }

    ndefLen += NDEF_HEADER_SIZE; // 加上头部字节
    if (ndefLen <= NDEF_HEADER_SIZE) {
        printf("ndefLen <= 2\r\n");
        return;
    }
    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("ndefBuff malloc is Falied!\r\n");
        return;
    }

    if (get_NDEFDataPackage(ndefBuff, ndefLen) != ERRCODE_SUCC) {
        printf("get_NDEFDataPackage is failed. \r\n");
        return;
    }
    nfc_test = 1;
    printf("start print ndefBuff.\r\n");
    for (i = 0; i < ndefLen; i++) {
        printf("0x%x ", ndefBuff[i]);
    }
}