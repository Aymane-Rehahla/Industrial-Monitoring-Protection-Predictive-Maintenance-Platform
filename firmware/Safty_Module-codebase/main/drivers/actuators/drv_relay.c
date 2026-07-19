#include "drivers/actuators/drv_relay.h"
#include "app_config.h"
#include "hal/hal_gpio.h"

#include "esp_log.h"

static const char *TAG = "drv_relay";

static bool s_initialized = false;
static bool s_commanded = false;

error_code_t drv_relay_init(void)
{
    error_code_t rc = hal_gpio_config_output(PIN_RELAY_DRIVE, false);
    if (rc != ERR_OK) {
        hal_gpio_force_relay_safe();
        return rc;
    }

    rc = hal_gpio_config_input(PIN_RELAY_READBACK, false);
    if (rc != ERR_OK) {
        hal_gpio_force_relay_safe();
        return rc;
    }

    s_commanded = false;
    s_initialized = true;
    ESP_LOGI(TAG, "Relay drive GPIO %d OFF, readback GPIO %d ready",
             PIN_RELAY_DRIVE, PIN_RELAY_READBACK);
    return ERR_OK;
}

error_code_t drv_relay_set(bool on)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    error_code_t rc = hal_gpio_write(PIN_RELAY_DRIVE, on);
    if (rc != ERR_OK) {
        hal_gpio_force_relay_safe();
        s_commanded = false;
        return rc;
    }

    s_commanded = on;
    return ERR_OK;
}

error_code_t drv_relay_get_commanded(bool *on_out)
{
    if (on_out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    *on_out = s_commanded;
    return ERR_OK;
}

error_code_t drv_relay_get_confirmed(bool *on_out)
{
    if (on_out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    return hal_gpio_read(PIN_RELAY_READBACK, on_out);
}

void drv_relay_force_safe(void)
{
    hal_gpio_force_relay_safe();
    s_commanded = false;
}
