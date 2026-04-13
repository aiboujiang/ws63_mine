/**
 * @file wk_zw101_test.h
 * @brief WK2114 子串口1挂载 ZW101 的独立测试程序接口。
 *
 * 说明：
 * 1) 本模块仅用于 ZW101 联调测试，不承载门锁主业务逻辑；
 * 2) 命令输入口固定为 UART0，数据链路固定为 WK2114 子串口1；
 * 3) 对外仅暴露 init/process 两个生命周期接口，便于任务层循环调用。
 */

#ifndef WK_ZW101_TEST_H
#define WK_ZW101_TEST_H

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 初始化 ZW101 测试模块。
 *
 * @return errcode_t ERRCODE_SUCC 初始化完成（设备可后续重探测），其他值表示初始化失败。
 */
errcode_t wk_zw101_test_init(void);

/**
 * @brief ZW101 测试模块周期处理函数。
 *
 * 任务主循环中周期调用，用于：
 * 1) 轮询子串口1接收并喂入协议解析；
 * 2) 解析 UART0 文本命令；
 * 3) 设备未就绪时按间隔重探测。
 */
void wk_zw101_test_process(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* WK_ZW101_TEST_H */
