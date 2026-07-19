#include "drivers/sensors/drv_current_sensor.h"
#include "drivers/sensors/drv_ads1115.h"
#include "app_config.h"
#include "utils/crc_utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <math.h>
#include <stddef.h>

static const char *TAG = "drv_current";

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
    out->unit         = UNIT_AMPS;
    out->quality      = quality;
    out->error_count  = errors;
    out->is_valid     = (quality != QUALITY_INVALID);
    out->checksum     = 0;
    out->checksum = crc16_calc((const uint8_t *)out,
                               offsetof(sensor_reading_t, checksum));
}

error_code_t drv_current_sensor_init(void)
{
    error_code_t rc = drv_ads1115_init(I2C_ADDR_ADS1115_CURRENT);
    if (rc != ERR_OK) { return rc; }

    s_initialized = true;
    ESP_LOGI(TAG, "ACS current sensor using ADS1115 0x%02X AIN0-AIN2",
             I2C_ADDR_ADS1115_CURRENT);
    return ERR_OK;
}

error_code_t drv_current_sensor_read_phase(uint8_t phase,
                                           sensor_reading_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (phase >= 3U) { return ERR_INVALID_ARG; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    double sum_sq = 0.0;
    int32_t raw_sum = 0;

    for (uint32_t i = 0; i < AC_SAMPLE_COUNT; i++) {
        float mv = 0.0f;
        error_code_t rc = drv_ads1115_read_millivolts(I2C_ADDR_ADS1115_CURRENT,
                                                      phase, &mv);
        if (rc != ERR_OK) {
            if (s_errors[phase] < UINT8_MAX) { s_errors[phase]++; }
            finish_reading(out, 0, 0.0f, QUALITY_INVALID, s_errors[phase]);
            return rc;
        }

        float centered_mv = mv - ACS758_ZERO_CURRENT_MV;
        sum_sq += (double)centered_mv * (double)centered_mv;
        raw_sum += (int32_t)mv;
    }

    float rms_mv = (float)sqrt(sum_sq / (double)AC_SAMPLE_COUNT);
    float current_a = rms_mv / ACS758_SENSITIVITY_MV_PER_A;
    int32_t raw_avg_mv = raw_sum / (int32_t)AC_SAMPLE_COUNT;

    s_errors[phase] = 0;
    finish_reading(out, raw_avg_mv, current_a, QUALITY_GOOD, 0);
    return ERR_OK;
}
