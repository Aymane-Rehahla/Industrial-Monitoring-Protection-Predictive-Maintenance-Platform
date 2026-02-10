/**
 * @file hal_adc.c
 * @brief Internal ADC HAL — thread-safety fixed.
 * @version 1.0.1
 * @safety MEDIUM
 *
 * BUG 11: mutex held for entire filtered read.
 * BUG 26: uses time_utils.h
 */

#include "hal_adc.h"
#include "app_config.h"
#include "time_utils.h"
#include "filter.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "HAL_ADC";

static const int CH_MAP[ADC_CHANNEL_COUNT] = {
    ADC_CHANNEL_LM35, ADC_CHANNEL_MAX9814,
    ADC_CHANNEL_MQ2,  ADC_CHANNEL_MQ4, ADC_CHANNEL_MQ9
};

#define DEFAULT_RATE_LIMIT   5000
#define MAX_MV               3500

/* ── State ───────────────────────────────────────────────────────────── */

static adc_oneshot_unit_handle_t s_handle = NULL;
static adc_cali_handle_t        s_cali   = NULL;
static SemaphoreHandle_t        s_mtx    = NULL;
static bool s_calibrated   = false;
static bool s_initialized  = false;

typedef struct {
    int32_t  last_mv;
    uint32_t last_time_ms;
    int32_t  rate_limit;
    int32_t  filt_buf[SENSOR_FILTER_SAMPLES];
    uint8_t  filt_idx;
    bool     filt_primed;
    adc_chan_stats_t stats;
} ch_state_t;

static ch_state_t s_ch[ADC_CHANNEL_COUNT];

/* ── Internal read (no mutex — caller must hold it) ──────────────────── */

static error_code_t read_raw_internal(int channel, int32_t *raw)
{
    int r = 0;
    esp_err_t e = adc_oneshot_read(s_handle, CH_MAP[channel], &r);
    if (e != ESP_OK) { return ERR_ADC_READ_FAILED; }
    if (r < 0 || r > ADC_MAX_RAW) { return ERR_ADC_OUT_OF_RANGE; }
    *raw = r;
    s_ch[channel].stats.total++;
    s_ch[channel].stats.valid++;
    if (r < s_ch[channel].stats.min_raw) { s_ch[channel].stats.min_raw = r; }
    if (r > s_ch[channel].stats.max_raw) { s_ch[channel].stats.max_raw = r; }
    return ERR_OK;
}

static error_code_t read_mv_internal(int channel, int32_t *mv)
{
    int32_t raw;
    error_code_t err = read_raw_internal(channel, &raw);
    if (err != ERR_OK) { return err; }

    int m = 0;
    if (s_calibrated && s_cali) {
        adc_cali_raw_to_voltage(s_cali, raw, &m);
    } else {
        m = (raw * ADC_REF_MV) / ADC_MAX_RAW;
    }
    if (m < 0 || m > MAX_MV) { return ERR_ADC_OUT_OF_RANGE; }
    *mv = m;
    return ERR_OK;
}

/* ── Public ──────────────────────────────────────────────────────────── */

error_code_t hal_adc_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC...");

    s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) { return ERR_ADC_INIT_FAILED; }

    adc_oneshot_unit_init_cfg_t u = { .unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE };
    esp_err_t e = adc_oneshot_new_unit(&u, &s_handle);
    if (e != ESP_OK) { return ERR_ADC_INIT_FAILED; }

    adc_oneshot_chan_cfg_t cc = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        adc_oneshot_config_channel(s_handle, CH_MAP[i], &cc);
        s_ch[i].rate_limit    = DEFAULT_RATE_LIMIT;
        s_ch[i].stats.min_raw = INT32_MAX;
        s_ch[i].stats.max_raw = INT32_MIN;
    }

    adc_cali_curve_fitting_config_t cal = {
        .unit_id = ADC_UNIT_1, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12
    };
    s_calibrated = (adc_cali_create_scheme_curve_fitting(&cal, &s_cali) == ESP_OK);
    ESP_LOGI(TAG, "  Calibration: %s", s_calibrated ? "YES" : "NO");

    s_initialized = true;
    ESP_LOGI(TAG, "ADC init complete");
    return ERR_OK;
}

error_code_t hal_adc_read_raw(int channel, int32_t *raw_out)
{
    if (!raw_out)                              { return ERR_NULL_POINTER; }
    if (channel < 0 || channel >= ADC_CHANNEL_COUNT) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized)                        { return ERR_ADC_INIT_FAILED; }
    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(100)) != pdTRUE) { return ERR_ADC_READ_FAILED; }

    error_code_t r = read_raw_internal(channel, raw_out);
    xSemaphoreGive(s_mtx);
    return r;
}

error_code_t hal_adc_read_mv(int channel, int32_t *mv_out)
{
    if (!mv_out)                               { return ERR_NULL_POINTER; }
    if (channel < 0 || channel >= ADC_CHANNEL_COUNT) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized)                        { return ERR_ADC_INIT_FAILED; }
    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(100)) != pdTRUE) { return ERR_ADC_READ_FAILED; }

    error_code_t r = read_mv_internal(channel, mv_out);
    xSemaphoreGive(s_mtx);
    return r;
}

/* BUG 11 fix: hold mutex for entire filtered operation */
error_code_t hal_adc_read_filtered(int channel, adc_reading_t *out)
{
    if (!out) { return ERR_NULL_POINTER; }
    if (channel < 0 || channel >= ADC_CHANNEL_COUNT) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized) { return ERR_ADC_INIT_FAILED; }

    memset(out, 0, sizeof(*out));
    out->timestamp_ms = get_time_ms();
    out->quality = QUALITY_INVALID;

    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ERR_ADC_READ_FAILED;
    }

    /* take all samples under one lock */
    int32_t samples[SENSOR_FILTER_SAMPLES];
    int valid = 0;
    for (int i = 0; i < SENSOR_FILTER_SAMPLES; i++) {
        int32_t mv;
        if (read_mv_internal(channel, &mv) == ERR_OK) {
            samples[valid++] = mv;
        }
    }

    if (valid == 0) { xSemaphoreGive(s_mtx); return ERR_ADC_READ_FAILED; }

    int32_t med = filter_median(samples, valid);

    /* update per-channel filter state (still under lock) */
    ch_state_t *ch = &s_ch[channel];
    ch->filt_buf[ch->filt_idx] = med;
    ch->filt_idx = (ch->filt_idx + 1) % SENSOR_FILTER_SAMPLES;
    if (ch->filt_idx == 0) { ch->filt_primed = true; }

    int fcount = ch->filt_primed ? SENSOR_FILTER_SAMPLES : ch->filt_idx;
    int32_t filtered = filter_median(ch->filt_buf, fcount > 0 ? fcount : 1);

    /* rate-of-change check */
    uint32_t now = get_time_ms();
    bool rate_ok = true;
    if (ch->last_time_ms > 0) {
        uint32_t dt = now - ch->last_time_ms;
        if (dt == 0) { dt = 1; }
        int32_t rate = (abs(filtered - ch->last_mv) * 1000) / (int32_t)dt;
        if (rate > ch->rate_limit) {
            ch->stats.rate_violations++;
            rate_ok = false;
        }
    }
    ch->last_mv = filtered;
    ch->last_time_ms = now;

    xSemaphoreGive(s_mtx);

    out->raw          = (med * ADC_MAX_RAW) / ADC_REF_MV;
    out->mv           = med;
    out->filtered_mv  = filtered;
    out->rate_ok      = rate_ok;
    out->quality      = (valid == SENSOR_FILTER_SAMPLES && rate_ok)
                        ? QUALITY_GOOD : QUALITY_DEGRADED;
    return ERR_OK;
}

error_code_t hal_adc_set_rate_limit(int channel, int32_t max_mv_per_s)
{
    if (channel < 0 || channel >= ADC_CHANNEL_COUNT) { return ERR_INVALID_PARAMETER; }
    if (max_mv_per_s <= 0) { return ERR_INVALID_PARAMETER; }
    s_ch[channel].rate_limit = max_mv_per_s;
    return ERR_OK;
}

error_code_t hal_adc_get_stats(int channel, adc_chan_stats_t *out)
{
    if (!out || channel < 0 || channel >= ADC_CHANNEL_COUNT) { return ERR_NULL_POINTER; }
    memcpy(out, &s_ch[channel].stats, sizeof(*out));
    return ERR_OK;
}

bool hal_adc_is_calibrated(void) { return s_calibrated; }

error_code_t hal_adc_self_test(void)
{
    ESP_LOGI(TAG, "ADC self-test...");
    if (!s_initialized) { return ERR_ADC_INIT_FAILED; }
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        int32_t r;
        error_code_t e = hal_adc_read_raw(i, &r);
        ESP_LOGI(TAG, "  CH%d: %s (%ld)", i, e == ERR_OK ? "OK" : "FAIL", (long)r);
    }
    ESP_LOGI(TAG, "ADC self-test done");
    return ERR_OK;
}