/**
 * @file drv_sht45.c
 * @brief SHT45 driver implementation
 * @version 1.0.0
 * 
 * @safety MEDIUM
 * 
 * Rule 4.2: CRC verification on all reads
 */

#include "drv_sht45.h"
#include "hal_i2c.h"
#include "app_config.h"
#include "crc_utils.h"
#include "time_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "DRV_SHT45";

/* ── SHT45 commands ──────────────────────────────────────────────────── */
#define CMD_MEASURE_HIGH    0xFD    /* High precision */
#define CMD_MEASURE_MED     0xF6    /* Medium precision */
#define CMD_MEASURE_LOW     0xE0    /* Low precision */
#define CMD_SOFT_RESET      0x94
#define CMD_READ_SERIAL     0x89

/* ── Module state ────────────────────────────────────────────────────── */
static bool s_initialized = false;
static bool s_online = false;
static uint32_t s_read_count = 0;
static uint32_t s_error_count = 0;

/* ── Verify CRC for 2-byte data ──────────────────────────────────────── */
static bool verify_crc(const uint8_t *data, uint8_t crc)
{
    return (crc8_calculate(data, 2) == crc);
}

/* ── Public: Init ────────────────────────────────────────────────────── */
error_code_t sht45_init(void)
{
    ESP_LOGI(TAG, "Initializing SHT45...");
    
    /* Probe device */
    bool found = false;
    error_code_t err = hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_SHT45, &found);
    
    if (err != ERR_OK || !found) {
        ESP_LOGE(TAG, "SHT45 not found at 0x%02X", I2C_ADDR_SHT45);
        s_online = false;
        return ERR_SENSOR_OFFLINE;
    }
    
    /* Soft reset */
    err = sht45_reset();
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "Reset failed, continuing anyway");
    }
    
    s_initialized = true;
    s_online = true;
    ESP_LOGI(TAG, "SHT45 initialized");
    return ERR_OK;
}

/* ── Public: Reset ───────────────────────────────────────────────────── */
error_code_t sht45_reset(void)
{
    uint8_t cmd = CMD_SOFT_RESET;
    error_code_t err = hal_i2c_write(I2C_BUS_SHARED, I2C_ADDR_SHT45, &cmd, 1);
    
    if (err == ERR_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));  /* Wait for reset */
    }
    return err;
}

/* ── Public: Read ────────────────────────────────────────────────────── */
error_code_t sht45_read(sht45_reading_t *reading)
{
    if (reading == NULL) { return ERR_NULL_POINTER; }
    
    /* Initialize output */
    memset(reading, 0, sizeof(*reading));
    reading->timestamp_ms = get_time_ms();
    reading->quality = QUALITY_INVALID;
    
    if (!s_initialized || !s_online) {
        return ERR_SENSOR_OFFLINE;
    }
    
    /* Send measurement command */
    uint8_t cmd = CMD_MEASURE_HIGH;
    error_code_t err = hal_i2c_write(I2C_BUS_SHARED, I2C_ADDR_SHT45, &cmd, 1);
    
    if (err != ERR_OK) {
        s_error_count++;
        ESP_LOGE(TAG, "Measure command failed: %d", err);
        return err;
    }
    
    /* Wait for measurement (~8.2ms for high precision) */
    vTaskDelay(pdMS_TO_TICKS(12));
    
    /* Read 6 bytes: T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC */
    uint8_t data[6];
    err = hal_i2c_read(I2C_BUS_SHARED, I2C_ADDR_SHT45, data, 6);
    
    if (err != ERR_OK) {
        s_error_count++;
        ESP_LOGE(TAG, "Read failed: %d", err);
        return err;
    }
    
    /* Verify CRCs (Rule 4.2) */
    bool temp_crc_ok = verify_crc(&data[0], data[2]);
    bool hum_crc_ok  = verify_crc(&data[3], data[5]);
    reading->crc_ok = temp_crc_ok && hum_crc_ok;
    
    if (!reading->crc_ok) {
        s_error_count++;
        ESP_LOGE(TAG, "CRC error (T:%d H:%d)", temp_crc_ok, hum_crc_ok);
        return ERR_SENSOR_INVALID;
    }
    
    /* Convert temperature: T = -45 + 175 × (raw / 65535) */
    uint16_t t_raw = ((uint16_t)data[0] << 8) | data[1];
    reading->temperature_c = -45.0f + 175.0f * ((float)t_raw / 65535.0f);
    
    /* Convert humidity: RH = -6 + 125 × (raw / 65535) */
    uint16_t h_raw = ((uint16_t)data[3] << 8) | data[4];
    reading->humidity_pct = -6.0f + 125.0f * ((float)h_raw / 65535.0f);
    
    /* Clamp humidity */
    if (reading->humidity_pct < 0.0f) { reading->humidity_pct = 0.0f; }
    if (reading->humidity_pct > 100.0f) { reading->humidity_pct = 100.0f; }
    
    reading->is_valid = true;
    reading->quality = QUALITY_GOOD;
    s_read_count++;
    
    return ERR_OK;
}

/* ── Public: Is online ───────────────────────────────────────────────── */
bool sht45_is_online(void)
{
    return s_initialized && s_online;
}

/* ── Public: Self-test ───────────────────────────────────────────────── */
error_code_t sht45_self_test(void)
{
    ESP_LOGI(TAG, "SHT45 self-test...");
    
    /* Re-probe */
    bool found = false;
    error_code_t err = hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_SHT45, &found);
    
    if (err != ERR_OK || !found) {
        s_online = false;
        ESP_LOGE(TAG, "  FAIL: not responding");
        return ERR_SENSOR_OFFLINE;
    }
    
    /* Read and verify */
    sht45_reading_t r;
    err = sht45_read(&r);
    
    if (err != ERR_OK || !r.is_valid) {
        ESP_LOGE(TAG, "  FAIL: read error");
        return ERR_SENSOR_INVALID;
    }
    
    /* Sanity check values */
    if (r.temperature_c < -40.0f || r.temperature_c > 125.0f) {
        ESP_LOGE(TAG, "  FAIL: temp out of range (%.1f)", r.temperature_c);
        return ERR_SENSOR_INVALID;
    }
    
    s_online = true;
    ESP_LOGI(TAG, "  PASS: T=%.1fC H=%.1f%%", r.temperature_c, r.humidity_pct);
    return ERR_OK;
}