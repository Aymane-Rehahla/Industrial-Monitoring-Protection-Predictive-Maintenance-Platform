/**
 * @file drv_ads1115.h
 * @brief ADS1115 16-bit ADC driver for voltage/current sensing
 * @version 1.0.0
 * @date 2025
 * 
 * @safety MEDIUM
 * @hardware ADS1115 on I2C Bus 0 (addresses 0x48, 0x49)
 * 
 * Rule 6.9: Hardware dependencies documented
 * - ADS1115 #1 (0x48): ZMPT101B voltage sensors (3 channels)
 * - ADS1115 #2 (0x49): ACS758 current sensors (3 channels)
 */

#ifndef DRV_ADS1115_H
#define DRV_ADS1115_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── Gain settings ───────────────────────────────────────────────────── */
typedef enum {
    ADS_GAIN_6144MV = 0,    /* ±6.144V, LSB = 187.5µV */
    ADS_GAIN_4096MV = 1,    /* ±4.096V, LSB = 125µV   */
    ADS_GAIN_2048MV = 2,    /* ±2.048V, LSB = 62.5µV  */
    ADS_GAIN_1024MV = 3,    /* ±1.024V, LSB = 31.25µV */
    ADS_GAIN_512MV  = 4,    /* ±0.512V, LSB = 15.625µV*/
    ADS_GAIN_256MV  = 5,    /* ±0.256V, LSB = 7.8125µV*/
} ads_gain_t;

/* ── Sample rate settings ────────────────────────────────────────────── */
typedef enum {
    ADS_RATE_8SPS   = 0,
    ADS_RATE_16SPS  = 1,
    ADS_RATE_32SPS  = 2,
    ADS_RATE_64SPS  = 3,
    ADS_RATE_128SPS = 4,    /* Default */
    ADS_RATE_250SPS = 5,
    ADS_RATE_475SPS = 6,
    ADS_RATE_860SPS = 7,
} ads_rate_t;

/* ── Device handle ───────────────────────────────────────────────────── */
typedef struct {
    uint8_t     i2c_addr;
    ads_gain_t  gain;
    ads_rate_t  rate;
    bool        is_online;
    uint32_t    read_count;
    uint32_t    error_count;
    uint32_t    last_read_ms;
} ads1115_handle_t;

/* ── Channel reading ─────────────────────────────────────────────────── */
typedef struct {
    int16_t  raw;               /* Raw 16-bit signed value */
    int32_t  millivolts;        /* Converted to mV */
    uint32_t timestamp_ms;
    bool     is_valid;
    data_quality_t quality;
} ads_reading_t;

/**
 * @brief Initialize ADS1115 device
 * @param handle Pointer to handle (must not be NULL)
 * @param i2c_addr I2C address (0x48 or 0x49)
 * @param gain Initial gain setting
 * @return ERR_OK, ERR_NULL_POINTER, ERR_I2C_NACK, ERR_SENSOR_OFFLINE
 * @wcet 50ms | thread-safe NO | isr-safe NO
 */
error_code_t ads1115_init(ads1115_handle_t *handle, uint8_t i2c_addr, 
                          ads_gain_t gain);

/**
 * @brief Read single channel (blocking, single-shot mode)
 * @param handle Device handle
 * @param channel Channel 0-3
 * @param reading Output reading structure
 * @return ERR_OK, ERR_NULL_POINTER, ERR_SENSOR_OFFLINE, ERR_I2C_TIMEOUT
 * @wcet 15ms at 128SPS | thread-safe NO | isr-safe NO
 */
error_code_t ads1115_read_channel(ads1115_handle_t *handle, uint8_t channel,
                                   ads_reading_t *reading);

/**
 * @brief Read all 4 channels sequentially
 * @param handle Device handle
 * @param readings Array of 4 readings
 * @return ERR_OK or first error encountered
 * @wcet 60ms | thread-safe NO | isr-safe NO
 */
error_code_t ads1115_read_all_channels(ads1115_handle_t *handle,
                                        ads_reading_t readings[4]);

/**
 * @brief Set gain (affects all subsequent reads)
 * @return ERR_OK or ERR_NULL_POINTER
 */
error_code_t ads1115_set_gain(ads1115_handle_t *handle, ads_gain_t gain);

/**
 * @brief Set sample rate
 * @return ERR_OK or ERR_NULL_POINTER
 */
error_code_t ads1115_set_rate(ads1115_handle_t *handle, ads_rate_t rate);

/**
 * @brief Check if device is responding
 * @return true if online
 */
bool ads1115_is_online(const ads1115_handle_t *handle);

/**
 * @brief Convert raw to millivolts based on current gain
 */
int32_t ads1115_raw_to_mv(const ads1115_handle_t *handle, int16_t raw);

/**
 * @brief Self-test: verify device responds and reads plausible values
 * @return ERR_OK or ERR_SENSOR_INVALID
 */
error_code_t ads1115_self_test(ads1115_handle_t *handle);

#endif /* DRV_ADS1115_H */