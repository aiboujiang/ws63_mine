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

#include "soc_osal.h"
#include "app_init.h"
#include "cmsis_os2.h"
#include "platform_core_rom.h"
#include "watchdog.h"
#include "timer.h"
#include "chip_core_irq.h"

#include <stdio.h>
#include "lvgl.h"                // 它为整个LVGL提供了更完整的头文件引用

osThreadId_t task1_ID; // 任务1

#define RELOAD_CNT 1000
#define TIMER_IRQ_PRIO 3 /* 中断优先级范围，从高到低: 0~7 */

timer_handle_t timer1_handle = 0;
void timer1_callback(uintptr_t data)
{
    unused(data);
    lv_tick_inc(1);
    /* 开启下一次timer中断 */
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}
void lvgl_timer_init(void)
{
    uapi_timer_init();
    /* 设置 timer1 硬件初始化，设置中断号，配置优先级 */
    uapi_timer_adapter(TIMER_INDEX_1, TIMER_1_IRQN, TIMER_IRQ_PRIO);
    /* 创建 timer1 软件定时器控制句柄 */
    uapi_timer_create(TIMER_INDEX_1, &timer1_handle);
    uapi_timer_start(timer1_handle, RELOAD_CNT, timer1_callback, 0);
}

/**
 * @brief 按钮事件回调函数
 * @param e LVGL事件对象指针，包含事件相关信息
 */
static void btn_event_cb(lv_event_t * e)
{
    /* 从事件对象中获取事件类型代码 */
    lv_event_code_t code = lv_event_get_code(e);
    
    /* 获取触发此事件的目标对象 */
    lv_obj_t * btn = lv_event_get_target(e);
    
    /* 检查事件类型是否为按钮点击事件 */
    if(code == LV_EVENT_CLICKED) {
        /* 静态变量，用于记录按钮点击次数，在函数调用之间保持值不变 */
        static uint8_t cnt = 0;
        cnt++;

        /* 获取按钮的第一个子对象，在LVGL中，按钮通常包含一个标签作为其子对象来显示文字 */
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        
        /* 使用格式化文本更新标签显示内容，显示当前点击次数 */
        lv_label_set_text_fmt(label, "Button: %d", cnt);
    }
}

static int lvgl_task(const char *arg)
{
    unused(arg);
    uint16_t msLVGL;
    uapi_watchdog_disable();
    
    lv_init();   
   
    lv_port_disp_init();                   // 注册LVGL的显示任务
	lv_port_indev_init();                  // 注册LVGL的触屏检测任务
  
	// 按钮
	lv_obj_t *myBtn = lv_btn_create(lv_scr_act());       // 创建按钮; 父对象：当前活动屏
    lv_obj_set_size(myBtn, 120, 50);                     // 设置大小
    lv_obj_center(myBtn);                                // 对齐于：父对象
    lv_obj_add_event_cb(myBtn, btn_event_cb, LV_EVENT_ALL, NULL); //为按钮添加回调

	// 按钮上的文本
	lv_obj_t *label_btn = lv_label_create(myBtn);   // 创建文本标签，父对象：上面的btn按钮
	lv_obj_center(label_btn);    // 对齐于：父对象
	lv_label_set_text(label_btn, "Button");        // 设置标签的文本
  

    lvgl_timer_init();
    while (1)
    {
		msLVGL = lv_timer_handler();
        osal_mdelay(msLVGL);
    }

    return 0;
}

static void lvgl_key_entry(void)
{
    printf("Enter lvgl_key_entry()!\r\n");

    osThreadAttr_t attr;
    attr.name = "lvgl_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 0x2000;
    attr.priority = osPriorityNormal;

    task1_ID = osThreadNew((osThreadFunc_t)lvgl_task, NULL, &attr);
    if (task1_ID != NULL)
    {
        printf("ID = %d, Create Task1_ID is OK!\r\n", task1_ID);
    }
}

/* Run the sample. */
app_run(lvgl_key_entry);