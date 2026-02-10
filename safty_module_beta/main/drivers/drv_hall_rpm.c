/**
 * @file drv_hall_rpm.c
 * @brief Hall sensor RPM implementation
 * @version 1.0.0
 * 
 * @safety LOW
 * 
 * Rule 3.9: ISR under 10µs
 */

#include "drv_hall_rpm.h"
#include "hal_gpio.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "DRV_HALL";

/* ── Volatile state (accessed from ISR) ──────────────────────────────── */
static volatile uint32_t s_pulse_count = 0;
static volatile uint32_t s_last_pulse_us = 0;
static volatile uint32_t s_period_us = 0;

/* ── Non-volatile state ──────────────────────────────────────────────── */
static uint32_t s_last_read_count = 0;
static uint32_t s_last_read_time = 0;
static bool s_initialized = false;

/* ── ISR callback (Rule 3.9: < 10µs) ─────────────────────────────────── */
static void hall_isr_callback(void)
{
    uint32_t now = (uint32_t)esp_timer_get_time();
    
    if (s_last_pulse_us > 0) {
        s_period_us = now - s_last_pulse_us;
    }
    s_last_pulse_us = now;
    s_pulse_count++;
}

/* ── Public: Init ────────────────────────────────────────────────────── */
error_code_t hall_rpm_init(void)
{
    ESP_LOGI(TAG, "Initializing Hall RPM sensor...");
    
    /* Configure interrupt via HAL */
    error_code_t err = hal_gpio_configure_hall_isr(hall_isr_callback);
    
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "Failed to configure Hall ISR: %d", err);
        return err;
    }
    
    s_pulse_count = 0;
    s_last_pulse_us = 0;
    s_period_us = 0;
    s_last_read_count = 0;
    s_last_read_time = get_time_ms();
    s_initialized = true;
    
    ESP_LOGI(TAG, "Hall RPM initialized");
    return ERR_OK;
}

/* ── Public: Read RPM ────────────────────────────────────────────────── */
error_code_t hall_rpm_read(hall_reading_t *reading)
{
    if (reading == NULL) { return ERR_NULL_POINTER; }
    
    memset(reading, 0, sizeof(*reading));
    reading->timestamp_ms = get_time_ms();
    
    if (!s_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    /* Capture volatile values atomically as possible */
    uint32_t count = s_pulse_count;
    uint32_t period = s_period_us;
    uint32_t last_pulse = s_last_pulse_us;
    
    reading->pulse_count = count;
    reading->period_us = period;
    
    /* Calculate pulses since last read */
    reading->pulse_delta = count - s_last_read_count;
    
    /* Calculate RPM */
    uint32_t now = get_time_ms();
    uint32_t dt_ms = now - s_last_read_time;
    
    if (dt_ms > 0 && reading->pulse_delta > 0) {
        /* RPM = (pulses / pulses_per_rev) * (60000 / dt_ms) */
        float pulses_per_sec = (float)reading->pulse_delta * 1000.0f / (float)dt_ms;
        reading->rpm = (pulses_per_sec * 60.0f) / (float)HALL_PULSES_PER_REV;
    }
    
    /* Check if rotating (pulse within timeout) */
    uint32_t now_us = (uint32_t)esp_timer_get_time();
    uint32_t since_pulse_ms = (now_us - last_pulse) / 1000;
    reading->is_rotating = (since_pulse_ms < HALL_RPM_TIMEOUT_MS);
    
    /* If not rotating for a while, RPM is 0 */
    if (!reading->is_rotating) {
        reading->rpm = 0;
    }
    
    reading->is_valid = true;
    
    /* Update for next read */
    s_last_read_count = count;
    s_last_read_time = now;
    
    return ERR_OK;
}

/* ── Public: Get pulses ──────────────────────────────────────────────── */
uint32_t hall_rpm_get_pulses(void)
{
    return s_pulse_count;
}

/* ── Public: Reset counter ───────────────────────────────────────────── */
void hall_rpm_reset_counter(void)
{
    s_pulse_count = 0;
    s_last_read_count = 0;
}

/* ── Public: Is rotating ─────────────────────────────────────────────── */
bool hall_rpm_is_rotating(void)
{
    if (!s_initialized) { return false; }
    
    uint32_t now_us = (uint32_t)esp_timer_get_time();
    uint32_t since_pulse_ms = (now_us - s_last_pulse_us) / 1000;
    
    return (since_pulse_ms < HALL_RPM_TIMEOUT_MS);
}