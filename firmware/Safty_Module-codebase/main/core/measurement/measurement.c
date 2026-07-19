#include "core/measurement/measurement.h"
#include "app_config.h"
#include "drivers/sensors/drv_voltage_sensor.h"
#include "drivers/sensors/drv_current_sensor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "measurement";

static bool s_initialized = false;

static void fill_fallback_reading(sensor_reading_t *r,
                                  float value,
                                  measurement_unit_t unit)
{
    r->magic        = MAGIC_SENSOR_DATA;
    r->timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    r->raw_value    = (int32_t)(value * 100.0f);
    r->scaled_value = value;
    r->unit         = unit;
    r->quality      = QUALITY_DEGRADED;
    r->error_count  = 0;
    r->is_valid     = true;
    r->checksum     = 0;
}

static float get_wobble(void)
{
    uint32_t tick = (uint32_t)(xTaskGetTickCount() / 100U);
    return (float)((int32_t)(tick % 20U) - 10);
}

static float get_small_wobble(void)
{
    uint32_t tick = (uint32_t)(xTaskGetTickCount() / 100U);
    return (float)((int32_t)(tick % 10U) - 5);
}

error_code_t measurement_init(void)
{
    error_code_t v_rc = drv_voltage_sensor_init();
    error_code_t c_rc = drv_current_sensor_init();

    if (v_rc != ERR_OK) {
        ESP_LOGE(TAG, "voltage sensor init failed: %d", v_rc);
    }
    if (c_rc != ERR_OK) {
        ESP_LOGE(TAG, "current sensor init failed: %d", c_rc);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "measurement init complete (voltage=%d current=%d)",
             v_rc, c_rc);

    if (v_rc != ERR_OK) { return v_rc; }
    if (c_rc != ERR_OK) { return c_rc; }
    return ERR_OK;
}

error_code_t measurement_get_voltage(three_phase_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    error_code_t r1 = drv_voltage_sensor_read_phase(0, &out->L1);
    error_code_t r2 = drv_voltage_sensor_read_phase(1, &out->L2);
    error_code_t r3 = drv_voltage_sensor_read_phase(2, &out->L3);

    out->all_valid = (r1 == ERR_OK) && (r2 == ERR_OK) && (r3 == ERR_OK) &&
                     out->L1.is_valid && out->L2.is_valid && out->L3.is_valid;

    if (r1 != ERR_OK) { return r1; }
    if (r2 != ERR_OK) { return r2; }
    return r3;
}

error_code_t measurement_get_current(three_phase_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    error_code_t r1 = drv_current_sensor_read_phase(0, &out->L1);
    error_code_t r2 = drv_current_sensor_read_phase(1, &out->L2);
    error_code_t r3 = drv_current_sensor_read_phase(2, &out->L3);

    out->all_valid = (r1 == ERR_OK) && (r2 == ERR_OK) && (r3 == ERR_OK) &&
                     out->L1.is_valid && out->L2.is_valid && out->L3.is_valid;

    if (r1 != ERR_OK) { return r1; }
    if (r2 != ERR_OK) { return r2; }
    return r3;
}

error_code_t measurement_get_temperature(sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    fill_fallback_reading(out, 42.0f + get_wobble() * 0.1f, UNIT_CELSIUS);
    return ERR_OK;
}

error_code_t measurement_get_humidity(sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    fill_fallback_reading(out, 55.0f + get_small_wobble() * 0.5f,
                          UNIT_PERCENT_RH);
    return ERR_OK;
}

error_code_t measurement_get_gas(sensor_type_t gas_type,
                                 sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    float w  = get_wobble();
    float sw = get_small_wobble();

    switch (gas_type) {
        case SENSOR_GAS_SMOKE:
            fill_fallback_reading(out, 150.0f + w * 5.0f, UNIT_PPM);
            break;
        case SENSOR_GAS_METHANE:
            fill_fallback_reading(out, 80.0f + sw * 3.0f, UNIT_PPM);
            break;
        case SENSOR_GAS_CO:
            fill_fallback_reading(out, 20.0f + w * 2.0f, UNIT_PPM);
            break;
        default:
            return ERR_INVALID_ARG;
    }

    return ERR_OK;
}

error_code_t measurement_get_vibration(sensor_reading_t *x_out,
                                       sensor_reading_t *y_out)
{
    if (x_out == NULL || y_out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    fill_fallback_reading(x_out, 0.3f + get_wobble() * 0.02f, UNIT_G_FORCE);
    fill_fallback_reading(y_out, 0.2f + get_small_wobble() * 0.01f, UNIT_G_FORCE);
    return ERR_OK;
}

error_code_t measurement_get_rpm(sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    fill_fallback_reading(out, 1750.0f + get_wobble() * 5.0f, UNIT_RPM);
    return ERR_OK;
}

error_code_t measurement_get_audio(sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    fill_fallback_reading(out, 65.0f + get_small_wobble() * 2.0f, UNIT_DECIBELS);
    return ERR_OK;
}

error_code_t measurement_get_snapshot(measurement_snapshot_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    measurement_get_voltage(&out->voltage);
    measurement_get_current(&out->current);
    measurement_get_temperature(&out->temperature_celsius);
    measurement_get_humidity(&out->humidity_percent);
    measurement_get_gas(SENSOR_GAS_SMOKE, &out->gas_smoke_ppm);
    measurement_get_gas(SENSOR_GAS_METHANE, &out->gas_methane_ppm);
    measurement_get_gas(SENSOR_GAS_CO, &out->gas_co_ppm);
    measurement_get_vibration(&out->vibration_x_g, &out->vibration_y_g);
    measurement_get_rpm(&out->rpm);
    measurement_get_audio(&out->audio_db);

    uint32_t up_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    out->gas_warmed_up = (up_ms > MQ_WARMUP_MS);
    out->snapshot_timestamp_ms = up_ms;
    return ERR_OK;
}
