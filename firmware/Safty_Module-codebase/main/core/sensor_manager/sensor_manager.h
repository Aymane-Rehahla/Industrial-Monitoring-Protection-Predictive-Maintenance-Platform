// ═══ FILE: main/core/sensor_manager/sensor_manager.h ═══
/**
 * @file    sensor_manager.h
 * @brief   Sensor registry — tracks which sensors are connected, enabled,
 *          removable.  Provides pin advisor for adding new sensors.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — Registry integrity affects protection coverage.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release — API definition.
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/** Maximum length of a sensor name string including null terminator. */
#define SENSOR_NAME_MAX_LEN   16

/** Maximum sensors in the registry. */
#define SENSOR_REGISTRY_MAX   20

/**
 * @brief  Single sensor registry entry.
 */
typedef struct {
    sensor_type_t type;
    char          name[SENSOR_NAME_MAX_LEN];
    uint8_t       gpio_pin;    /* Primary GPIO (0xFF if not GPIO-based)  */
    uint8_t       i2c_bus;     /* I2C bus index (0xFF if not I2C)        */
    uint8_t       i2c_addr;    /* I2C address (0x00 if not I2C)          */
    bool          is_enabled;
    bool          is_removable;/* false for fixed sensors (V, I)         */
    bool          is_online;   /* Detected and responding                */
} sensor_entry_t;

/**
 * @brief  Available GPIO pin suggestion for adding new sensors.
 */
typedef struct {
    uint8_t gpio_pin;
    bool    is_adc_capable;
    bool    is_in_use;
} pin_suggestion_t;

/** @brief Initialise sensor registry with default sensor set. */
error_code_t sensor_manager_init(void);

/** @brief Get number of registered sensors. */
error_code_t sensor_manager_get_sensor_count(uint32_t *count_out);

/** @brief Get sensor entry by index. */
error_code_t sensor_manager_get_sensor(uint32_t index,
                                       sensor_entry_t *entry_out);

/** @brief Find first sensor of a given type. */
error_code_t sensor_manager_find_by_type(sensor_type_t type,
                                         sensor_entry_t *entry_out);

/** @brief Add a new sensor to the registry. */
error_code_t sensor_manager_add_sensor(const sensor_entry_t *entry);

/** @brief Remove a sensor (must be removable). */
error_code_t sensor_manager_remove_sensor(uint32_t index);

/** @brief Enable or disable a sensor. */
error_code_t sensor_manager_enable_sensor(uint32_t index, bool enable);

/** @brief Check if a sensor type is currently online. */
bool sensor_manager_is_sensor_online(sensor_type_t type);

/** @brief Get human-readable name for a sensor type. */
const char *sensor_manager_get_type_name(sensor_type_t type);

/** @brief Get list of available GPIO pins for a sensor type. */
error_code_t sensor_manager_get_available_pins(sensor_type_t type,
                                               pin_suggestion_t *pins_out,
                                               uint32_t max_pins,
                                               uint32_t *count_out);

#endif /* SENSOR_MANAGER_H */