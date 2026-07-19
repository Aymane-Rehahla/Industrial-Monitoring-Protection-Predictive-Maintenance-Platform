// ═══ FILE: main/core/sensor_manager/sensor_manager.c ═══
/**
 * @file    sensor_manager.c
 * @brief   Sensor manager — STUB implementation with default sensor registry.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — Stub only; replace before production.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  STUB release.
 */

#include "core/sensor_manager/sensor_manager.h"
#include "app_config.h"

#include "esp_log.h"

#include <string.h>

static const char *TAG = "sensor_manager";

#define NO_GPIO  0xFF
#define NO_I2C   0xFF

/* Justification: Sensor registry must persist across all calls.
 * Modified by add/remove/enable.  File scope. */
static sensor_entry_t s_registry[SENSOR_REGISTRY_MAX];
static uint32_t       s_count       = 0;
static bool           s_initialized = false;

/**
 * @brief  Add one entry to the registry during init.
 *
 * WHY a helper: The default sensor table has 15 entries.  Without
 * this helper, init() would exceed the 50-line limit.
 */
static void reg_add(sensor_type_t type, const char *name,
                    uint8_t gpio, uint8_t bus, uint8_t addr,
                    bool removable)
{
    if (s_count >= SENSOR_REGISTRY_MAX) { return; }

    sensor_entry_t *e = &s_registry[s_count];
    e->type        = type;
    strncpy(e->name, name, SENSOR_NAME_MAX_LEN - 1);
    e->name[SENSOR_NAME_MAX_LEN - 1] = '\0';
    e->gpio_pin    = gpio;
    e->i2c_bus     = bus;
    e->i2c_addr    = addr;
    e->is_enabled  = true;
    e->is_removable = removable;
    e->is_online   = true;  /* STUB: all online */
    s_count++;
}

/* STUB — returns fake data */
error_code_t sensor_manager_init(void)
{
    s_count = 0;

    /* Three-phase voltage via ADS1115 #1 on I2C Bus 0 */
    reg_add(SENSOR_VOLTAGE, "Voltage L1", NO_GPIO, 0, 0x48, false);
    reg_add(SENSOR_VOLTAGE, "Voltage L2", NO_GPIO, 0, 0x48, false);
    reg_add(SENSOR_VOLTAGE, "Voltage L3", NO_GPIO, 0, 0x48, false);

    /* Three-phase current via ADS1115 #2 on I2C Bus 0 */
    reg_add(SENSOR_CURRENT, "Current L1", NO_GPIO, 0, 0x49, false);
    reg_add(SENSOR_CURRENT, "Current L2", NO_GPIO, 0, 0x49, false);
    reg_add(SENSOR_CURRENT, "Current L3", NO_GPIO, 0, 0x49, false);

    /* Environmental sensors on I2C Bus 1 */
    reg_add(SENSOR_TEMP,     "Temperature", NO_GPIO, 1, 0x44, true);
    reg_add(SENSOR_HUMIDITY,  "Humidity",    NO_GPIO, 1, 0x44, true);

    /* Gas sensors via ADC */
    reg_add(SENSOR_GAS_SMOKE,   "MQ-2 Smoke",   1, NO_I2C, 0, true);
    reg_add(SENSOR_GAS_METHANE, "MQ-4 Methane",  2, NO_I2C, 0, true);
    reg_add(SENSOR_GAS_CO,      "MQ-9 CO",       4, NO_I2C, 0, true);

    /* Vibration via ADC */
    reg_add(SENSOR_VIBRATION, "Vibration X", 5, NO_I2C, 0, false);
    reg_add(SENSOR_VIBRATION, "Vibration Y", 6, NO_I2C, 0, false);

    /* Hall-effect RPM */
    reg_add(SENSOR_RPM, "RPM", 41, NO_I2C, 0, false);

    /* Audio level via ADC */
    reg_add(SENSOR_AUDIO, "Audio Level", 7, NO_I2C, 0, false);

    s_initialized = true;
    ESP_LOGW(TAG, "STUB: sensor_manager_init — %lu sensors registered",
             (unsigned long)s_count);
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t sensor_manager_get_sensor_count(uint32_t *count_out)
{
    if (count_out == NULL) { return ERR_NULL_POINTER; }
    *count_out = s_count;
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t sensor_manager_get_sensor(uint32_t index,
                                       sensor_entry_t *entry_out)
{
    if (entry_out == NULL) { return ERR_NULL_POINTER; }
    if (index >= s_count)  { return ERR_INVALID_ARG; }

    *entry_out = s_registry[index];
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t sensor_manager_find_by_type(sensor_type_t type,
                                         sensor_entry_t *entry_out)
{
    if (entry_out == NULL) { return ERR_NULL_POINTER; }

    for (uint32_t i = 0; i < s_count && i < SENSOR_REGISTRY_MAX; i++) {
        if (s_registry[i].type == type) {
            *entry_out = s_registry[i];
            return ERR_OK;
        }
    }
    return ERR_NOT_FOUND;
}

/* STUB — appends to registry */
error_code_t sensor_manager_add_sensor(const sensor_entry_t *entry)
{
    if (entry == NULL) { return ERR_NULL_POINTER; }
    if (s_count >= SENSOR_REGISTRY_MAX) { return ERR_BUFFER_FULL; }

    s_registry[s_count] = *entry;
    s_count++;
    ESP_LOGI(TAG, "STUB: sensor added — %s (total %lu)",
             entry->name, (unsigned long)s_count);
    return ERR_OK;
}

/* STUB — removes from registry */
error_code_t sensor_manager_remove_sensor(uint32_t index)
{
    if (index >= s_count) { return ERR_INVALID_ARG; }
    if (!s_registry[index].is_removable) { return ERR_INVALID_ARG; }

    /* Shift remaining entries down. Bounded by SENSOR_REGISTRY_MAX. */
    for (uint32_t i = index; i < s_count - 1 && i < SENSOR_REGISTRY_MAX - 1; i++) {
        s_registry[i] = s_registry[i + 1];
    }
    s_count--;
    return ERR_OK;
}

/* STUB */
error_code_t sensor_manager_enable_sensor(uint32_t index, bool enable)
{
    if (index >= s_count) { return ERR_INVALID_ARG; }
    s_registry[index].is_enabled = enable;
    return ERR_OK;
}

/* STUB — returns fake data */
bool sensor_manager_is_sensor_online(sensor_type_t type)
{
    for (uint32_t i = 0; i < s_count && i < SENSOR_REGISTRY_MAX; i++) {
        if (s_registry[i].type == type && s_registry[i].is_online) {
            return true;
        }
    }
    return false;
}

const char *sensor_manager_get_type_name(sensor_type_t type)
{
    switch (type) {
        case SENSOR_NONE:        return "None";
        case SENSOR_VOLTAGE:     return "Voltage";
        case SENSOR_CURRENT:     return "Current";
        case SENSOR_TEMP:        return "Temperature";
        case SENSOR_HUMIDITY:    return "Humidity";
        case SENSOR_GAS_SMOKE:   return "Gas (Smoke)";
        case SENSOR_GAS_METHANE: return "Gas (Methane)";
        case SENSOR_GAS_CO:      return "Gas (CO)";
        case SENSOR_VIBRATION:   return "Vibration";
        case SENSOR_RPM:         return "RPM";
        case SENSOR_AUDIO:       return "Audio";
        default:                 return "Unknown";
    }
}

/* STUB — returns hardcoded spare pins */
error_code_t sensor_manager_get_available_pins(sensor_type_t type,
                                               pin_suggestion_t *pins_out,
                                               uint32_t max_pins,
                                               uint32_t *count_out)
{
    if (pins_out == NULL || count_out == NULL) { return ERR_NULL_POINTER; }

    UNUSED(type);

    /* Spare GPIO pins from the pin map. */
    const pin_suggestion_t spares[] = {
        { .gpio_pin = 38, .is_adc_capable = false, .is_in_use = false },
        { .gpio_pin = 39, .is_adc_capable = false, .is_in_use = false }
    };

    uint32_t avail = ARRAY_SIZE(spares);
    if (avail > max_pins) { avail = max_pins; }

    for (uint32_t i = 0; i < avail; i++) {
        pins_out[i] = spares[i];
    }

    *count_out = avail;
    return ERR_OK;
}