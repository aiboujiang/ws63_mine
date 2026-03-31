/**
 * @file zw101.h
 * @brief ZW101 fingerprint protocol parsing.
 */
#ifndef ZW101_H
#define ZW101_H
#include "errcode.h"
#include <stdint.h>

void zw101_init(void);
void zw101_process_data(const uint8_t *data, uint16_t len);

#endif
