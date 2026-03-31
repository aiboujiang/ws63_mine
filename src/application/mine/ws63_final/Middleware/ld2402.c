/**
 * @file ld2402.c
 * @brief LD2402 radar data logic (business only).
 */
#include "ld2402.h"
#include "osal_debug.h"

/**
 * @brief Initialize ld2402 parsing logic.
 */
void ld2402_init(void)
{
    osal_printk("ld2402 initialized.\n");
}

/**
 * @brief Process ld2402 raw data without knowing hardware specifics.
 */
void ld2402_process_data(const uint8_t *data, uint16_t len)
{
    if (len > 0) {
        osal_printk("LD2402 processing %u bytes.\n", (unsigned int)len);
        /* logic to parse ld2402 radar packet */
    }
}
