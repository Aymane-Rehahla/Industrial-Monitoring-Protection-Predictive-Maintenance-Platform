/**
 * @file    nvs_config.c
 * @brief   NVS wrapper — identical logic to S3, different namespace.
 * @version 1.0.0
 */

#include "nvs_config.h"
#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "nvs_config";
static bool s_initialized = false;

error_code_t nvs_config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt — erasing");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }
    s_initialized = true;
    ESP_LOGI(TAG, "NVS ready — ns \"%s\"", NVS_NAMESPACE);
    return ERR_OK;
}

error_code_t nvs_config_read_u8(const char *key, uint8_t *value)
{
    if (!key || !value) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    esp_err_t e = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (e != ESP_OK) return (e == ESP_ERR_NVS_NOT_FOUND) ? ERR_NVS_NOT_FOUND : ERR_NVS_READ_FAILED;
    e = nvs_get_u8(h, key, value);
    nvs_close(h);
    if (e == ESP_ERR_NVS_NOT_FOUND) return ERR_NVS_NOT_FOUND;
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_READ_FAILED;
}

error_code_t nvs_config_write_u8(const char *key, uint8_t value)
{
    if (!key) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return ERR_NVS_WRITE_FAILED;
    esp_err_t e = nvs_set_u8(h, key, value);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

error_code_t nvs_config_read_u32(const char *key, uint32_t *value)
{
    if (!key || !value) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    esp_err_t e = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (e != ESP_OK) return (e == ESP_ERR_NVS_NOT_FOUND) ? ERR_NVS_NOT_FOUND : ERR_NVS_READ_FAILED;
    e = nvs_get_u32(h, key, value);
    nvs_close(h);
    if (e == ESP_ERR_NVS_NOT_FOUND) return ERR_NVS_NOT_FOUND;
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_READ_FAILED;
}

error_code_t nvs_config_write_u32(const char *key, uint32_t value)
{
    if (!key) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return ERR_NVS_WRITE_FAILED;
    esp_err_t e = nvs_set_u32(h, key, value);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

error_code_t nvs_config_read_blob(const char *key, void *buf, size_t *buf_size)
{
    if (!key || !buf || !buf_size) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    esp_err_t e = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (e != ESP_OK) return ERR_NVS_READ_FAILED;
    e = nvs_get_blob(h, key, buf, buf_size);
    nvs_close(h);
    if (e == ESP_ERR_NVS_NOT_FOUND) return ERR_NVS_NOT_FOUND;
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_READ_FAILED;
}

error_code_t nvs_config_write_blob(const char *key, const void *data, size_t len)
{
    if (!key || !data) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return ERR_NVS_WRITE_FAILED;
    esp_err_t e = nvs_set_blob(h, key, data, len);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

error_code_t nvs_config_erase_key(const char *key)
{
    if (!key) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return ERR_NVS_WRITE_FAILED;
    esp_err_t e = nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
    if (e == ESP_ERR_NVS_NOT_FOUND) return ERR_NVS_NOT_FOUND;
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}

error_code_t nvs_config_erase_all(void)
{
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return ERR_NVS_WRITE_FAILED;
    esp_err_t e = nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
    return (e == ESP_OK) ? ERR_OK : ERR_NVS_WRITE_FAILED;
}