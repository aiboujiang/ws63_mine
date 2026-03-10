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

#include "stdio.h"
#include "string.h"
#include "soc_osal.h"
#include "securec.h"
#include "osal_debug.h"
#include "cmsis_os2.h"
#include "lvgl.h"
#include "app_init.h"
osThreadId_t Task1_ID; // 任务1设置为低优先级任务

//注意：修改\src\open_source\lvgl\src\BSP\bsp_ili9341_4line.c横竖平配置为竖屏

#define TASK_DELAY_TIME 100
void Task1(void)
{
    osDelay(TASK_DELAY_TIME);
    // 要显示的字符串
    char str[] = "1234567890*#&| ABCD abcd !";
    // SPI引脚初始化
    app_spi_init_pin();
    // SPI配置初始化
    app_spi_master_init_config();
    printf("Enter ILI9341_Init()!\n");
    // 屏幕配置初始化
    ILI9341_Init();
    // 显示字符串
    LCD_ShowString(0, 24, strlen(str), 24, (uint8_t *)str);
    osDelay(TASK_DELAY_TIME);
    // 显示图片
    LCD_DrawPicture(0, 0, 240, 320, (uint8_t *)gImage_1);
    osDelay(TASK_DELAY_TIME);
    ILI9341_Clear(WHITE);
    while (1) {
        LCD_DrawPicture(20, 0, 220, 97, (uint8_t *)lcd_test);
        osDelay(TASK_DELAY_TIME);
        // 清屏
        ILI9341_Clear(WHITE);
        osDelay(TASK_DELAY_TIME);
    }
}
static void base_lcd_demo(void)
{
    printf("Enter base_lcd_demo()!\r\n");

    osThreadAttr_t attr;
    attr.name = "Task1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x3000;
    attr.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &attr);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
}
app_run(base_lcd_demo);