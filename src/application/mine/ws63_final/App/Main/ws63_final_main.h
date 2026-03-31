/**
 * @file ws63_final_main.h
 * @brief WK2114 最终版应用主入口接口。
 */

#ifndef MINE_WS63_FINAL_MAIN_H
#define MINE_WS63_FINAL_MAIN_H

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
errcode_t mine_ws63_final_start(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif
