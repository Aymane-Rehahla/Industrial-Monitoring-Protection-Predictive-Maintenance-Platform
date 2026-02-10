/**
 * @file drv_mq_gas.c
 * @brief MQ gas sensor implementation
 * @version 1.0.0
 * 
 * @safety MEDIUM
 */

#include "drv_mq_gas.h"
#include "hal_adc.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "DRV_MQ";

/* ── ADC channel mapping ─────────────────────────────────────────────── */
static const int GAS_ADC_CHANNELS[GAS_SENSOR_COUNT] = {
    ADC_CHANNEL_MQ2,
    ADC_CHANNEL_MQ4,
    ADC_CHANNEL_MQ9
};

static const char *GAS_NAMES[GAS_SENSOR_COUNT] = {
    "MQ-2 (Smoke)",
    "MQ-4 (Methane)",
    "MQ-9 (CO)"
};

/* ── Module state ────────────────────────────────────────────────────── */
static uint32_t s_init_time_ms = 0;
static uint16_t s_thresholds[GAS_SENSOR_COUNT];
static bool s_initialized = false;

/* ── Public: Init ────────────────────────────────────────────────────── */
error_code_t mq_gas_init(void)
{
    ESP_LOGI(TAG, "Initializing gas sensors...");
    
    s_init_time_ms = get_time_ms();
    
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        s_thresholds[i] = MQ_DEFAULT_THRESHOLD;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Gas sensors initialized (warmup: %d ms)", MQ_WARMUP_MS);
    return ERR_OK;
}

/* ── Public: Read single sensor ──────────────────────────────────────── */
error_code_t mq_gas_read(gas_sensor_id_t sensor, gas_reading_t *reading)
{
    if (reading == NULL) { return ERR_NULL_POINTER; }
    if (sensor >= GAS_SENSOR_COUNT) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    
    memset(reading, 0, sizeof(*reading));
    reading->timestamp_ms = get_time_ms();
    reading->quality = QUALITY_INVALID;
    reading->is_warmed_up = mq_gas_is_warmed_up();
    
    /* Read ADC */
    adc_reading_t adc;
    error_code_t err = hal_adc_read_filtered(GAS_ADC_CHANNELS[sensor], &adc);
    
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "%s read failed: %d", GAS_NAMES[sensor], err);
        return err;
    }
    
    reading->raw_adc = adc.raw;
    reading->compensated_mv = (int32_t)(adc.mv * VDIV_MULTIPLY);
    
    /* Calculate percentage of ADC range */
    reading->level_pct = (uint16_t)((adc.raw * 100) / ADC_MAX_RAW);
    
    reading->is_valid = true;
    reading->quality = reading->is_warmed_up ? QUALITY_GOOD : QUALITY_DEGRADED;
    
    return ERR_OK;
}

/* ── Public: Read all ────────────────────────────────────────────────── */
error_code_t mq_gas_read_all(gas_reading_t readings[GAS_SENSOR_COUNT])
{
    if (readings == NULL) { return ERR_NULL_POINTER; }
    
    error_code_t first_err = ERR_OK;
    
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        error_code_t err = mq_gas_read((gas_sensor_id_t)i, &readings[i]);
        if (err != ERR_OK && first_err == ERR_OK) {
            first_err = err;
        }
    }
    
    return first_err;
}

/* ── Public: Warmup check ────────────────────────────────────────────── */
bool mq_gas_is_warmed_up(void)
{
    if (!s_initialized) { return false; }
    return (get_time_ms() - s_init_time_ms) >= MQ_WARMUP_MS;
}

uint32_t mq_gas_warmup_remaining_ms(void)
{
    if (!s_initialized) { return MQ_WARMUP_MS; }
    
    uint32_t elapsed = get_time_ms() - s_init_time_ms;
    if (elapsed >= MQ_WARMUP_MS) { return 0; }
    return MQ_WARMUP_MS - elapsed;
}

/* ── Public: Threshold management ────────────────────────────────────── */
error_code_t mq_gas_set_threshold(gas_sensor_id_t sensor, uint16_t threshold)
{
    if (sensor >= GAS_SENSOR_COUNT) { return ERR_INVALID_PARAMETER; }
    s_thresholds[sensor] = threshold;
    ESP_LOGI(TAG, "%s threshold set to %d", GAS_NAMES[sensor], threshold);
    return ERR_OK;
}

uint16_t mq_gas_get_threshold(gas_sensor_id_t sensor)
{
    if (sensor >= GAS_SENSOR_COUNT) { return 0; }
    return s_thresholds[sensor];
}

/* ── Public: Alarm check ─────────────────────────────────────────────── */
bool mq_gas_check_alarm(gas_sensor_id_t *which_sensor)
{
    if (!s_initialized || !mq_gas_is_warmed_up()) { return false; }
    
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        gas_reading_t r;
        if (mq_gas_read((gas_sensor_id_t)i, &r) == ERR_OK && r.is_valid) {
            if (r.raw_adc > s_thresholds[i]) {
                if (which_sensor != NULL) {
                    *which_sensor = (gas_sensor_id_t)i;
                }
                ESP_LOGW(TAG, "%s ALARM: %ld > %d",
                         GAS_NAMES[i], (long)r.raw_adc, s_thresholds[i]);
                return true;
            }
        }
    }
    
    return false;
}