// ═══ FILE: main/hal/hal_adc.c ═══
/**
 * @file    hal_adc.c
 * @brief   ADC HAL — STUB IMPLEMENTATION.
 *
 *          ╔══════════════════════════════════════════════════════════╗
 *          ║  WARNING: THIS IS A STUB.  ALL READINGS ARE FAKE.      ║
 *          ║  Raw returns 2048 (mid-scale).  mV returns 1650.       ║
 *          ║  This exists ONLY so upper layers compile and run       ║
 *          ║  with predictable sensor data during development.       ║
 *          ║  Replace with real ADC driver before field deployment.  ║
 *          ╚══════════════════════════════════════════════════════════╝
 *
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Must be replaced before production use.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Stub release — returns fake mid-scale data.
 */

#include "hal/hal_adc.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "hal_adc";

/* Justification: Tracks initialisation state. Must persist across calls. */
static bool s_adc_initialized = false;

/* Stub mid-scale values */
#define STUB_RAW_VALUE   2048    /* Mid-scale for 12-bit ADC           */
#define STUB_MV_VALUE    1650    /* Mid-scale for 0–3300 mV range      */

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_adc_init  (STUB)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_adc_init(void)
{
    ESP_LOGW(TAG, "STUB: ADC init — no real hardware configured");
    s_adc_initialized = true;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_adc_read_raw  (STUB)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_adc_read_raw(uint8_t gpio_num, int32_t *raw_out)
{
    UNUSED(gpio_num);

    if (raw_out == NULL) {
        return ERR_NULL_POINTER;
    }

    *raw_out = STUB_RAW_VALUE;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_adc_read_millivolts  (STUB)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_adc_read_millivolts(uint8_t gpio_num, int32_t *mv_out)
{
    UNUSED(gpio_num);

    if (mv_out == NULL) {
        return ERR_NULL_POINTER;
    }

    *mv_out = STUB_MV_VALUE;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_adc_is_initialized  (STUB)
 * ═══════════════════════════════════════════════════════════════════════ */
bool hal_adc_is_initialized(void)
{
    return s_adc_initialized;
}