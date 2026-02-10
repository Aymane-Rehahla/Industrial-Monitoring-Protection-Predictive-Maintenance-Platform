/**
 * @file  sensor_manager.h
 * @brief Centralized sensor reading, caching, and calibration.
 * @version 1.0.0
 *
 * @safety HIGH
 *
 * Rule 4.8: All readings include quality indicators.
 * Rule 5.13: Sensor failure → last-good with QUALITY_DEGRADED.
 */
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── Calibration statistics (per measurand) ──────────────────────────── */
typedef struct {
    float   min_val;
    float   max_val;
    double  sum;
    uint32_t count;
} cal_stat_t;

/* ── Calibration results (suggested thresholds) ──────────────────────── */
typedef struct {
    float    suggested_overcurrent_A;
    float    suggested_overvoltage_V;
    float    suggested_undervoltage_V;
    float    suggested_overtemp_C;
    uint16_t suggested_gas_threshold;

    float    observed_voltage_max_V;
    float    observed_voltage_min_V;
    float    observed_current_max_A;
    float    observed_temp_max_C;
    uint16_t observed_gas_max;

    uint32_t total_samples;
    uint32_t duration_ms;
    bool     is_valid;
} calibration_results_t;

/* ── Calibration session ─────────────────────────────────────────────── */
#define CALIBRATION_DEFAULT_DURATION_MS  (24UL * 3600UL * 1000UL)
#define CALIBRATION_MIN_DURATION_MS      (60UL * 1000UL)
#define CALIBRATION_MARGIN_PCT           10

typedef struct {
    bool     is_active;
    bool     is_complete;
    uint32_t start_time_ms;
    uint32_t target_duration_ms;
    uint32_t sample_count;

    cal_stat_t voltage[3];
    cal_stat_t current[3];
    cal_stat_t temp_machine;
    cal_stat_t temp_ambient;
    cal_stat_t humidity;
    cal_stat_t gas[3];            /* GAS_SENSOR_COUNT */

    calibration_results_t results;
} calibration_session_t;

/**
 * @brief  Initialize all sensor drivers.
 * @return ERR_OK (non-fatal if individual sensors offline)
 * @wcet   500 ms | thread-safe NO | isr-safe NO
 */
error_code_t sensor_mgr_init(void);

/**
 * @brief  Read all sensors and update cached data.
 * @return ERR_OK
 * @wcet   200 ms | thread-safe NO | isr-safe NO
 */
error_code_t sensor_mgr_read_all(void);

/**
 * @brief  Get thread-safe copy of cached sensor data.
 * @param  out  Destination (must not be NULL)
 * @return ERR_OK, ERR_NULL_POINTER
 * @wcet   <10 µs | thread-safe YES (mutex) | isr-safe NO
 */
error_code_t sensor_mgr_get_data(sensor_set_t *out);

/**
 * @brief  Start calibration session.
 * @param  duration_ms  Learning window (min 60 s, default 24 h)
 * @return ERR_OK, ERR_INVALID_PARAMETER
 */
error_code_t sensor_mgr_start_calibration(uint32_t duration_ms);

/**
 * @brief  Stop calibration and compute suggestions.
 * @return ERR_OK
 */
error_code_t sensor_mgr_stop_calibration(void);

/**
 * @brief  Get calibration progress 0-100%.
 */
uint8_t sensor_mgr_get_calibration_progress(void);

/**
 * @brief  Get calibration results (valid only after stop).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED
 */
error_code_t sensor_mgr_get_calibration_results(calibration_results_t *out);

/**
 * @brief  Is calibration currently running?
 */
bool sensor_mgr_is_calibrating(void);

/**
 * @brief  Self-test all sensors.
 * @return ERR_OK or first error
 */
error_code_t sensor_mgr_self_test(void);

#endif /* SENSOR_MANAGER_H */