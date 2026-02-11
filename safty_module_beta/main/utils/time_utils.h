/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  time_utils.h - Unified Time Functions                                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Replaces 9 duplicate get_time_ms() implementations                          ║
 * ║  Safety Level: LOW                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h"

/**
 * @brief Get current time in milliseconds
 * @return Milliseconds since boot (wraps at ~49 days)
 */
static inline uint32_t time_get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Get current time in microseconds
 * @return Microseconds since boot
 */
static inline int64_t time_get_us(void)
{
    return esp_timer_get_time();
}

/**
 * @brief Calculate elapsed time (handles 32-bit rollover)
 * @param start_ms Start timestamp from time_get_ms()
 * @return Elapsed milliseconds
 */
static inline uint32_t time_elapsed_ms(uint32_t start_ms)
{
    return time_get_ms() - start_ms;
}

/**
 * @brief Check if timeout has expired
 * @param start_ms Start timestamp
 * @param timeout_ms Timeout duration
 * @return true if expired
 */
static inline bool time_has_expired(uint32_t start_ms, uint32_t timeout_ms)
{
    return time_elapsed_ms(start_ms) >= timeout_ms;
}

#endif /* TIME_UTILS_H */