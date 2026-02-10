/**
 * @file drv_mq_gas.h
 * @brief MQ-series gas sensor driver (MQ-2, MQ-4, MQ-9)
 * @version 1.0.0
 * 
 * @safety MEDIUM
 * @hardware MQ sensors → voltage divider → ESP32 internal ADC
 */

#ifndef DRV_MQ_GAS_H
#define DRV_MQ_GAS_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── Gas reading structure ───────────────────────────────────────────── */
typedef struct {
    int32_t  raw_adc;           /* Raw ADC value 0-4095 */
    int32_t  compensated_mv;    /* After divider compensation */
    uint16_t level_pct;         /* Relative level 0-100% of range */
    uint32_t timestamp_ms;
    bool     is_valid;
    bool     is_warmed_up;
    data_quality_t quality;
} gas_reading_t;

/**
 * @brief Initialize all gas sensors
 * @return ERR_OK
 */
error_code_t mq_gas_init(void);

/**
 * @brief Read specific gas sensor
 * @param sensor GAS_SENSOR_MQ2, MQ4, or MQ9
 * @param reading Output structure
 * @return ERR_OK or error
 */
error_code_t mq_gas_read(gas_sensor_id_t sensor, gas_reading_t *reading);

/**
 * @brief Read all gas sensors
 */
error_code_t mq_gas_read_all(gas_reading_t readings[GAS_SENSOR_COUNT]);

/**
 * @brief Check if sensors are warmed up (30+ seconds)
 */
bool mq_gas_is_warmed_up(void);

/**
 * @brief Get warmup time remaining in ms (0 if ready)
 */
uint32_t mq_gas_warmup_remaining_ms(void);

/**
 * @brief Set alarm threshold for a sensor
 */
error_code_t mq_gas_set_threshold(gas_sensor_id_t sensor, uint16_t threshold);

/**
 * @brief Get alarm threshold
 */
uint16_t mq_gas_get_threshold(gas_sensor_id_t sensor);

/**
 * @brief Check if any sensor exceeds threshold
 * @param which_sensor Output: which sensor triggered (can be NULL)
 * @return true if alarm condition
 */
bool mq_gas_check_alarm(gas_sensor_id_t *which_sensor);

#endif /* DRV_MQ_GAS_H */