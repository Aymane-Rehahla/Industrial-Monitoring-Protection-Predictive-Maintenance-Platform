/**
 * @file drv_ads1115.c
 * @brief ADS1115 driver implementation
 * @version 1.0.0
 * 
 * @safety MEDIUM
 * 
 * Rule 2.1: All pointers checked
 * Rule 2.5: All blocking operations have timeouts
 * Rule 3.8: I2C transactions have hard timeouts
 */

#include "drv_ads1115.h"
#include "hal_i2c.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DRV_ADS1115";

/* ── ADS1115 registers ───────────────────────────────────────────────── */
#define REG_CONVERSION  0x00
#define REG_CONFIG      0x01

/* ── Config register bits ────────────────────────────────────────────── */
#define CFG_OS_START    (1 << 15)
#define CFG_MUX_SHIFT   12
#define CFG_GAIN_SHIFT  9
#define CFG_MODE_SINGLE (1 << 8)
#define CFG_RATE_SHIFT  5
#define CFG_COMP_DISABLE 0x03

/* ── Full-scale voltage in microvolts for each gain ──────────────────── */
static const int32_t GAIN_FS_UV[6] = {
    6144000, 4096000, 2048000, 1024000, 512000, 256000
};

/* ── Conversion time in ms for each rate ─────────────────────────────── */
static const uint8_t RATE_CONV_MS[8] = {
    125, 63, 32, 16, 8, 4, 3, 2
};

/* ── Build config word (Rule 1.1: one job) ───────────────────────────── */
static uint16_t build_config(uint8_t channel, ads_gain_t gain, ads_rate_t rate)
{
    /* Single-ended: AINx vs GND = MUX 0x04 + channel */
    uint16_t mux = (0x04 + (channel & 0x03)) << CFG_MUX_SHIFT;
    uint16_t pga = ((uint16_t)gain & 0x07) << CFG_GAIN_SHIFT;
    uint16_t dr  = ((uint16_t)rate & 0x07) << CFG_RATE_SHIFT;
    
    return CFG_OS_START | mux | pga | CFG_MODE_SINGLE | dr | CFG_COMP_DISABLE;
}

/* ── Write config register ───────────────────────────────────────────── */
static error_code_t write_config(uint8_t addr, uint16_t config)
{
    uint8_t buf[3] = {
        REG_CONFIG,
        (config >> 8) & 0xFF,
        config & 0xFF
    };
    return hal_i2c_write(I2C_BUS_ADC, addr, buf, 3);
}

/* ── Read conversion register ────────────────────────────────────────── */
static error_code_t read_conversion(uint8_t addr, int16_t *raw_out)
{
    if (raw_out == NULL) { return ERR_NULL_POINTER; }
    
    uint8_t reg = REG_CONVERSION;
    uint8_t buf[2];
    
    error_code_t err = hal_i2c_write_read(I2C_BUS_ADC, addr, &reg, 1, buf, 2);
    if (err != ERR_OK) { return err; }
    
    /* Big-endian to signed 16-bit */
    *raw_out = (int16_t)((buf[0] << 8) | buf[1]);
    return ERR_OK;
}

/* ── Public: Init ────────────────────────────────────────────────────── */
error_code_t ads1115_init(ads1115_handle_t *handle, uint8_t i2c_addr, 
                          ads_gain_t gain)
{
    if (handle == NULL) { return ERR_NULL_POINTER; }
    
    /* Initialize handle to known state */
    handle->i2c_addr    = i2c_addr;
    handle->gain        = gain;
    handle->rate        = ADS_RATE_128SPS;
    handle->is_online   = false;
    handle->read_count  = 0;
    handle->error_count = 0;
    handle->last_read_ms = 0;
    
    /* Probe device */
    bool found = false;
    error_code_t err = hal_i2c_probe(I2C_BUS_ADC, i2c_addr, &found);
    
    if (err != ERR_OK || !found) {
        ESP_LOGE(TAG, "ADS1115 at 0x%02X not found", i2c_addr);
        return ERR_SENSOR_OFFLINE;
    }
    
    handle->is_online = true;
    ESP_LOGI(TAG, "ADS1115 at 0x%02X initialized, gain=%d", i2c_addr, gain);
    return ERR_OK;
}

/* ── Public: Read channel ────────────────────────────────────────────── */
error_code_t ads1115_read_channel(ads1115_handle_t *handle, uint8_t channel,
                                   ads_reading_t *reading)
{
    /* Rule 2.1: NULL checks */
    if (handle == NULL || reading == NULL) { return ERR_NULL_POINTER; }
    
    /* Rule 2.9: Range check */
    if (channel > 3) { return ERR_INVALID_PARAMETER; }
    
    /* Initialize output to invalid */
    reading->raw          = 0;
    reading->millivolts   = 0;
    reading->timestamp_ms = get_time_ms();
    reading->is_valid     = false;
    reading->quality      = QUALITY_INVALID;
    
    /* Check device online */
    if (!handle->is_online) {
        handle->error_count++;
        return ERR_SENSOR_OFFLINE;
    }
    
    /* Build and write config to start conversion */
    uint16_t config = build_config(channel, handle->gain, handle->rate);
    error_code_t err = write_config(handle->i2c_addr, config);
    
    if (err != ERR_OK) {
        handle->error_count++;
        ESP_LOGE(TAG, "0x%02X write config failed: %d", handle->i2c_addr, err);
        return err;
    }
    
    /* Wait for conversion (Rule 3.7: no busy-wait > 1ms) */
    uint8_t wait_ms = RATE_CONV_MS[handle->rate] + 2; /* +2ms margin */
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    
    /* Read result */
    int16_t raw;
    err = read_conversion(handle->i2c_addr, &raw);
    
    if (err != ERR_OK) {
        handle->error_count++;
        ESP_LOGE(TAG, "0x%02X read conversion failed: %d", handle->i2c_addr, err);
        return err;
    }
    
    /* Success - populate reading */
    reading->raw          = raw;
    reading->millivolts   = ads1115_raw_to_mv(handle, raw);
    reading->timestamp_ms = get_time_ms();
    reading->is_valid     = true;
    reading->quality      = QUALITY_GOOD;
    
    handle->read_count++;
    handle->last_read_ms = reading->timestamp_ms;
    
    return ERR_OK;
}

/* ── Public: Read all channels ───────────────────────────────────────── */
error_code_t ads1115_read_all_channels(ads1115_handle_t *handle,
                                        ads_reading_t readings[4])
{
    if (handle == NULL || readings == NULL) { return ERR_NULL_POINTER; }
    
    error_code_t first_error = ERR_OK;
    
    /* Rule 2.3: bounded loop */
    for (uint8_t ch = 0; ch < 4; ch++) {
        error_code_t err = ads1115_read_channel(handle, ch, &readings[ch]);
        if (err != ERR_OK && first_error == ERR_OK) {
            first_error = err;
        }
    }
    
    return first_error;
}

/* ── Public: Set gain ────────────────────────────────────────────────── */
error_code_t ads1115_set_gain(ads1115_handle_t *handle, ads_gain_t gain)
{
    if (handle == NULL) { return ERR_NULL_POINTER; }
    if (gain > ADS_GAIN_256MV) { return ERR_INVALID_PARAMETER; }
    handle->gain = gain;
    return ERR_OK;
}

/* ── Public: Set rate ────────────────────────────────────────────────── */
error_code_t ads1115_set_rate(ads1115_handle_t *handle, ads_rate_t rate)
{
    if (handle == NULL) { return ERR_NULL_POINTER; }
    if (rate > ADS_RATE_860SPS) { return ERR_INVALID_PARAMETER; }
    handle->rate = rate;
    return ERR_OK;
}

/* ── Public: Is online ───────────────────────────────────────────────── */
bool ads1115_is_online(const ads1115_handle_t *handle)
{
    if (handle == NULL) { return false; }
    return handle->is_online;
}

/* ── Public: Raw to mV ───────────────────────────────────────────────── */
int32_t ads1115_raw_to_mv(const ads1115_handle_t *handle, int16_t raw)
{
    if (handle == NULL) { return 0; }
    if (handle->gain > ADS_GAIN_256MV) { return 0; }
    
    /* mv = raw × (full_scale_µV / 32768) / 1000 */
    int64_t uv = ((int64_t)raw * GAIN_FS_UV[handle->gain]) / 32768;
    return (int32_t)(uv / 1000);
}

/* ── Public: Self-test ───────────────────────────────────────────────── */
error_code_t ads1115_self_test(ads1115_handle_t *handle)
{
    if (handle == NULL) { return ERR_NULL_POINTER; }
    
    ESP_LOGI(TAG, "Self-test 0x%02X...", handle->i2c_addr);
    
    /* Re-probe */
    bool found = false;
    error_code_t err = hal_i2c_probe(I2C_BUS_ADC, handle->i2c_addr, &found);
    
    if (err != ERR_OK || !found) {
        handle->is_online = false;
        ESP_LOGE(TAG, "  FAIL: device not responding");
        return ERR_SENSOR_OFFLINE;
    }
    
    /* Read channel 0 and verify plausible */
    ads_reading_t reading;
    err = ads1115_read_channel(handle, 0, &reading);
    
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "  FAIL: read error %d", err);
        return ERR_SENSOR_INVALID;
    }
    
    /* Check raw value is not stuck at limits */
    if (reading.raw == 32767 || reading.raw == -32768) {
        ESP_LOGW(TAG, "  WARN: saturated reading");
    }
    
    ESP_LOGI(TAG, "  PASS: raw=%d, mv=%ld", reading.raw, (long)reading.millivolts);
    handle->is_online = true;
    return ERR_OK;
}