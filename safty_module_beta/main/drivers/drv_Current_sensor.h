/**
 * @file drv_current_sensor.h
 * @brief ACS758-50A current sensor driver (via ADS1115)
 * @version 1.0.0
 * 
 * @safety CRITICAL
 * @hardware ACS758 → voltage divider → ADS1115 channel
 * 
 * ACS758-50A: 40mV/A, 2.5V at 0A
 * Through 10k+10k divider: 1.25V at 0A, 20mV/A effective
 */

#ifndef DRV_CURRENT_SENSOR_H
#define DRV_CURRENT_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"
#include "drv_ads1115.h"

/* ── Current reading with full metadata ──────────────────────────────── */
typedef struct {
    int32_t  raw_mv;            /* mV from ADS1115 */
    int32_t  compensated_mv;    /* After divider compensation */
    float    current_ma;        /* Calculated current in mA */
    float    current_a;         /* Current in Amps */
    uint32_t timestamp_ms;
    bool     is_valid;
    data_quality_t quality;
    uint8_t  channel;
} current_reading_t;

/* ── Calibration data ────────────────────────────────────────────────── */
typedef struct {
    int32_t  zero_mv;           /* mV reading at 0A (after divider comp) */
    float    sensitivity_mv_a;  /* mV per Amp (original, before divider) */
    float    divider_ratio;     /* Voltage divider compensation */
    uint32_t calibration_date;
    bool     is_calibrated;
} current_calibration_t;

/**
 * @brief Initialize current sensor system
 * @param ads_handle Pointer to initialized ADS1115 handle for current
 * @return ERR_OK or error
 */
error_code_t current_sensor_init(ads1115_handle_t *ads_handle);

/**
 * @brief Read current from specific phase
 * @param phase 0=L1, 1=L2, 2=L3
 * @param reading Output reading structure
 * @return ERR_OK or error
 * @wcet 20ms | thread-safe NO | isr-safe NO
 */
error_code_t current_sensor_read(uint8_t phase, current_reading_t *reading);

/**
 * @brief Read all three phases
 */
error_code_t current_sensor_read_all(current_reading_t readings[3]);

/**
 * @brief Set calibration for a phase
 */
error_code_t current_sensor_set_calibration(uint8_t phase,
                                             const current_calibration_t *cal);

/**
 * @brief Get current calibration
 */
error_code_t current_sensor_get_calibration(uint8_t phase,
                                             current_calibration_t *cal);

/**
 * @brief Auto-calibrate zero (call with no current flowing)
 */
error_code_t current_sensor_calibrate_zero(uint8_t phase);

/**
 * @brief Check if sensor is online
 */
bool current_sensor_is_online(void);

#endif /* DRV_CURRENT_SENSOR_H */