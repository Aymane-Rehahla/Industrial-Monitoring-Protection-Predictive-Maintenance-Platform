#include "drivers/sensors/drv_voltage_sensor.h"
#include "drivers/sensors/drv_ads1115.h"
#include "app_config.h"
#include "utils/crc_utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <math.h>
#include <stddef.h>

static const char *TAG = "drv_voltage";

#define AC_SAMPLE_COUNT 64U

static bool s_initialized = false;
static uint8_t s_errors[3] = {0};

static void finish_reading(sensor_reading_t *out,
                           int32_t raw,
                           float value,
                           data_quality_t quality,
                           uint8_t errors)
{
    out->magic        = MAGIC_SENSOR_DATA;
    out->timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    out->raw_value    = raw;
    out->scaled_value = value;
    out->unit         = UNIT_VOLTS;
    out->quality      = quality;
    out->error_count  = errors;
    out->is_valid     = (quality != QUALITY_INVALID);
    out->checksum     = 0;
    out->checksum = crc16_calc((const uint8_t *)out,
                               offsetof(sensor_reading_t, checksum));
}

error_code_t drv_voltage_sensor_init(void)
{
    error_code_t rc = drv_ads1115_init(I2C_ADDR_ADS1115_VOLTAGE);
    if (rc != ERR_OK) { return rc; }

    s_initialized = true;
    ESP_LOGI(TAG, "ZMPT voltage sensor using ADS1115 0x%02X AIN0-AIN2",
             I2C_ADDR_ADS1115_VOLTAGE);
    return ERR_OK;
}

error_code_t drv_voltage_sensor_read_phase(uint8_t phase,
                                           sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (phase >= 3U) { return ERR_INVALID_ARG; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    double sum_sq = 0.0;
    int32_t raw_sum = 0;

    for (uint32_t i = 0; i < AC_SAMPLE_COUNT; i++) {
        float mv = 0.0f;
        error_code_t rc = drv_ads1115_read_millivolts(I2C_ADDR_ADS1115_VOLTAGE,
                                                      phase, &mv);
        if (rc != ERR_OK) {
            if (s_errors[phase] < UINT8_MAX) { s_errors[phase]++; }
            finish_reading(out, 0, 0.0f, QUALITY_INVALID, s_errors[phase]);
            return rc;
        }

        float centered_mv = mv - ZMPT101B_OFFSET_MV;
        sum_sq += (double)centered_mv * (double)centered_mv;
        raw_sum += (int32_t)mv;
    }

    float sensor_rms_mv = (float)sqrt(sum_sq / (double)AC_SAMPLE_COUNT);
    float line_voltage_v = (sensor_rms_mv / 1000.0f) * ZMPT101B_RATIO;
    int32_t raw_avg_mv = raw_sum / (int32_t)AC_SAMPLE_COUNT;

    s_errors[phase] = 0;
    finish_reading(out, raw_avg_mv, line_voltage_v, QUALITY_GOOD, 0);
    return ERR_OK;
}
