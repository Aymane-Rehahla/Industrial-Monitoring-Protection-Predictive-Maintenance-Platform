/**
 * @file drv_hall_rpm.h
 * @brief SS41F Hall sensor driver for RPM measurement
 * @version 1.0.0
 * 
 * @safety LOW
 * @hardware SS41F on GPIO6 (digital input with interrupt)
 */

#ifndef DRV_HALL_RPM_H
#define DRV_HALL_RPM_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── RPM reading structure ───────────────────────────────────────────── */
typedef struct {
    float    rpm;
    uint32_t pulse_count;
    uint32_t pulse_delta;       /* Pulses since last read */
    uint32_t period_us;         /* Time between pulses */
    uint32_t timestamp_ms;
    bool     is_valid;
    bool     is_rotating;       /* True if recent pulses detected */
} hall_reading_t;

/**
 * @brief Initialize Hall sensor with interrupt
 * @return ERR_OK or error
 */
error_code_t hall_rpm_init(void);

/**
 * @brief Read current RPM
 * @param reading Output structure
 * @return ERR_OK
 */
error_code_t hall_rpm_read(hall_reading_t *reading);

/**
 * @brief Get total pulse count
 */
uint32_t hall_rpm_get_pulses(void);

/**
 * @brief Reset pulse counter
 */
void hall_rpm_reset_counter(void);

/**
 * @brief Check if rotation detected recently
 */
bool hall_rpm_is_rotating(void);

#endif /* DRV_HALL_RPM_H */