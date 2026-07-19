#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h"

static inline uint32_t time_get_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static inline bool time_expired(uint32_t start_ms, uint32_t timeout_ms) {
    return (time_get_ms() - start_ms) >= timeout_ms;
}

#endif