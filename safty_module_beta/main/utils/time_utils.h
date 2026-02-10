/**
 * @file time_utils.h
 * @brief Single definition of get_time_ms. BUG 26 fix.
 */
#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>
#include "esp_timer.h"

static inline uint32_t get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

#endif /* TIME_UTILS_H */