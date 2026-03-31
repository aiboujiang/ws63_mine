/**
 * @file ld2402.h
 * @brief LD2402 protocol parsing.
 */
#ifndef LD2402_H
#define LD2402_H
#include "errcode.h"
#include <stdint.h>

void ld2402_init(void);
void ld2402_process_data(const uint8_t *data, uint16_t len);

#endif
