#include "core/system_status.h"
#include "app_config.h"
#include "drivers/actuators/drv_relay.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "system_status";

static system_state_t s_state = SYS_STATE_BOOT;
static device_role_t  s_role = ROLE_UNKNOWN;
static bool           s_relay_commanded = false;
static uint32_t       s_error_count = 0;
static uint32_t       s_warning_count = 0;
static uint32_t       s_boot_count = 1;
static bool           s_initialized = false;
static security_mode_t s_security_mode = SECURITY_NORMAL;

error_code_t system_status_init(void)
{
    s_state = SYS_STATE_READY;
    s_role = ROLE_UNKNOWN;
    s_relay_commanded = false;
    s_error_count = 0;
    s_warning_count = 0;
    s_boot_count = 1;
    s_initialized = true;

    bool commanded = false;
    if (drv_relay_get_commanded(&commanded) == ERR_OK) {
        s_relay_commanded = commanded;
    }

    ESP_LOGI(TAG, "system_status init: relay commanded=%d",
             s_relay_commanded);
    return ERR_OK;
}

error_code_t system_status_get_snapshot(system_snapshot_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }

    bool commanded = s_relay_commanded;
    bool confirmed = false;
    if (drv_relay_get_commanded(&commanded) == ERR_OK) {
        s_relay_commanded = commanded;
    }
    (void)drv_relay_get_confirmed(&confirmed);

    out->magic               = STATUS_MAGIC;
    out->state               = s_state;
    out->role                = s_role;
    out->uptime_seconds      = system_status_get_uptime_seconds();
    out->boot_count          = s_boot_count;
    out->relay_commanded     = commanded;
    out->relay_confirmed     = confirmed;
    out->error_count         = s_error_count;
    out->warning_count       = s_warning_count;
    out->free_heap_bytes     = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    out->min_free_heap_bytes = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    return ERR_OK;
}

system_state_t system_status_get_state(void)
{
    return s_state;
}

device_role_t system_status_get_role(void)
{
    return s_role;
}

uint32_t system_status_get_uptime_seconds(void)
{
    return (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
}

bool system_status_is_relay_on(void)
{
    bool commanded = s_relay_commanded;
    if (drv_relay_get_commanded(&commanded) == ERR_OK) {
        s_relay_commanded = commanded;
    }
    return s_relay_commanded;
}

error_code_t system_status_set_state(system_state_t new_state)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (new_state > SYS_STATE_FAULT) { return ERR_INVALID_ARG; }
    s_state = new_state;
    return ERR_OK;
}

error_code_t system_status_set_role(device_role_t role)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (role > ROLE_SILENT) { return ERR_INVALID_ARG; }
    s_role = role;
    return ERR_OK;
}

error_code_t system_status_set_relay(bool commanded)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    error_code_t rc = drv_relay_set(commanded);
    if (rc == ERR_OK) {
        s_relay_commanded = commanded;
    } else {
        s_relay_commanded = false;
    }
    return rc;
}

error_code_t system_status_increment_errors(void)
{
    s_error_count++;
    return ERR_OK;
}

error_code_t system_status_increment_warnings(void)
{
    s_warning_count++;
    return ERR_OK;
}

security_mode_t system_status_get_security_mode(void)
{
    return s_security_mode;
}

error_code_t system_status_set_security_mode(security_mode_t mode)
{
    if (mode > SECURITY_CUSTOM) { return ERR_INVALID_ARG; }
    s_security_mode = mode;
    return ERR_OK;
}
