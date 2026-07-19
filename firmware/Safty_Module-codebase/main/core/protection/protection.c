// ═══ FILE: main/core/protection/protection.c ═══
/**
 * @file    protection.c
 * @brief   Protection engine — STUB implementation.
 *          Returns "all OK" for HMI development.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  CRITICAL — Stub only; replace before production.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  STUB release.
 */

#include "core/protection/protection.h"

#include "esp_log.h"

static const char *TAG = "protection";

static bool s_initialized = false;

/* STUB — returns fake data */
error_code_t protection_init(void)
{
    s_initialized = true;
    ESP_LOGW(TAG, "STUB: protection_init — all sensors report OK");
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t protection_get_alarm(sensor_type_t type,
                                  sensor_alarm_t *alarm_out)
{
    if (alarm_out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized)    { return ERR_NOT_INITIALIZED; }

    UNUSED(type);
    *alarm_out = SENSOR_ALARM_OK;
    return ERR_OK;
}

/* STUB — returns fake data */
bool protection_is_tripped(void)
{
    return false;
}

/* STUB — returns fake data */
uint32_t protection_get_active_fault_count(void)
{
    return 0;
}

/* STUB */
error_code_t protection_acknowledge_faults(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    return ERR_OK;
}