/**
 * @file ws63_final_main.c
 * @brief WK2114 最终版应用主入口实现。
 *
 * 说明：
 * 1) 主控固定为 WS63；
 * 2) WK2114 作为外设扩展芯片，由 Driver 层统一管理；
 * 3) 应用层只负责启动任务，不直接做硬件访问。
 */

#include "ws63_final_main.h"

#include "app_init.h"
#include "osal_debug.h"

#include "ws63_final_config.h"
#include "ws63_final_osal.h"
#include "ws63_final_task.h"

/**
 * @brief 启动 WK2114 最终版业务任务。
 */
errcode_t ws63_start(void)
{
    errcode_t ret;

    ret = ws63_os_start_task("ws63_final_task",
        ws63_task_entry,
        0,
        WS63_TASK_STACK_SIZE,
        WS63_TASK_PRIORITY);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[wk2114 final main] start task fail\r\n");
        return ret;
    }

    osal_printk("[wk2114 final main] start task ok\r\n");
    return ERRCODE_SUCC;
}

#if (WS63_AUTO_RUN == 1)
/**
 * @brief 系统启动自动入口。
 */
static void ws63_app_entry(void)
{
    (void)ws63_start();
}
app_run(ws63_app_entry);
#endif
