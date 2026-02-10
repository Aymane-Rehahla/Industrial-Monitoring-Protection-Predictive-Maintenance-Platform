/**
 * @file drv_sht45.h
 * @brief SHT45 temperature/humidity sensor driver
 * @version 1.0.0
 * 
 * @safety MEDIUM
 * @hardware SHT45 on I2C Bus 1 (shared with LCD), address 0x44
 */

#ifndef DRV_SHT45_H
#define DRV_SHT45_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── Reading structure ───────────────────────────────────────────────── */
typedef struct {
    float    temperature_c;
    float    humidity_pct;
    uint32_t timestamp_ms;
    bool     is_valid;
    bool     crc_ok;
    data_quality_t quality;
} sht45_reading_t;

/**
 * @brief Initialize SHT45 sensor
 * @return ERR_OK, ERR_I2C_NACK, ERR_SENSOR_OFFLINE
 */
error_code_t sht45_init(void);

/**
 * @brief Read temperature and humidity
 * @param reading Output structure
 * @return ERR_OK or error
 * @wcet 15ms | thread-safe NO | isr-safe NO
 */
error_code_t sht45_read(sht45_reading_t *reading);

/**
 * @brief Soft reset the sensor
 * @return ERR_OK or error
 */
error_code_t sht45_reset(void);

/**
 * @brief Check if sensor is online
 */
bool sht45_is_online(void);

/**
 * @brief Self-test
 */
error_code_t sht45_self_test(void);

#endif /* DRV_SHT45_H */