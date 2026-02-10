/**
 * @file hal_i2c.c
 * @brief I2C HAL — 2 buses only. All bugs fixed.
 * @version 1.0.1
 * @safety MEDIUM
 *
 * BUG 2:  only 2 I2C ports
 * BUG 10: reset_bus race condition fixed
 */

#include "hal_i2c.h"
#include "app_config.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "HAL_I2C";

/* ── Bus configuration table ─────────────────────────────────────────── */

typedef struct {
    int         port;
    int         sda;
    int         scl;
    uint32_t    freq;
    const char *name;
} bus_cfg_t;

static const bus_cfg_t BUS_CFG[I2C_BUS_COUNT] = {
    [I2C_BUS_ADC]    = { I2C_PORT_ADC,    PIN_I2C0_SDA, PIN_I2C0_SCL,
                         I2C_FREQ_ADC_HZ,    "ADC" },
    [I2C_BUS_SHARED] = { I2C_PORT_SHARED, PIN_I2C1_SDA, PIN_I2C1_SCL,
                         I2C_FREQ_SHARED_HZ, "SHARED" },
};

/* ── Per-bus state ───────────────────────────────────────────────────── */

static bool              s_init[I2C_BUS_COUNT] = {false, false};
static SemaphoreHandle_t s_mtx[I2C_BUS_COUNT]  = {NULL, NULL};
static i2c_stats_t       s_stats[I2C_BUS_COUNT];

/* ── Mutex helpers ───────────────────────────────────────────────────── */

static bool take(i2c_bus_id_t b, uint32_t ms)
{
    if (s_mtx[b] == NULL) { return false; }
    return xSemaphoreTake(s_mtx[b], pdMS_TO_TICKS(ms)) == pdTRUE;
}

static void give(i2c_bus_id_t b)
{
    if (s_mtx[b]) { xSemaphoreGive(s_mtx[b]); }
}

/* ── Install a single bus (no mutex creation) ────────────────────────── */

static error_code_t install_bus_driver(i2c_bus_id_t b)
{
    const bus_cfg_t *c = &BUS_CFG[b];
    i2c_config_t conf  = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = c->sda,
        .scl_io_num       = c->scl,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = c->freq,
    };
    esp_err_t e = i2c_param_config(c->port, &conf);
    if (e != ESP_OK) { return ERR_I2C_INIT_FAILED; }
    e = i2c_driver_install(c->port, I2C_MODE_MASTER, 0, 0, 0);
    if (e != ESP_OK) { return ERR_I2C_INIT_FAILED; }
    s_init[b] = true;
    return ERR_OK;
}

/* ── Init all ────────────────────────────────────────────────────────── */

error_code_t hal_i2c_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C (%d buses)...", I2C_BUS_COUNT);
    error_code_t result = ERR_OK;

    for (int b = 0; b < I2C_BUS_COUNT; b++) {
        s_mtx[b] = xSemaphoreCreateMutex();
        if (s_mtx[b] == NULL) {
            ESP_LOGE(TAG, "  Bus %d mutex fail", b);
            result = ERR_I2C_INIT_FAILED;
            continue;
        }
        memset(&s_stats[b], 0, sizeof(i2c_stats_t));
        error_code_t e = install_bus_driver(b);
        if (e != ERR_OK) {
            ESP_LOGE(TAG, "  Bus %d (%s) FAILED", b, BUS_CFG[b].name);
            result = e;
        } else {
            ESP_LOGI(TAG, "  Bus %d (%s) OK", b, BUS_CFG[b].name);
        }
    }
    return result;
}

/* ── Write ───────────────────────────────────────────────────────────── */

error_code_t hal_i2c_write(i2c_bus_id_t bus, uint8_t addr,
                           const uint8_t *data, size_t len)
{
    if (data == NULL)              { return ERR_NULL_POINTER; }
    if (bus >= I2C_BUS_COUNT)      { return ERR_INVALID_PARAMETER; }
    if (!s_init[bus])              { return ERR_I2C_INIT_FAILED; }
    if (len == 0 || len > 256)     { return ERR_INVALID_PARAMETER; }
    if (!take(bus, I2C_TIMEOUT_MS)){ s_stats[bus].err_timeout++; return ERR_I2C_TIMEOUT; }

    s_stats[bus].tx_total++;
    esp_err_t e = ESP_FAIL;
    for (int r = 0; r < I2C_RETRY_COUNT; r++) {
        e = i2c_master_write_to_device(BUS_CFG[bus].port, addr,
                                       data, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        if (e == ESP_OK) { s_stats[bus].tx_ok++; give(bus); return ERR_OK; }
    }
    give(bus);
    if (e == ESP_ERR_TIMEOUT) { s_stats[bus].err_timeout++; return ERR_I2C_TIMEOUT; }
    s_stats[bus].err_nack++;
    return ERR_I2C_NACK;
}

/* ── Read ────────────────────────────────────────────────────────────── */

error_code_t hal_i2c_read(i2c_bus_id_t bus, uint8_t addr,
                          uint8_t *data, size_t len)
{
    if (data == NULL)              { return ERR_NULL_POINTER; }
    if (bus >= I2C_BUS_COUNT)      { return ERR_INVALID_PARAMETER; }
    if (!s_init[bus])              { return ERR_I2C_INIT_FAILED; }
    if (len == 0 || len > 256)     { return ERR_INVALID_PARAMETER; }
    if (!take(bus, I2C_TIMEOUT_MS)){ s_stats[bus].err_timeout++; return ERR_I2C_TIMEOUT; }

    s_stats[bus].tx_total++;
    esp_err_t e = ESP_FAIL;
    for (int r = 0; r < I2C_RETRY_COUNT; r++) {
        e = i2c_master_read_from_device(BUS_CFG[bus].port, addr,
                                        data, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        if (e == ESP_OK) { s_stats[bus].tx_ok++; give(bus); return ERR_OK; }
    }
    give(bus);
    if (e == ESP_ERR_TIMEOUT) { s_stats[bus].err_timeout++; return ERR_I2C_TIMEOUT; }
    s_stats[bus].err_nack++;
    return ERR_I2C_NACK;
}

/* ── Write-Read ──────────────────────────────────────────────────────── */

error_code_t hal_i2c_write_read(i2c_bus_id_t bus, uint8_t addr,
                                const uint8_t *wr, size_t wr_len,
                                uint8_t *rd, size_t rd_len)
{
    if (wr == NULL || rd == NULL)  { return ERR_NULL_POINTER; }
    if (bus >= I2C_BUS_COUNT)      { return ERR_INVALID_PARAMETER; }
    if (!s_init[bus])              { return ERR_I2C_INIT_FAILED; }
    if (!take(bus, I2C_TIMEOUT_MS)){ s_stats[bus].err_timeout++; return ERR_I2C_TIMEOUT; }

    s_stats[bus].tx_total++;
    esp_err_t e = ESP_FAIL;
    for (int r = 0; r < I2C_RETRY_COUNT; r++) {
        e = i2c_master_write_read_device(BUS_CFG[bus].port, addr,
                                         wr, wr_len, rd, rd_len,
                                         pdMS_TO_TICKS(I2C_TIMEOUT_MS));
        if (e == ESP_OK) { s_stats[bus].tx_ok++; give(bus); return ERR_OK; }
    }
    give(bus);
    if (e == ESP_ERR_TIMEOUT) { s_stats[bus].err_timeout++; return ERR_I2C_TIMEOUT; }
    s_stats[bus].err_nack++;
    return ERR_I2C_NACK;
}

/* ── Probe ───────────────────────────────────────────────────────────── */

error_code_t hal_i2c_probe(i2c_bus_id_t bus, uint8_t addr, bool *found)
{
    if (found == NULL)             { return ERR_NULL_POINTER; }
    *found = false;
    if (bus >= I2C_BUS_COUNT)      { return ERR_INVALID_PARAMETER; }
    if (!s_init[bus])              { return ERR_I2C_INIT_FAILED; }
    if (!take(bus, I2C_TIMEOUT_MS)){ return ERR_I2C_TIMEOUT; }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t e = i2c_master_cmd_begin(BUS_CFG[bus].port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    give(bus);

    *found = (e == ESP_OK);
    return ERR_OK;
}

/* ── Scan ────────────────────────────────────────────────────────────── */

uint8_t hal_i2c_scan(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT || !s_init[bus]) { return 0; }
    ESP_LOGI(TAG, "Scanning bus %d (%s)...", bus, BUS_CFG[bus].name);
    uint8_t n = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
        bool f = false;
        hal_i2c_probe(bus, a, &f);
        if (f) { ESP_LOGI(TAG, "  0x%02X found", a); n++; }
    }
    ESP_LOGI(TAG, "  %d device(s)", n);
    return n;
}

/* ── Reset (BUG 10 fix: hold mutex, reuse it) ────────────────────────── */

error_code_t hal_i2c_reset_bus(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_PARAMETER; }
    if (!take(bus, 500))      { return ERR_I2C_TIMEOUT; }

    ESP_LOGW(TAG, "Resetting bus %d...", bus);
    i2c_driver_delete(BUS_CFG[bus].port);
    s_init[bus] = false;
    vTaskDelay(pdMS_TO_TICKS(10));

    error_code_t err = install_bus_driver(bus);
    s_stats[bus].bus_resets++;
    give(bus);  /* reuse existing mutex — do NOT recreate */

    if (err == ERR_OK) { ESP_LOGI(TAG, "  Bus %d reset OK", bus); }
    else               { ESP_LOGE(TAG, "  Bus %d reset FAILED", bus); }
    return err;
}

/* ── Stats ───────────────────────────────────────────────────────────── */

error_code_t hal_i2c_get_stats(i2c_bus_id_t bus, i2c_stats_t *out)
{
    if (out == NULL)          { return ERR_NULL_POINTER; }
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_PARAMETER; }
    memcpy(out, &s_stats[bus], sizeof(i2c_stats_t));
    return ERR_OK;
}

/* ── Self-test ───────────────────────────────────────────────────────── */

error_code_t hal_i2c_self_test(void)
{
    ESP_LOGI(TAG, "I2C self-test...");
    error_code_t r = ERR_OK;
    for (int b = 0; b < I2C_BUS_COUNT; b++) {
        if (!s_init[b]) {
            ESP_LOGE(TAG, "  Bus %d not init", b);
            r = ERR_I2C_INIT_FAILED;
        } else {
            ESP_LOGI(TAG, "  Bus %d OK", b);
        }
    }
    bool f;
    hal_i2c_probe(I2C_BUS_ADC, I2C_ADDR_ADS_VOLTAGE, &f);
    ESP_LOGI(TAG, "  ADS volt: %s", f ? "FOUND" : "---");
    hal_i2c_probe(I2C_BUS_ADC, I2C_ADDR_ADS_CURRENT, &f);
    ESP_LOGI(TAG, "  ADS curr: %s", f ? "FOUND" : "---");
    hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_LCD_PRIMARY, &f);
    if (!f) { hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_LCD_ALTERNATE, &f); }
    ESP_LOGI(TAG, "  LCD:      %s", f ? "FOUND" : "---");
    hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_SHT45, &f);
    ESP_LOGI(TAG, "  SHT45:    %s", f ? "FOUND" : "---");
    return r;
}