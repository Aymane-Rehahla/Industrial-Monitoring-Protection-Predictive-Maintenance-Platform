/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  nvs_config.c - NVS Configuration Implementation                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Triple redundancy: Stores config in 3 slots, uses voting to recover         ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "nvs_config.h"
#include "default_config.h"
#include "crc_utils.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *TAG = "NVS_CFG";

#define NVS_NAMESPACE           "safety"
#define NVS_KEY_CONFIG_1        "cfg1"
#define NVS_KEY_CONFIG_2        "cfg2"
#define NVS_KEY_CONFIG_3        "cfg3"
#define NVS_KEY_FAULT_LOG       "faults"

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool s_initialized = false;
static nvs_stats_t s_stats = {0};

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              PRIVATE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool validate_config(const protection_config_t *cfg)
{
    if (cfg->magic != MAGIC_CONFIG_DATA) {
        return false;
    }
    
    /* Verify CRC */
    uint16_t calc_crc = crc16_calculate(cfg, 
                                         sizeof(protection_config_t) - sizeof(uint16_t));
    if (calc_crc != cfg->checksum) {
        return false;
    }
    
    return default_config_validate(cfg);
}

/* ─────────────────────────────────────────────────────────────────────────────── */

static nvs_result_t read_single_slot(const char *key, protection_config_t *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return NVS_ERR_READ_FAILED;
    }
    
    size_t size = sizeof(protection_config_t);
    err = nvs_get_blob(handle, key, cfg, &size);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        return NVS_ERR_READ_FAILED;
    }
    
    if (size != sizeof(protection_config_t)) {
        return NVS_ERR_CORRUPT;
    }
    
    if (!validate_config(cfg)) {
        return NVS_ERR_INVALID_DATA;
    }
    
    return NVS_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

static nvs_result_t write_single_slot(const char *key, const protection_config_t *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return NVS_ERR_WRITE_FAILED;
    }
    
    err = nvs_set_blob(handle, key, cfg, sizeof(protection_config_t));
    if (err != ESP_OK) {
        nvs_close(handle);
        return NVS_ERR_WRITE_FAILED;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    return (err == ESP_OK) ? NVS_OK : NVS_ERR_WRITE_FAILED;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

nvs_result_t nvs_config_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS...");
    
    esp_err_t err = nvs_flash_init();
    
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || 
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition issue, erasing...");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return NVS_ERR_NOT_INITIALIZED;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "NVS initialized");
    
    return NVS_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_load(protection_config_t *config)
{
    if (config == NULL) {
        return NVS_ERR_INVALID_DATA;
    }
    
    if (!s_initialized) {
        default_config_copy(config);
        return NVS_ERR_NOT_INITIALIZED;
    }
    
    s_stats.loads_total++;
    
    /* Try all 3 slots, use voting */
    protection_config_t slots[3];
    bool slot_valid[3] = {false, false, false};
    const char *keys[3] = {NVS_KEY_CONFIG_1, NVS_KEY_CONFIG_2, NVS_KEY_CONFIG_3};
    
    int valid_count = 0;
    for (int i = 0; i < 3; i++) {
        if (read_single_slot(keys[i], &slots[i]) == NVS_OK) {
            slot_valid[i] = true;
            valid_count++;
        }
    }
    
    ESP_LOGI(TAG, "Config load: %d/3 slots valid", valid_count);
    
    /* No valid config found */
    if (valid_count == 0) {
        ESP_LOGW(TAG, "No valid config, using factory defaults");
        s_stats.corruptions_detected++;
        default_config_copy(config);
        return NVS_ERR_CORRUPT;
    }
    
    /* Use first valid slot */
    for (int i = 0; i < 3; i++) {
        if (slot_valid[i]) {
            memcpy(config, &slots[i], sizeof(protection_config_t));
            s_stats.loads_success++;
            
            /* Repair any corrupt slots */
            if (valid_count < 3) {
                ESP_LOGI(TAG, "Repairing corrupt slots...");
                nvs_config_save(config);
            }
            
            return NVS_OK;
        }
    }
    
    /* Should never reach here */
    default_config_copy(config);
    return NVS_ERR_CORRUPT;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_save(const protection_config_t *config)
{
    if (config == NULL) {
        return NVS_ERR_INVALID_DATA;
    }
    
    if (!s_initialized) {
        return NVS_ERR_NOT_INITIALIZED;
    }
    
    /* Rule 13.2: Validate before saving */
    if (!default_config_validate(config)) {
        ESP_LOGE(TAG, "Config validation failed, refusing to save");
        return NVS_ERR_INVALID_DATA;
    }
    
    s_stats.saves_total++;
    
    /* Prepare config with CRC */
    protection_config_t to_save;
    memcpy(&to_save, config, sizeof(protection_config_t));
    to_save.magic = MAGIC_CONFIG_DATA;
    to_save.checksum = crc16_calculate(&to_save, 
                                        sizeof(protection_config_t) - sizeof(uint16_t));
    
    /* Write to all 3 slots */
    const char *keys[3] = {NVS_KEY_CONFIG_1, NVS_KEY_CONFIG_2, NVS_KEY_CONFIG_3};
    int success_count = 0;
    
    for (int i = 0; i < 3; i++) {
        if (write_single_slot(keys[i], &to_save) == NVS_OK) {
            success_count++;
        }
    }
    
    ESP_LOGI(TAG, "Config saved to %d/3 slots", success_count);
    
    /* Rule 4.7: Verify after write */
    protection_config_t verify;
    if (read_single_slot(NVS_KEY_CONFIG_1, &verify) != NVS_OK) {
        ESP_LOGE(TAG, "Post-write verification FAILED");
        return NVS_ERR_WRITE_FAILED;
    }
    
    if (memcmp(&to_save, &verify, sizeof(protection_config_t)) != 0) {
        ESP_LOGE(TAG, "Post-write data mismatch!");
        return NVS_ERR_WRITE_FAILED;
    }
    
    s_stats.saves_success++;
    return (success_count >= 2) ? NVS_OK : NVS_ERR_WRITE_FAILED;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_factory_reset(void)
{
    ESP_LOGW(TAG, "FACTORY RESET initiated");
    
    s_stats.factory_resets++;
    
    protection_config_t defaults;
    default_config_copy(&defaults);
    
    nvs_result_t result = nvs_config_save(&defaults);
    
    if (result == NVS_OK) {
        ESP_LOGI(TAG, "Factory reset complete");
    } else {
        ESP_LOGE(TAG, "Factory reset FAILED");
    }
    
    return result;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

bool nvs_config_exists(void)
{
    if (!s_initialized) {
        return false;
    }
    
    protection_config_t temp;
    return (read_single_slot(NVS_KEY_CONFIG_1, &temp) == NVS_OK);
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_get_stats(nvs_stats_t *stats)
{
    if (stats == NULL) {
        return NVS_ERR_INVALID_DATA;
    }
    
    memcpy(stats, &s_stats, sizeof(nvs_stats_t));
    return NVS_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_save_fault(const fault_entry_t *fault)
{
    if (fault == NULL || !s_initialized) {
        return NVS_ERR_INVALID_DATA;
    }
    
    /* Load existing log */
    fault_log_t log;
    nvs_config_load_fault_log(&log);
    
    /* Add new fault at head */
    log.entries[log.head] = *fault;
    log.entries[log.head].magic = MAGIC_FAULT_LOG;
    log.head = (log.head + 1) % FAULT_LOG_SIZE;
    if (log.count < FAULT_LOG_SIZE) {
        log.count++;
    }
    log.total_faults++;
    
    /* Save back */
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return NVS_ERR_WRITE_FAILED;
    }
    
    err = nvs_set_blob(handle, NVS_KEY_FAULT_LOG, &log, sizeof(log));
    nvs_commit(handle);
    nvs_close(handle);
    
    return (err == ESP_OK) ? NVS_OK : NVS_ERR_WRITE_FAILED;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_load_fault_log(fault_log_t *log)
{
    if (log == NULL) {
        return NVS_ERR_INVALID_DATA;
    }
    
    memset(log, 0, sizeof(fault_log_t));
    
    if (!s_initialized) {
        return NVS_ERR_NOT_INITIALIZED;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return NVS_OK;  /* Empty log is OK */
    }
    
    size_t size = sizeof(fault_log_t);
    err = nvs_get_blob(handle, NVS_KEY_FAULT_LOG, log, &size);
    nvs_close(handle);
    
    return NVS_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

nvs_result_t nvs_config_clear_fault_log(void)
{
    if (!s_initialized) {
        return NVS_ERR_NOT_INITIALIZED;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return NVS_ERR_WRITE_FAILED;
    }
    
    nvs_erase_key(handle, NVS_KEY_FAULT_LOG);
    nvs_commit(handle);
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Fault log cleared");
    return NVS_OK;
}