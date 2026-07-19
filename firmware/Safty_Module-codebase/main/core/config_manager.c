// ═══ FILE: main/core/config_manager.c ═══
/**
 * @file    config_manager.c
 * @brief   Config manager — STUB implementation with default thresholds.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Stub only; replace before production.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  STUB release.
 */

#include "core/config_manager.h"
#include "app_config.h"

#include "esp_log.h"

static const char *TAG = "config_manager";

/* Justification: Active protection config must persist across calls.
 * Static file scope — read by protection engine, written by HMI. */
static protection_config_t s_config;
static bool s_initialized = false;

/**
 * @brief  Apply factory default thresholds.
 *
 * WHY these values: Conservative defaults chosen to protect typical
 * 3-phase 220 V / 50 A industrial motor without nuisance trips.
 */
static void apply_defaults(void)
{
    s_config.magic = MAGIC_CONFIG_DATA;

    s_config.voltage = (sensor_threshold_t){
        .high_limit = 260.0f, .low_limit = 180.0f,
        .hysteresis = 5.0f,   .high_enabled = true, .low_enabled = true
    };
    s_config.current = (sensor_threshold_t){
        .high_limit = 45.0f,  .low_limit = 0.0f,
        .hysteresis = 2.0f,   .high_enabled = true, .low_enabled = false
    };
    s_config.temp = (sensor_threshold_t){
        .high_limit = 80.0f,  .low_limit = -10.0f,
        .hysteresis = 3.0f,   .high_enabled = true, .low_enabled = true
    };
    s_config.gas = (sensor_threshold_t){
        .high_limit = 2500.0f,.low_limit = 0.0f,
        .hysteresis = 100.0f, .high_enabled = true, .low_enabled = false
    };
    s_config.vibration = (sensor_threshold_t){
        .high_limit = 5.0f,   .low_limit = 0.0f,
        .hysteresis = 0.5f,   .high_enabled = true, .low_enabled = false
    };
    s_config.rpm = (sensor_threshold_t){
        .high_limit = 3600.0f,.low_limit = 100.0f,
        .hysteresis = 50.0f,  .high_enabled = true, .low_enabled = true
    };

    s_config.checksum = 0; /* STUB: no real CRC */
}

/* STUB — returns fake data */
error_code_t config_manager_init(void)
{
    apply_defaults();
    s_initialized = true;
    ESP_LOGW(TAG, "STUB: config_manager_init — using default thresholds");
    return ERR_OK;
}

/**
 * @brief  Map sensor_type_t to the corresponding threshold field.
 * @return Pointer to threshold inside s_config, or NULL if unmapped.
 */
static sensor_threshold_t *get_threshold_ptr(sensor_type_t type)
{
    switch (type) {
        case SENSOR_VOLTAGE:     return &s_config.voltage;
        case SENSOR_CURRENT:     return &s_config.current;
        case SENSOR_TEMP:        return &s_config.temp;
        case SENSOR_GAS_SMOKE:   return &s_config.gas;
        case SENSOR_GAS_METHANE: return &s_config.gas;
        case SENSOR_GAS_CO:      return &s_config.gas;
        case SENSOR_VIBRATION:   return &s_config.vibration;
        case SENSOR_RPM:         return &s_config.rpm;
        default:                 return NULL;
    }
}

/* STUB — returns fake data */
error_code_t config_manager_get_threshold(sensor_type_t type,
                                          sensor_threshold_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    const sensor_threshold_t *ptr = get_threshold_ptr(type);
    if (ptr == NULL) { return ERR_INVALID_ARG; }

    *out = *ptr;
    return ERR_OK;
}

/* STUB — stores but does not persist */
error_code_t config_manager_set_threshold(sensor_type_t type,
                                          const sensor_threshold_t *value)
{
    if (value == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    sensor_threshold_t *ptr = get_threshold_ptr(type);
    if (ptr == NULL) { return ERR_INVALID_ARG; }

    *ptr = *value;
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t config_manager_get_config(protection_config_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    *out = s_config;
    return ERR_OK;
}

/* STUB — no-op */
error_code_t config_manager_save(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    ESP_LOGW(TAG, "STUB: config_manager_save — no NVS write");
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t config_manager_load_defaults(void)
{
    apply_defaults();
    ESP_LOGI(TAG, "STUB: defaults reloaded");
    return ERR_OK;
}