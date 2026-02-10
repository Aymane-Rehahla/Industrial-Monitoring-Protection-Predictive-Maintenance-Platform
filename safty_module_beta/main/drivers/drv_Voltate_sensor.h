/**
 * @file drv_voltage_sensor.h
 * @brief ZMPT101B voltage sensor driver (via ADS1115)
 * @version 1.0.0
 * 
 * @safety HIGH
 * @hardware ZMPT101B → voltage divider → ADS1115 channel
 * 
 * The ZMPT101B outputs 0-5V AC waveform proportional to input voltage.
 * Through 10k+10k divider: 0-2.5V to ADS1115.
 * This driver reads the ADS1115 and scales to actual AC voltage.
 */

#ifndef DRV_VOLTAGE_SENSOR_H
#define DRV_VOLTAGE_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"
#include "drv_ads1115.h"

/* ── Voltage reading with full metadata ──────────────────────────────── */
typedef struct {
    int32_t  raw_mv;            /* mV from ADS1115 */
    int32_t  compensated_mv;    /* After divider compensation */
    float    voltage_rms_v;     /* Calculated RMS voltage */
    uint32_t timestamp_ms;
    bool     is_valid;
    data_quality_t quality;
    uint8_t  channel;
} voltage_reading_t;

/* ── Calibration data ────────────────────────────────────────────────── */
typedef struct {
    float    scale_factor;      /* ADC mV to actual V */
    int32_t  offset_mv;         /* Zero offset correction */
    float    divider_ratio;     /* Voltage divider compensation */
    uint32_t calibration_date;  /* Unix timestamp */
    bool     is_calibrated;
} voltage_calibration_t;

/**
 * @brief Initialize voltage sensor system
 * @param ads_handle Pointer to initialized ADS1115 handle
 * @return ERR_OK or error
 */
error_code_t voltage_sensor_init(ads1115_handle_t *ads_handle);

/**
 * @brief Read voltage from specific phase
 * @param phase 0=L1, 1=L2, 2=L3
 * @param reading Output reading structure
 * @return ERR_OK or error
 * @wcet 20ms | thread-safe NO | isr-safe NO
 */
error_code_t voltage_sensor_read(uint8_t phase, voltage_reading_t *reading);

/**
 * @brief Read all three phases
 * @param readings Array of 3 readings
 * @return ERR_OK or first error
 */
error_code_t voltage_sensor_read_all(voltage_reading_t readings[3]);

/**
 * @brief Set calibration for a phase
 * @param phase 0-2
 * @param cal Calibration data
 * @return ERR_OK or error
 */
error_code_t voltage_sensor_set_calibration(uint8_t phase, 
                                             const voltage_calibration_t *cal);

/**
 * @brief Get current calibration
 */
error_code_t voltage_sensor_get_calibration(uint8_t phase,
                                             voltage_calibration_t *cal);

/**
 * @brief Auto-calibrate zero offset (call with no input voltage)
 * @param phase 0-2
 * @return ERR_OK or error
 */
error_code_t voltage_sensor_calibrate_zero(uint8_t phase);

/**
 * @brief Check if sensor is online
 */
bool voltage_sensor_is_online(void);

#endif /* DRV_VOLTAGE_SENSOR_H */