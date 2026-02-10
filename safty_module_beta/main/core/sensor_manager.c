/**
 * @file  sensor_manager.c
 * @brief Sensor manager implementation.
 * @version 1.0.0
 *
 * @safety HIGH
 *
 * Rule 1.1: Functions ≤ 50 lines (split by sensor group).
 * Rule 2.1: All pointer and range checks.
 * Rule 4.1: sensor_reading_t stamped with MAGIC_SENSOR_DATA.
 * Rule 4.2: CRC on every reading.
 * Rule 5.13: On sensor failure, keep last-good value with QUALITY_DEGRADED.
 */
#include "sensor_manager.h"
#include "drv_ads1115.h"
#include "drv_voltage_sensor.h"
#include "drv_current_sensor.h"
#include "drv_sht45.h"
#include "drv_mq_gas.h"
#include "drv_hall_rpm.h"
#include "hal_adc.h"
#include "crc_utils.h"
#include "app_config.h"
#include "time_utils.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include <math.h>
#include <float.h>

static const char *TAG = "SENSOR_MGR";

/* ── Driver instances ────────────────────────────────────────────────── */
static ads1115_handle_t   s_ads_voltage;
static ads1115_handle_t   s_ads_current;

/* ── Cached data ─────────────────────────────────────────────────────── */
static sensor_set_t        s_data;
static SemaphoreHandle_t   s_data_mtx = NULL;

/* ── Calibration ─────────────────────────────────────────────────────── */
static calibration_session_t s_cal;

static bool s_initialized = false;

/* ── Helper: stamp a sensor_reading_t ────────────────────────────────── */

static void fill_reading(sensor_reading_t *r, float value,
                          measurement_unit_t unit, bool valid)
{
    r->magic        = MAGIC_SENSOR_DATA;
    r->timestamp_ms = get_time_ms();
    r->raw_value    = (int32_t)(value * 1000.0f);
    r->scaled_value = value;
    r->unit         = unit;
    r->quality      = valid ? QUALITY_GOOD : QUALITY_INVALID;
    r->is_valid     = valid;
    r->error_count  = valid ? 0 : (r->error_count + 1);
    r->checksum     = 0;
    r->checksum     = crc16_struct(r, sizeof(sensor_reading_t));
}

/* ── Helper: mark reading degraded (Rule 5.13: keep last-good) ───────── */

static void mark_degraded(sensor_reading_t *r)
{
    /* Don't overwrite scaled_value — that's the last-good value */
    r->quality      = QUALITY_DEGRADED;
    r->timestamp_ms = get_time_ms();
    r->error_count++;
    r->checksum     = 0;
    r->checksum     = crc16_struct(r, sizeof(sensor_reading_t));
}

/* ── Init ADS1115 devices ────────────────────────────────────────────── */

static error_code_t init_ads_devices(void)
{
    error_code_t err;

    err = ads1115_init(&s_ads_voltage, I2C_ADDR_ADS_VOLTAGE, ADS_GAIN_4096MV);
    s_data.ads_voltage_online = (err == ERR_OK);
    if (err != ERR_OK) { ESP_LOGW(TAG, "ADS voltage OFFLINE"); }

    err = ads1115_init(&s_ads_current, I2C_ADDR_ADS_CURRENT, ADS_GAIN_4096MV);
    s_data.ads_current_online = (err == ERR_OK);
    if (err != ERR_OK) { ESP_LOGW(TAG, "ADS current OFFLINE"); }

    voltage_sensor_init(&s_ads_voltage);
    current_sensor_init(&s_ads_current);

    return ERR_OK;  /* non-fatal — handled via online flags */
}

/* ── Init environment sensors ────────────────────────────────────────── */

static error_code_t init_env_sensors(void)
{
    error_code_t err;

    err = sht45_init();
    s_data.sht45_online = (err == ERR_OK);
    if (err != ERR_OK) { ESP_LOGW(TAG, "SHT45 OFFLINE"); }

    mq_gas_init();
    hall_rpm_init();

    return ERR_OK;
}

/* ── Calibration statistics helpers ──────────────────────────────────── */

static void init_cal_stat(cal_stat_t *s)
{
    s->min_val = FLT_MAX;
    s->max_val = -FLT_MAX;
    s->sum     = 0.0;
    s->count   = 0;
}

static void update_cal_stat(cal_stat_t *s, float value)
{
    if (value < s->min_val) { s->min_val = value; }
    if (value > s->max_val) { s->max_val = value; }
    s->sum += (double)value;
    s->count++;
}

static float cal_stat_avg(const cal_stat_t *s)
{
    if (s->count == 0) { return 0.0f; }
    return (float)(s->sum / (double)s->count);
}

/* ── Read voltage phases ─────────────────────────────────────────────── */

static void read_voltages(void)
{
    if (!s_data.ads_voltage_online) {
        mark_degraded(&s_data.voltage.L1);
        mark_degraded(&s_data.voltage.L2);
        mark_degraded(&s_data.voltage.L3);
        s_data.voltage.all_valid = false;
        return;
    }

    voltage_reading_t vr[3];
    error_code_t err = voltage_sensor_read_all(vr);

    sensor_reading_t *phases[] = {
        &s_data.voltage.L1, &s_data.voltage.L2, &s_data.voltage.L3
    };

    bool all_ok = true;
    for (int i = 0; i < 3; i++) {
        if (err == ERR_OK && vr[i].is_valid) {
            fill_reading(phases[i], vr[i].voltage_rms_v, UNIT_VOLTS, true);
        } else {
            mark_degraded(phases[i]);
            all_ok = false;
        }
    }
    s_data.voltage.all_valid = all_ok;
}

/* ── Read current phases ─────────────────────────────────────────────── */

static void read_currents(void)
{
    if (!s_data.ads_current_online) {
        mark_degraded(&s_data.current.L1);
        mark_degraded(&s_data.current.L2);
        mark_degraded(&s_data.current.L3);
        s_data.current.all_valid = false;
        return;
    }

    current_reading_t cr[3];
    error_code_t err = current_sensor_read_all(cr);

    sensor_reading_t *phases[] = {
        &s_data.current.L1, &s_data.current.L2, &s_data.current.L3
    };

    bool all_ok = true;
    for (int i = 0; i < 3; i++) {
        if (err == ERR_OK && cr[i].is_valid) {
            fill_reading(phases[i], cr[i].current_a, UNIT_AMPS, true);
        } else {
            mark_degraded(phases[i]);
            all_ok = false;
        }
    }
    s_data.current.all_valid = all_ok;
}

/* ── Read SHT45 temperature and humidity ─────────────────────────────── */

static void read_sht45(void)
{
    if (!s_data.sht45_online) {
        mark_degraded(&s_data.temp_ambient);
        mark_degraded(&s_data.humidity);
        return;
    }

    sht45_reading_t sr;
    error_code_t err = sht45_read(&sr);

    if (err == ERR_OK && sr.is_valid) {
        fill_reading(&s_data.temp_ambient, sr.temperature_c, UNIT_CELSIUS, true);
        fill_reading(&s_data.humidity, sr.humidity_pct, UNIT_PERCENT, true);
    } else {
        mark_degraded(&s_data.temp_ambient);
        mark_degraded(&s_data.humidity);
    }
}

/* ── Read internal ADC sensors (LM35 + audio) ────────────────────────── */

static void read_internal_adc(void)
{
    adc_reading_t adc;
    error_code_t err = hal_adc_read_filtered(ADC_CHANNEL_LM35, &adc);

    if (err == ERR_OK) {
        float actual_mv = (float)adc.filtered_mv * VDIV_MULTIPLY;
        float temp_c    = actual_mv / LM35_MV_PER_DEG_C;
        fill_reading(&s_data.temp_machine, temp_c, UNIT_CELSIUS, true);
    } else {
        mark_degraded(&s_data.temp_machine);
    }

    err = hal_adc_read_filtered(ADC_CHANNEL_MAX9814, &adc);

    if (err == ERR_OK) {
        fill_reading(&s_data.audio_level, (float)adc.filtered_mv,
                     UNIT_MILLIVOLTS, true);
        if (adc.raw > s_data.audio_peak) { s_data.audio_peak = adc.raw; }
    } else {
        mark_degraded(&s_data.audio_level);
    }
}

/* ── Read gas sensors ────────────────────────────────────────────────── */

static void read_gas_sensors(void)
{
    gas_reading_t gr[GAS_SENSOR_COUNT];
    mq_gas_read_all(gr);
    s_data.gas_warmed_up = mq_gas_is_warmed_up();

    sensor_reading_t *targets[] = {
        &s_data.gas_mq2, &s_data.gas_mq4, &s_data.gas_mq9
    };

    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        if (gr[i].is_valid) {
            fill_reading(targets[i], (float)gr[i].raw_adc,
                         UNIT_ADC_COUNT, true);
        } else {
            mark_degraded(targets[i]);
        }
    }
}

/* ── Read Hall RPM ───────────────────────────────────────────────────── */

static void read_hall_rpm(void)
{
    hall_reading_t hr;
    error_code_t err = hall_rpm_read(&hr);

    if (err == ERR_OK && hr.is_valid) {
        s_data.rpm              = hr.rpm;
        s_data.hall_pulse_count = hr.pulse_count;
    }
}

/* ── Update calibration accumulation ─────────────────────────────────── */

static void calibration_accumulate(void)
{
    if (!s_cal.is_active || s_cal.is_complete) { return; }

    uint32_t elapsed = get_time_ms() - s_cal.start_time_ms;
    if (elapsed >= s_cal.target_duration_ms) {
        sensor_mgr_stop_calibration();
        return;
    }

    /* Voltage phases */
    sensor_reading_t *v[] = {
        &s_data.voltage.L1, &s_data.voltage.L2, &s_data.voltage.L3
    };
    sensor_reading_t *c[] = {
        &s_data.current.L1, &s_data.current.L2, &s_data.current.L3
    };

    for (int i = 0; i < 3; i++) {
        if (v[i]->is_valid) { update_cal_stat(&s_cal.voltage[i], v[i]->scaled_value); }
        if (c[i]->is_valid) { update_cal_stat(&s_cal.current[i], c[i]->scaled_value); }
    }

    if (s_data.temp_machine.is_valid) {
        update_cal_stat(&s_cal.temp_machine, s_data.temp_machine.scaled_value);
    }
    if (s_data.temp_ambient.is_valid) {
        update_cal_stat(&s_cal.temp_ambient, s_data.temp_ambient.scaled_value);
    }
    if (s_data.humidity.is_valid) {
        update_cal_stat(&s_cal.humidity, s_data.humidity.scaled_value);
    }

    sensor_reading_t *gas[] = {
        &s_data.gas_mq2, &s_data.gas_mq4, &s_data.gas_mq9
    };
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        if (gas[i]->is_valid) {
            update_cal_stat(&s_cal.gas[i], gas[i]->scaled_value);
        }
    }

    s_cal.sample_count++;
}

/* ── Compute suggested thresholds from calibration ───────────────────── */

static void compute_suggestions(void)
{
    float margin = 1.0f + ((float)CALIBRATION_MARGIN_PCT / 100.0f);
    calibration_results_t *r = &s_cal.results;

    /* Find worst-case voltage and current across all phases */
    float max_v = 0.0f, min_v = FLT_MAX, max_a = 0.0f;

    for (int i = 0; i < 3; i++) {
        if (s_cal.voltage[i].count > 0) {
            if (s_cal.voltage[i].max_val > max_v) { max_v = s_cal.voltage[i].max_val; }
            if (s_cal.voltage[i].min_val < min_v) { min_v = s_cal.voltage[i].min_val; }
        }
        if (s_cal.current[i].count > 0) {
            if (s_cal.current[i].max_val > max_a) { max_a = s_cal.current[i].max_val; }
        }
    }

    /* Observed ranges */
    r->observed_voltage_max_V = max_v;
    r->observed_voltage_min_V = (min_v < FLT_MAX) ? min_v : 0.0f;
    r->observed_current_max_A = max_a;
    r->observed_temp_max_C    = s_cal.temp_machine.max_val;

    /* Find max gas reading */
    uint16_t max_gas = 0;
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        uint16_t g = (uint16_t)s_cal.gas[i].max_val;
        if (g > max_gas) { max_gas = g; }
    }
    r->observed_gas_max = max_gas;

    /* Suggested thresholds: observed max × margin */
    r->suggested_overcurrent_A  = max_a * margin;
    r->suggested_overvoltage_V  = max_v * margin;
    r->suggested_overtemp_C     = s_cal.temp_machine.max_val * margin;
    r->suggested_gas_threshold  = (uint16_t)((float)max_gas * margin);

    /* Undervoltage: observed min minus margin */
    float uv_margin = 1.0f - ((float)CALIBRATION_MARGIN_PCT / 100.0f);
    r->suggested_undervoltage_V = (min_v < FLT_MAX) ? min_v * uv_margin : DEF_UNDERVOLTAGE_V;

    /* Clamp to sane defaults */
    if (r->suggested_overcurrent_A < 1.0f)  { r->suggested_overcurrent_A = DEF_OVERCURRENT_A; }
    if (r->suggested_overvoltage_V < 50.0f) { r->suggested_overvoltage_V = DEF_OVERVOLTAGE_V; }
    if (r->suggested_overtemp_C < 30.0f)    { r->suggested_overtemp_C = DEF_OVERTEMP_C; }

    r->total_samples = s_cal.sample_count;
    r->duration_ms   = get_time_ms() - s_cal.start_time_ms;
    r->is_valid      = (s_cal.sample_count > 100);

    ESP_LOGI(TAG, "Calibration complete: OC=%.1fA OV=%.0fV UV=%.0fV OT=%.0fC GAS=%d",
             r->suggested_overcurrent_A, r->suggested_overvoltage_V,
             r->suggested_undervoltage_V, r->suggested_overtemp_C,
             r->suggested_gas_threshold);
}

/* ══════════════════════════════════════════════════════════════════════ */
/* ── Public API                                                       ── */
/* ══════════════════════════════════════════════════════════════════════ */

error_code_t sensor_mgr_init(void)
{
    ESP_LOGI(TAG, "Initializing sensor manager...");

    s_data_mtx = xSemaphoreCreateMutex();
    if (s_data_mtx == NULL) { return ERR_SENSOR_OFFLINE; }

    memset(&s_data, 0, sizeof(s_data));
    memset(&s_cal, 0, sizeof(s_cal));

    init_ads_devices();
    init_env_sensors();

    s_initialized = true;
    ESP_LOGI(TAG, "Sensor manager ready (V:%s C:%s T:%s)",
             s_data.ads_voltage_online ? "ON" : "OFF",
             s_data.ads_current_online ? "ON" : "OFF",
             s_data.sht45_online       ? "ON" : "OFF");
    return ERR_OK;
}

error_code_t sensor_mgr_read_all(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    read_voltages();
    read_currents();
    read_sht45();
    read_internal_adc();
    read_gas_sensors();
    read_hall_rpm();

    /* Update calibration if active */
    calibration_accumulate();

    return ERR_OK;
}

error_code_t sensor_mgr_get_data(sensor_set_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    if (xSemaphoreTake(s_data_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ERR_SENSOR_TIMEOUT;
    }
    memcpy(out, &s_data, sizeof(sensor_set_t));
    xSemaphoreGive(s_data_mtx);
    return ERR_OK;
}

error_code_t sensor_mgr_start_calibration(uint32_t duration_ms)
{
    if (duration_ms < CALIBRATION_MIN_DURATION_MS) {
        return ERR_INVALID_PARAMETER;
    }

    memset(&s_cal, 0, sizeof(s_cal));

    for (int i = 0; i < 3; i++) {
        init_cal_stat(&s_cal.voltage[i]);
        init_cal_stat(&s_cal.current[i]);
    }
    init_cal_stat(&s_cal.temp_machine);
    init_cal_stat(&s_cal.temp_ambient);
    init_cal_stat(&s_cal.humidity);
    for (int i = 0; i < GAS_SENSOR_COUNT; i++) {
        init_cal_stat(&s_cal.gas[i]);
    }

    s_cal.start_time_ms      = get_time_ms();
    s_cal.target_duration_ms = duration_ms;
    s_cal.is_active          = true;
    s_cal.is_complete        = false;

    ESP_LOGI(TAG, "Calibration started (%lu ms)", (unsigned long)duration_ms);
    return ERR_OK;
}

error_code_t sensor_mgr_stop_calibration(void)
{
    if (!s_cal.is_active) { return ERR_OK; }

    s_cal.is_active   = false;
    s_cal.is_complete = true;
    compute_suggestions();

    ESP_LOGI(TAG, "Calibration stopped (%lu samples)",
             (unsigned long)s_cal.sample_count);
    return ERR_OK;
}

uint8_t sensor_mgr_get_calibration_progress(void)
{
    if (!s_cal.is_active) {
        return s_cal.is_complete ? 100 : 0;
    }
    uint32_t elapsed = get_time_ms() - s_cal.start_time_ms;
    if (elapsed >= s_cal.target_duration_ms) { return 100; }
    return (uint8_t)((elapsed * 100UL) / s_cal.target_duration_ms);
}

error_code_t sensor_mgr_get_calibration_results(calibration_results_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_cal.is_complete) { return ERR_NOT_INITIALIZED; }

    memcpy(out, &s_cal.results, sizeof(calibration_results_t));
    return ERR_OK;
}

bool sensor_mgr_is_calibrating(void)
{
    return s_cal.is_active && !s_cal.is_complete;
}

error_code_t sensor_mgr_self_test(void)
{
    ESP_LOGI(TAG, "Sensor manager self-test...");

    if (s_data.ads_voltage_online) { ads1115_self_test(&s_ads_voltage); }
    if (s_data.ads_current_online) { ads1115_self_test(&s_ads_current); }
    if (s_data.sht45_online)       { sht45_self_test(); }

    hal_adc_self_test();

    ESP_LOGI(TAG, "Self-test complete");
    return ERR_OK;
}