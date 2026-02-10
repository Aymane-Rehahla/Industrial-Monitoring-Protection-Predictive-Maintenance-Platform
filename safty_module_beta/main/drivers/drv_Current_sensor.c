/**
 * @file drv_current_sensor.c
 * @brief ACS758-50A current sensor implementation
 * @version 1.0.0
 * 
 * @safety CRITICAL
 * 
 * Rule 2.10: Rate-of-change checks
 * Rule 5.7: Critical errors open all relays
 */

#include "drv_current_sensor.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"

#include <string.h>
#include <math.h>

static const char *TAG = "DRV_CURR";

/* ── Module state ────────────────────────────────────────────────────── */
static ads1115_handle_t *s_ads = NULL;
static current_calibration_t s_cal[3];
static int32_t s_last_mv[3] = {0, 0, 0};
static uint32_t s_last_time[3] = {0, 0, 0};
static bool s_initialized = false;

/* ── Constants ───────────────────────────────────────────────────────── */
#define MAX_RATE_MA_PER_SEC  100000  /* 100A/s max rate */

/* ── Default calibration ─────────────────────────────────────────────── */
static void set_default_calibration(current_calibration_t *cal)
{
    cal->zero_mv          = (int32_t)ACS_ZERO_MV_DIVIDED;
    cal->sensitivity_mv_a = ACS_SENS_MV_PER_A_DIV;
    cal->divider_ratio    = VDIV_MULTIPLY;
    cal->calibration_date = 0;
    cal->is_calibrated    = false;
}

/* ── Public: Init ────────────────────────────────────────────────────── */
error_code_t current_sensor_init(ads1115_handle_t *ads_handle)
{
    if (ads_handle == NULL) { return ERR_NULL_POINTER; }
    
    s_ads = ads_handle;
    
    for (int i = 0; i < 3; i++) {
        set_default_calibration(&s_cal[i]);
        s_last_mv[i] = 0;
        s_last_time[i] = 0;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Current sensor initialized (zero=%ld mV, sens=%.1f mV/A)",
             (long)s_cal[0].zero_mv, s_cal[0].sensitivity_mv_a);
    return ERR_OK;
}

/* ── Public: Read single phase ───────────────────────────────────────── */
error_code_t current_sensor_read(uint8_t phase, current_reading_t *reading)
{
    if (reading == NULL) { return ERR_NULL_POINTER; }
    if (phase > 2) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized || s_ads == NULL) { return ERR_NOT_INITIALIZED; }
    
    /* Initialize output */
    memset(reading, 0, sizeof(*reading));
    reading->channel      = phase;
    reading->timestamp_ms = get_time_ms();
    reading->quality      = QUALITY_INVALID;
    
    /* Read from ADS1115 */
    ads_reading_t adc;
    error_code_t err = ads1115_read_channel(s_ads, phase, &adc);
    
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "Phase %d read failed: %d", phase, err);
        return err;
    }
    
    reading->raw_mv = adc.millivolts;
    
    /* Compensate for voltage divider */
    reading->compensated_mv = (int32_t)(adc.millivolts * s_cal[phase].divider_ratio);
    
    /* Calculate current: I = (V - Vzero) / sensitivity */
    int32_t deviation = reading->compensated_mv - s_cal[phase].zero_mv;
    reading->current_ma = (float)deviation / s_cal[phase].sensitivity_mv_a * 1000.0f;
    reading->current_a  = reading->current_ma / 1000.0f;
    
    /* Rate of change check (Rule 2.10) */
    uint32_t now = get_time_ms();
    if (s_last_time[phase] > 0) {
        uint32_t dt = now - s_last_time[phase];
        if (dt > 0) {
            float delta_ma = fabsf(reading->current_ma - 
                            ((float)(s_last_mv[phase] - s_cal[phase].zero_mv) / 
                             s_cal[phase].sensitivity_mv_a * 1000.0f));
            float rate = (delta_ma * 1000.0f) / (float)dt;
            
            if (rate > MAX_RATE_MA_PER_SEC) {
                ESP_LOGW(TAG, "Phase %d rate violation: %.0f mA/s", phase, rate);
                reading->quality = QUALITY_DEGRADED;
            }
        }
    }
    
    s_last_mv[phase] = reading->compensated_mv;
    s_last_time[phase] = now;
    
    reading->is_valid = true;
    if (reading->quality == QUALITY_INVALID) {
        reading->quality = QUALITY_GOOD;
    }
    
    return ERR_OK;
}

/* ── Public: Read all phases ─────────────────────────────────────────── */
error_code_t current_sensor_read_all(current_reading_t readings[3])
{
    if (readings == NULL) { return ERR_NULL_POINTER; }
    
    error_code_t first_err = ERR_OK;
    
    for (uint8_t p = 0; p < 3; p++) {
        error_code_t err = current_sensor_read(p, &readings[p]);
        if (err != ERR_OK && first_err == ERR_OK) {
            first_err = err;
        }
    }
    
    return first_err;
}

/* ── Public: Set calibration ─────────────────────────────────────────── */
error_code_t current_sensor_set_calibration(uint8_t phase,
                                             const current_calibration_t *cal)
{
    if (cal == NULL) { return ERR_NULL_POINTER; }
    if (phase > 2) { return ERR_INVALID_PARAMETER; }
    
    memcpy(&s_cal[phase], cal, sizeof(current_calibration_t));
    ESP_LOGI(TAG, "Phase %d calibration: zero=%ld mV, sens=%.2f mV/A",
             phase, (long)cal->zero_mv, cal->sensitivity_mv_a);
    return ERR_OK;
}

/* ── Public: Get calibration ─────────────────────────────────────────── */
error_code_t current_sensor_get_calibration(uint8_t phase,
                                             current_calibration_t *cal)
{
    if (cal == NULL) { return ERR_NULL_POINTER; }
    if (phase > 2) { return ERR_INVALID_PARAMETER; }
    
    memcpy(cal, &s_cal[phase], sizeof(current_calibration_t));
    return ERR_OK;
}

/* ── Public: Calibrate zero ──────────────────────────────────────────── */
error_code_t current_sensor_calibrate_zero(uint8_t phase)
{
    if (phase > 2) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    
    ESP_LOGI(TAG, "Calibrating zero for phase %d (ensure no current!)...", phase);
    
    int32_t sum = 0;
    int count = 0;
    const int SAMPLES = 32;
    
    for (int i = 0; i < SAMPLES; i++) {
        ads_reading_t adc;
        if (ads1115_read_channel(s_ads, phase, &adc) == ERR_OK) {
            sum += adc.millivolts;
            count++;
        }
    }
    
    if (count < SAMPLES / 2) {
        ESP_LOGE(TAG, "Calibration failed: only %d/%d samples", count, SAMPLES);
        return ERR_SENSOR_INVALID;
    }
    
    int32_t avg_mv = sum / count;
    s_cal[phase].zero_mv = (int32_t)(avg_mv * s_cal[phase].divider_ratio);
    s_cal[phase].is_calibrated = true;
    s_cal[phase].calibration_date = get_time_ms();
    
    ESP_LOGI(TAG, "Phase %d zero calibrated: %ld mV", phase, (long)s_cal[phase].zero_mv);
    return ERR_OK;
}

/* ── Public: Is online ───────────────────────────────────────────────── */
bool current_sensor_is_online(void)
{
    if (!s_initialized || s_ads == NULL) { return false; }
    return ads1115_is_online(s_ads);
}