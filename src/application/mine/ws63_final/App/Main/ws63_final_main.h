/**
 * @file ws63_final_main.h
 * @brief WK2114 最终版应用主入口接口。
 */

#ifndef WS63_MAIN_H
#define WS63_MAIN_H

#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/**
 * @brief 启动 WK2114 最终版业务任务。
 *
 * @return errcode_t ERRCODE_SUCC 成功，其他失败。
 */
errcode_t ws63_start(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
