/**
 * @file zw101.c
 * @brief ZW101 fingerprint logic (business only).
 */
#include "zw101.h"
#include "osal_debug.h"

/**
 * @brief Initialize zw101 module parsing logic.
 */
void zw101_init(void)
{
    osal_printk("zw101 logic initialized.\n");
}

/**
 * @brief Process zw101 raw data logic.
 */
void zw101_process_data(const uint8_t *data, uint16_t len)
{
    if (len > 0) {
        osal_printk("ZW101 processing %u bytes.\n", (unsigned int)len);
        /* logic to parse zw101 fingerprint packet */
    }
}
