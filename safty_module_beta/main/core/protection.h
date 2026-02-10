/**
 * @file  protection.h
 * @brief Protection engine — overcurrent, overvoltage, overtemp, gas.
 * @version 1.0.0
 *
 * @safety CRITICAL
 *
 * Rule 3.11: Runs even if all other tasks are dead.
 * Rule 5.7:  Critical errors open relay immediately.
 * Rule 8.4:  Catastrophic → direct register writes via emergency_safe.
 */
#ifndef PROTECTION_H
#define PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── Catastrophic thresholds (bypass software, direct GPIO) ──────────── */
#define CATASTROPHIC_CURRENT_MULT   1.5f   /* 150% of limit */
#define CATASTROPHIC_VOLTAGE_MULT   1.2f   /* 120% of limit */
#define CATASTROPHIC_TEMP_MARGIN_C  20.0f  /* limit + 20 °C */

/* ── Per-condition status ────────────────────────────────────────────── */
typedef struct {
    bool     is_tripped;
    bool     is_warning;
    uint8_t  confirm_count;
    float    worst_value;        /* highest/lowest observed */
    float    current_value;
    float    threshold;
    float    warning_threshold;
} condition_status_t;

/* ── Full protection status snapshot ─────────────────────────────────── */
typedef struct {
    bool                is_tripped;
    error_code_t        trip_reason;
    float               trip_value;
    float               trip_threshold;
    uint32_t            trip_time_ms;
    uint32_t            total_trips;
    bool                relay_commanded;

    condition_status_t  overcurrent[3];    /* per phase */
    condition_status_t  overvoltage[3];
    condition_status_t  undervoltage[3];
    condition_status_t  overtemp;
    condition_status_t  gas;
} protection_status_t;

/**
 * @brief  Initialize protection engine with config.
 * @param  config  Protection thresholds (NULL → use defaults)
 * @return ERR_OK
 */
error_code_t protection_init(const protection_config_t *config);

/**
 * @brief  Check all protection conditions against current sensor data.
 *         Trips relay if confirmed fault. Logs fault via fault_handler.
 * @param  sensors  Current sensor snapshot
 * @return ERR_OK if no trip, or ERR_OVERCURRENT / ERR_OVERVOLTAGE / etc.
 * @wcet   <1 ms | thread-safe NO | isr-safe NO
 *
 * @note   Rule 5.7: On CRITICAL → opens relay via hal_gpio_set_relay.
 * @note   Rule 8.4: On CATASTROPHIC → calls hal_gpio_emergency_safe.
 */
error_code_t protection_check(const sensor_set_t *sensors);

/**
 * @brief  Update config (e.g. from settings menu or calibration).
 */
error_code_t protection_set_config(const protection_config_t *config);

/**
 * @brief  Get current config.
 */
error_code_t protection_get_config(protection_config_t *config);

/**
 * @brief  Get full protection status snapshot.
 */
error_code_t protection_get_status(protection_status_t *out);

/**
 * @brief  Is the protection currently tripped?
 */
bool protection_is_tripped(void);

/**
 * @brief  Reset trip after fault is cleared.
 *         Only succeeds if the fault condition is no longer present.
 * @return ERR_OK, or ERR_OVERCURRENT etc. if condition persists
 */
error_code_t protection_reset(void);

/**
 * @brief  Force trip (for testing or peer command).
 */
error_code_t protection_force_trip(error_code_t reason);

#endif /* PROTECTION_H */