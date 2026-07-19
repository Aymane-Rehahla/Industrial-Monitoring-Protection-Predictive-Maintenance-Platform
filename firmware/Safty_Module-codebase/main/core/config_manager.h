// ═══ FILE: main/core/config_manager.h ═══
/**
 * @file    config_manager.h
 * @brief   Protection thresholds and system configuration manager.
 *          Loaded from NVS on boot, editable via HMI threshold screen.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Thresholds determine when machine is tripped.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release — API definition.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Complete set of protection thresholds with integrity fields.
 */
typedef struct {
    uint16_t           magic;      /* MAGIC_CONFIG_DATA (0xC0FD)          */
    sensor_threshold_t voltage;    /* Volts                               */
    sensor_threshold_t current;    /* Amps                                */
    sensor_threshold_t temp;       /* Celsius                             */
    sensor_threshold_t gas;        /* PPM                                 */
    sensor_threshold_t vibration;  /* g-force                             */
    sensor_threshold_t rpm;        /* RPM                                 */
    uint16_t           checksum;   /* CRC-16 of preceding fields          */
} protection_config_t;

/** @brief Initialise config manager, load from NVS or apply defaults. */
error_code_t config_manager_init(void);

/** @brief Get threshold for a specific sensor type. */
error_code_t config_manager_get_threshold(sensor_type_t type,
                                          sensor_threshold_t *out);

/** @brief Set threshold for a specific sensor type. */
error_code_t config_manager_set_threshold(sensor_type_t type,
                                          const sensor_threshold_t *value);

/** @brief Get complete protection configuration. */
error_code_t config_manager_get_config(protection_config_t *out);

/** @brief Save current configuration to NVS. */
error_code_t config_manager_save(void);

/** @brief Reset all thresholds to factory defaults. */
error_code_t config_manager_load_defaults(void);

#endif /* CONFIG_MANAGER_H */