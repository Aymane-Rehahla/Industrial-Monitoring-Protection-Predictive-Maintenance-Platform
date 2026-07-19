// ═══ FILE: main/core/measurement/measurement.h ═══
/**
 * @file    measurement.h
 * @brief   Provides latest sensor readings to all consumers (HMI, protection).
 *          Real implementation reads from sensor drivers and applies filtering.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  CRITICAL — Protection engine bases trip decisions on these readings.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release — API definition.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Three-phase reading bundle (voltage or current).
 */
typedef struct {
    sensor_reading_t L1;
    sensor_reading_t L2;
    sensor_reading_t L3;
    bool             all_valid;
} three_phase_reading_t;

/**
 * @brief  Complete snapshot of all sensor readings at a point in time.
 */
typedef struct {
    three_phase_reading_t voltage;              /* Volts             */
    three_phase_reading_t current;              /* Amps              */
    sensor_reading_t      temperature_celsius;
    sensor_reading_t      humidity_percent;
    sensor_reading_t      gas_smoke_ppm;        /* MQ-2              */
    sensor_reading_t      gas_methane_ppm;      /* MQ-4              */
    sensor_reading_t      gas_co_ppm;           /* MQ-9              */
    sensor_reading_t      vibration_x_g;
    sensor_reading_t      vibration_y_g;
    sensor_reading_t      rpm;
    sensor_reading_t      audio_db;
    bool                  gas_warmed_up;        /* MQ warmup done?   */
    uint32_t              snapshot_timestamp_ms;
} measurement_snapshot_t;

/** @brief Initialise measurement subsystem. */
error_code_t measurement_init(void);

/** @brief Get complete snapshot of all sensors. */
error_code_t measurement_get_snapshot(measurement_snapshot_t *out);

/** @brief Get three-phase voltage readings. */
error_code_t measurement_get_voltage(three_phase_reading_t *out);

/** @brief Get three-phase current readings. */
error_code_t measurement_get_current(three_phase_reading_t *out);

/** @brief Get temperature reading. */
error_code_t measurement_get_temperature(sensor_reading_t *out);

/** @brief Get humidity reading. */
error_code_t measurement_get_humidity(sensor_reading_t *out);

/** @brief Get gas sensor reading by type (SENSOR_GAS_SMOKE/METHANE/CO). */
error_code_t measurement_get_gas(sensor_type_t gas_type,
                                 sensor_reading_t *out);

/** @brief Get vibration readings (X and Y axes). */
error_code_t measurement_get_vibration(sensor_reading_t *x_out,
                                       sensor_reading_t *y_out);

/** @brief Get RPM reading. */
error_code_t measurement_get_rpm(sensor_reading_t *out);

/** @brief Get audio level reading. */
error_code_t measurement_get_audio(sensor_reading_t *out);

#endif /* MEASUREMENT_H */