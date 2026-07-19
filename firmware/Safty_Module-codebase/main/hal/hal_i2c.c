// ═══ FILE: main/hal/hal_i2c.c ═══
/**
 * @file    hal_i2c.c
 * @brief   I2C HAL — ESP-IDF v5.2+ new i2c_master driver.
 *
 *          WHY new API: The legacy driver/i2c.h is deprecated in
 *          ESP-IDF v5.2+ and may be removed.  The new i2c_master API
 *          is the supported path forward.
 *
 *          ARCHITECTURE: The new API uses device handles (one per
 *          address+bus pair).  We lazily create and cache these on
 *          first use.  Cache is fixed-size (8 entries per bus),
 *          covering all known devices with room to spare.
 *
 * @version 2.0.0
 * @date    2025-01-01
 * @safety  HIGH — Bus 0 reads safety-critical sensors (ADS1115).
 *
 * CHANGELOG:
 *   2.0.0  2025-01-01  Rewritten for ESP-IDF v5.2+ new I2C master API.
 *   1.0.0  2025-01-01  Initial version using legacy driver/i2c.h.
 */

#include "hal/hal_i2c.h"
#include "system_types.h"
#include "app_config.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "hal_i2c";

/* ── Constants ───────────────────────────────────────────────────────── */

/** Max cached device handles per bus.  We have at most 5 known
 *  devices across both buses; 8 gives headroom for probing. */
#define I2C_DEV_CACHE_SIZE      8

/** Mutex wait timeout — must exceed worst-case transaction. */
#define I2C_MUTEX_TIMEOUT_MS    1000

/* ── Types ───────────────────────────────────────────────────────────── */

/** Cached device handle for a specific I2C address on a bus. */
typedef struct {
    uint8_t                   addr;
    i2c_master_dev_handle_t   handle;
    bool                      valid;
} i2c_dev_cache_entry_t;

/** Complete state for one I2C bus. */
typedef struct {
    bool                      initialized;
    bool                      has_failed;
    i2c_master_bus_handle_t   bus_handle;
    i2c_dev_cache_entry_t     dev_cache[I2C_DEV_CACHE_SIZE];
    uint32_t                  dev_count;
    uint32_t                  error_count;
    uint32_t                  consecutive_errors;
    SemaphoreHandle_t         mutex;
    uint8_t                   sda_pin;
    uint8_t                   scl_pin;
    uint32_t                  freq_hz;
} i2c_bus_state_t;

/* Justification: Per-bus state must persist across all function calls.
 * One entry per physical I2C bus.  Static file scope. */
static i2c_bus_state_t s_bus[I2C_BUS_COUNT];

/* ── Forward declarations ────────────────────────────────────────────── */
static error_code_t             map_esp_err(esp_err_t err);
static void                     handle_bus_error(i2c_bus_state_t *state,
                                                 i2c_bus_id_t bus);
static error_code_t             bus_recover_internal(i2c_bus_state_t *state,
                                                     i2c_bus_id_t bus);
static i2c_master_dev_handle_t  get_or_create_dev(i2c_bus_state_t *state,
                                                   uint8_t addr);
static void                     clear_dev_cache(i2c_bus_state_t *state);

/* ═══════════════════════════════════════════════════════════════════════
 *  map_esp_err — Convert ESP-IDF error to project error_code_t.
 * ═══════════════════════════════════════════════════════════════════════ */
static error_code_t map_esp_err(esp_err_t err)
{
    switch (err) {
        case ESP_OK:                return ERR_OK;
        case ESP_ERR_TIMEOUT:       return ERR_HW_TIMEOUT;
        case ESP_ERR_INVALID_ARG:   return ERR_INVALID_ARG;
        case ESP_ERR_INVALID_STATE: return ERR_NOT_INITIALIZED;
        case ESP_ERR_NOT_FOUND:     return ERR_HW_NOT_FOUND;
        default:                    return ERR_HW_WRITE_FAILED;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  handle_bus_error — Track errors, trigger auto-recovery at threshold.
 *
 *  @pre   Caller holds state->mutex.
 * ═══════════════════════════════════════════════════════════════════════ */
static void handle_bus_error(i2c_bus_state_t *state, i2c_bus_id_t bus)
{
    state->error_count++;
    state->consecutive_errors++;

    if (state->consecutive_errors >= I2C_ERROR_THRESHOLD) {
        ESP_LOGE(TAG, "Bus %d: %lu consecutive errors — recovering",
                 (int)bus, (unsigned long)state->consecutive_errors);
        bus_recover_internal(state, bus);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  clear_dev_cache — Free all cached device handles.
 *
 *  @pre   Caller holds state->mutex.  Bus handle may be NULL.
 * ═══════════════════════════════════════════════════════════════════════ */
static void clear_dev_cache(i2c_bus_state_t *state)
{
    for (uint32_t i = 0; i < I2C_DEV_CACHE_SIZE; i++) {
        if (state->dev_cache[i].valid &&
            state->dev_cache[i].handle != NULL) {
            i2c_master_bus_rm_device(state->dev_cache[i].handle);
        }
        state->dev_cache[i].valid  = false;
        state->dev_cache[i].handle = NULL;
        state->dev_cache[i].addr   = 0;
    }
    state->dev_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  get_or_create_dev — Find cached handle or create new one.
 *
 *  WHY lazy creation: Our public API takes address per call, but the
 *  new ESP-IDF API needs a device handle.  We cache handles so
 *  allocation only happens once per unique address.  After warm-up
 *  (first few seconds), no further allocations occur.
 *
 *  @pre   Caller holds state->mutex.  Bus must be initialized.
 *  @return Device handle, or NULL on failure.
 * ═══════════════════════════════════════════════════════════════════════ */
static i2c_master_dev_handle_t get_or_create_dev(i2c_bus_state_t *state,
                                                  uint8_t addr)
{
    /* Search cache — bounded loop. */
    for (uint32_t i = 0; i < I2C_DEV_CACHE_SIZE; i++) {
        if (state->dev_cache[i].valid &&
            state->dev_cache[i].addr == addr) {
            return state->dev_cache[i].handle;
        }
    }

    /* Cache miss — create new device handle. */
    if (state->dev_count >= I2C_DEV_CACHE_SIZE) {
        ESP_LOGE(TAG, "Device cache full, cannot add 0x%02X", addr);
        return NULL;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = state->freq_hz,
    };

    i2c_master_dev_handle_t dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(state->bus_handle,
                                               &dev_cfg, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Add device 0x%02X failed: %s",
                 addr, esp_err_to_name(err));
        return NULL;
    }

    /* Store in first available slot. */
    for (uint32_t i = 0; i < I2C_DEV_CACHE_SIZE; i++) {
        if (!state->dev_cache[i].valid) {
            state->dev_cache[i].addr   = addr;
            state->dev_cache[i].handle = dev;
            state->dev_cache[i].valid  = true;
            state->dev_count++;
            return dev;
        }
    }

    /* Should never reach here — dev_count check above prevents it. */
    i2c_master_bus_rm_device(dev);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  bus_recover_internal — Bit-bang SCL recovery + reinit.
 *
 *  Recovery sequence:
 *    1. Free all cached device handles
 *    2. Delete bus driver (releases GPIO pins)
 *    3. Bit-bang SCL 16 times to clock out stuck slave
 *    4. Generate STOP condition
 *    5. Recreate bus driver
 *    6. Reset error counters
 *
 *  @pre   Caller holds state->mutex.
 * ═══════════════════════════════════════════════════════════════════════ */
static error_code_t bus_recover_internal(i2c_bus_state_t *state,
                                         i2c_bus_id_t bus)
{
    ESP_LOGW(TAG, "Bus %d: recovery started", (int)bus);

    /* Step 1: Remove all cached device handles. */
    clear_dev_cache(state);

    /* Step 2: Delete bus (releases GPIO to default state). */
    if (state->bus_handle != NULL) {
        i2c_del_master_bus(state->bus_handle);
        state->bus_handle = NULL;
    }
    state->initialized = false;

    /* Step 3: Bit-bang SCL to clock out stuck slave. */
    gpio_reset_pin(state->scl_pin);
    gpio_reset_pin(state->sda_pin);
    gpio_set_direction(state->sda_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(state->scl_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(state->sda_pin, 1);
    gpio_set_level(state->scl_pin, 1);

    for (int i = 0; i < I2C_BUS_RECOVERY_CLOCK_PULSES; i++) {
        gpio_set_level(state->scl_pin, 0);
        esp_rom_delay_us(5);
        gpio_set_level(state->scl_pin, 1);
        esp_rom_delay_us(5);
    }

    /* Step 4: Generate STOP — SDA low→high while SCL high. */
    gpio_set_level(state->sda_pin, 0);
    esp_rom_delay_us(5);
    gpio_set_level(state->sda_pin, 1);
    esp_rom_delay_us(5);

    /* Step 5: Recreate bus. */
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port   = (bus == I2C_BUS_SENSORS) ? I2C_NUM_0 : I2C_NUM_1,
        .scl_io_num = state->scl_pin,
        .sda_io_num = state->sda_pin,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &state->bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bus %d: recovery FAILED (%s)",
                 (int)bus, esp_err_to_name(err));
        state->has_failed = true;
        return ERR_HW_INIT_FAILED;
    }

    /* Step 6: Reset error counters. */
    state->consecutive_errors = 0;
    state->has_failed = false;
    state->initialized = true;
    ESP_LOGI(TAG, "Bus %d: recovery OK", (int)bus);
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_init(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT) {
        return ERR_INVALID_ARG;
    }

    i2c_bus_state_t *state = &s_bus[bus];

    if (state->initialized) {
        return ERR_ALREADY_INITIALIZED;
    }

    /* Populate bus-specific configuration. */
    if (bus == I2C_BUS_SENSORS) {
        state->sda_pin = PIN_I2C_BUS0_SDA;
        state->scl_pin = PIN_I2C_BUS0_SCL;
        state->freq_hz = I2C_BUS0_FREQ_HZ;
    } else {
        state->sda_pin = PIN_I2C_BUS1_SDA;
        state->scl_pin = PIN_I2C_BUS1_SCL;
        state->freq_hz = I2C_BUS1_FREQ_HZ;
    }

    /* Create mutex for thread-safe bus access. */
    state->mutex = xSemaphoreCreateMutex();
    if (state->mutex == NULL) {
        ESP_LOGE(TAG, "Bus %d: mutex creation failed", (int)bus);
        return ERR_HW_INIT_FAILED;
    }

    /* Create the I2C master bus. */
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port   = (bus == I2C_BUS_SENSORS) ? I2C_NUM_0 : I2C_NUM_1,
        .scl_io_num = state->scl_pin,
        .sda_io_num = state->sda_pin,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &state->bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bus %d: init failed (%s)",
                 (int)bus, esp_err_to_name(err));
        vSemaphoreDelete(state->mutex);
        state->mutex = NULL;
        return ERR_HW_INIT_FAILED;
    }

    /* Clear device cache and error counters. */
    memset(state->dev_cache, 0, sizeof(state->dev_cache));
    state->dev_count          = 0;
    state->error_count        = 0;
    state->consecutive_errors = 0;
    state->has_failed         = false;
    state->initialized        = true;

    ESP_LOGI(TAG, "Bus %d OK: SDA=%u SCL=%u %luHz",
             (int)bus, state->sda_pin, state->scl_pin,
             (unsigned long)state->freq_hz);
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_deinit
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_deinit(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_ARG; }

    i2c_bus_state_t *state = &s_bus[bus];
    if (!state->initialized) { return ERR_NOT_INITIALIZED; }

    clear_dev_cache(state);

    if (state->bus_handle != NULL) {
        i2c_del_master_bus(state->bus_handle);
        state->bus_handle = NULL;
    }
    state->initialized = false;

    if (state->mutex != NULL) {
        vSemaphoreDelete(state->mutex);
        state->mutex = NULL;
    }

    ESP_LOGI(TAG, "Bus %d deinitialised", (int)bus);
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_write
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_write(i2c_bus_id_t bus, uint8_t dev_addr,
                           const uint8_t *data, size_t len,
                           uint32_t timeout_ms)
{
    if (bus >= I2C_BUS_COUNT)      { return ERR_INVALID_ARG; }
    if (data == NULL && len > 0)   { return ERR_NULL_POINTER; }

    i2c_bus_state_t *state = &s_bus[bus];
    if (!state->initialized)       { return ERR_NOT_INITIALIZED; }
    if (state->has_failed)         { return ERR_HW_INIT_FAILED; }

    if (xSemaphoreTake(state->mutex,
                       pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ERR_TIMEOUT;
    }

    i2c_master_dev_handle_t dev = get_or_create_dev(state, dev_addr);
    if (dev == NULL) {
        xSemaphoreGive(state->mutex);
        return ERR_HW_NOT_FOUND;
    }

    esp_err_t err = i2c_master_transmit(dev, data, len,
                                         (int)timeout_ms);
    error_code_t result;

    if (err == ESP_OK) {
        state->consecutive_errors = 0;
        result = ERR_OK;
    } else {
        ESP_LOGW(TAG, "Bus %d write 0x%02X fail: %s",
                 (int)bus, dev_addr, esp_err_to_name(err));
        result = map_esp_err(err);
        handle_bus_error(state, bus);
    }

    xSemaphoreGive(state->mutex);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_read
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_read(i2c_bus_id_t bus, uint8_t dev_addr,
                          uint8_t *data, size_t len,
                          uint32_t timeout_ms)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_ARG; }
    if (data == NULL)         { return ERR_NULL_POINTER; }

    i2c_bus_state_t *state = &s_bus[bus];
    if (!state->initialized)  { return ERR_NOT_INITIALIZED; }
    if (state->has_failed)    { return ERR_HW_INIT_FAILED; }

    if (xSemaphoreTake(state->mutex,
                       pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ERR_TIMEOUT;
    }

    i2c_master_dev_handle_t dev = get_or_create_dev(state, dev_addr);
    if (dev == NULL) {
        xSemaphoreGive(state->mutex);
        return ERR_HW_NOT_FOUND;
    }

    esp_err_t err = i2c_master_receive(dev, data, len,
                                        (int)timeout_ms);
    error_code_t result;

    if (err == ESP_OK) {
        state->consecutive_errors = 0;
        result = ERR_OK;
    } else {
        ESP_LOGW(TAG, "Bus %d read 0x%02X fail: %s",
                 (int)bus, dev_addr, esp_err_to_name(err));
        result = map_esp_err(err);
        handle_bus_error(state, bus);
    }

    xSemaphoreGive(state->mutex);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_write_then_read
 *
 *  WHY: ADS1115 and SHT45 require register-write then data-read
 *  in a single transaction with repeated START (no STOP between).
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_write_then_read(i2c_bus_id_t bus, uint8_t dev_addr,
                                     const uint8_t *write_data,
                                     size_t write_len,
                                     uint8_t *read_data,
                                     size_t read_len,
                                     uint32_t timeout_ms)
{
    if (bus >= I2C_BUS_COUNT)                  { return ERR_INVALID_ARG; }
    if (write_data == NULL && write_len > 0)   { return ERR_NULL_POINTER; }
    if (read_data  == NULL && read_len  > 0)   { return ERR_NULL_POINTER; }

    i2c_bus_state_t *state = &s_bus[bus];
    if (!state->initialized)                   { return ERR_NOT_INITIALIZED; }
    if (state->has_failed)                     { return ERR_HW_INIT_FAILED; }

    if (xSemaphoreTake(state->mutex,
                       pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ERR_TIMEOUT;
    }

    i2c_master_dev_handle_t dev = get_or_create_dev(state, dev_addr);
    if (dev == NULL) {
        xSemaphoreGive(state->mutex);
        return ERR_HW_NOT_FOUND;
    }

    esp_err_t err = i2c_master_transmit_receive(
        dev, write_data, write_len,
        read_data, read_len, (int)timeout_ms);

    error_code_t result;

    if (err == ESP_OK) {
        state->consecutive_errors = 0;
        result = ERR_OK;
    } else {
        ESP_LOGW(TAG, "Bus %d w+r 0x%02X fail: %s",
                 (int)bus, dev_addr, esp_err_to_name(err));
        result = map_esp_err(err);
        handle_bus_error(state, bus);
    }

    xSemaphoreGive(state->mutex);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_probe
 *
 *  WHY: The new API has i2c_master_probe() built in — no device
 *  handle needed.  Used during init to detect LCD for role assignment.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_probe(i2c_bus_id_t bus, uint8_t dev_addr)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_ARG; }

    i2c_bus_state_t *state = &s_bus[bus];
    if (!state->initialized)  { return ERR_NOT_INITIALIZED; }
    if (state->has_failed)    { return ERR_HW_INIT_FAILED; }

    if (xSemaphoreTake(state->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ERR_TIMEOUT;
    }

    esp_err_t err = i2c_master_probe(state->bus_handle, dev_addr, 100);

    xSemaphoreGive(state->mutex);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Bus %d: device 0x%02X found",
                 (int)bus, dev_addr);
        return ERR_OK;
    }

    ESP_LOGD(TAG, "Bus %d: device 0x%02X not found",
             (int)bus, dev_addr);
    return ERR_HW_NOT_FOUND;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_recover_bus — Public recovery entry point.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_recover_bus(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_ARG; }

    i2c_bus_state_t *state = &s_bus[bus];
    if (state->mutex == NULL) { return ERR_NOT_INITIALIZED; }

    if (xSemaphoreTake(state->mutex,
                       pdMS_TO_TICKS(I2C_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ERR_TIMEOUT;
    }

    ESP_LOGW(TAG, "Bus %d: manual recovery requested", (int)bus);
    error_code_t result = bus_recover_internal(state, bus);

    xSemaphoreGive(state->mutex);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_get_error_count
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_get_error_count(i2c_bus_id_t bus, uint32_t *count_out)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_ARG; }
    if (count_out == NULL)    { return ERR_NULL_POINTER; }

    *count_out = s_bus[bus].error_count;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_reset_error_count
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_i2c_reset_error_count(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT) { return ERR_INVALID_ARG; }

    s_bus[bus].error_count        = 0;
    s_bus[bus].consecutive_errors = 0;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_i2c_is_initialized
 * ═══════════════════════════════════════════════════════════════════════ */
bool hal_i2c_is_initialized(i2c_bus_id_t bus)
{
    if (bus >= I2C_BUS_COUNT) { return false; }
    return s_bus[bus].initialized && !s_bus[bus].has_failed;
}