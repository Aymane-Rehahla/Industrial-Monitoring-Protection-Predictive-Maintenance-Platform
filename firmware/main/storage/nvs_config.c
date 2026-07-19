/**
 * @file    nvs_config.c
 * @brief   NVS abstraction — wraps ESP-IDF NVS with error_code_t.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Persistent storage for thresholds, MACs, calibration.
 *
 * WHY a wrapper instead of calling nvs_* directly:
 *   1. Centralises the namespace — cannot typo it in 10 different files.
 *   2. Maps esp_err_t → error_code_t for consistent error handling.
 *   3. Handles open/commit/close boilerplate once.
 *   4. Auto-recovery on corrupt partition (erase + re-init).
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "storage/nvs_config.h"
#include "app_config.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "nvs_config";

/* Justification: Tracks one-time init state.  Written once in init,
 * read by every subsequent NVS operation.  File scope. */
static bool s_initialized = false;

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrupt — erasing and reinit");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "NVS initialised — namespace \"%s\"", NVS_NAMESPACE);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_read_u8(const char *key, uint8_t *value)
{
    if (key == NULL || value == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized)              { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ERR_NVS_NOT_FOUND
                                               : ERR_NVS_READ_FAILED;
    }

    err = nvs_get_u8(handle, key, value);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) { return ERR_NVS_NOT_FOUND; }
    if (err != ESP_OK)                { return ERR_NVS_READ_FAILED; }
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_write_u8(const char *key, uint8_t value)
{
    if (key == NULL)    { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) { return ERR_NVS_WRITE_FAILED; }

    err = nvs_set_u8(handle, key, value);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ERR_NVS_WRITE_FAILED;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return (err == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_read_u32(const char *key, uint32_t *value)
{
    if (key == NULL || value == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized)              { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ERR_NVS_NOT_FOUND
                                               : ERR_NVS_READ_FAILED;
    }

    err = nvs_get_u32(handle, key, value);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) { return ERR_NVS_NOT_FOUND; }
    if (err != ESP_OK)                { return ERR_NVS_READ_FAILED; }
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_write_u32(const char *key, uint32_t value)
{
    if (key == NULL)    { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) { return ERR_NVS_WRITE_FAILED; }

    err = nvs_set_u32(handle, key, value);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ERR_NVS_WRITE_FAILED;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return (err == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_read_blob(const char *key, void *buf,
                                  size_t *buf_size)
{
    if (key == NULL || buf == NULL || buf_size == NULL) {
        return ERR_NULL_POINTER;
    }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return (err == ESP_ERR_NVS_NOT_FOUND) ? ERR_NVS_NOT_FOUND
                                               : ERR_NVS_READ_FAILED;
    }

    err = nvs_get_blob(handle, key, buf, buf_size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND)     { return ERR_NVS_NOT_FOUND; }
    if (err == ESP_ERR_NVS_INVALID_LENGTH) { return ERR_INVALID_ARG; }
    if (err != ESP_OK)                     { return ERR_NVS_READ_FAILED; }
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_write_blob(const char *key, const void *data,
                                   size_t len)
{
    if (key == NULL || data == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized)             { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) { return ERR_NVS_WRITE_FAILED; }

    err = nvs_set_blob(handle, key, data, len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ERR_NVS_WRITE_FAILED;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    return (err == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_erase_key(const char *key)
{
    if (key == NULL)    { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) { return ERR_NVS_WRITE_FAILED; }

    err = nvs_erase_key(handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ERR_NVS_NOT_FOUND;
    }

    nvs_commit(handle);
    nvs_close(handle);

    return (err == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t nvs_config_erase_all(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) { return ERR_NVS_WRITE_FAILED; }

    err = nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGW(TAG, "All NVS keys erased — factory reset");
    return (err == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}