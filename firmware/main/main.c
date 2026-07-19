/**
 * @file    main.c
 * @brief   Production main — init all subsystems in dependency order.
 * @version 4.0.0
 *
 * CHANGELOG:
 *   4.0.0  2025-01-01  Added comm subsystem init (NVS, ESP-NOW, pairing, telemetry).
 *   3.0.0  2025-01-01  Removed GPIO 5 test task — moved to menu.
 *   2.0.0  2025-01-01  Added GPIO 5 test fault button.
 *   1.1.0  2025-01-01  Removed event-stealing debug loop.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "system_types.h"
#include "app_config.h"
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_adc.h"
#include "hal_uart.h"
#include "system_status.h"
#include "config_manager.h"
#include "measurement.h"
#include "protection.h"
#include "fault_handler.h"
#include "redundancy.h"
#include "sensor_manager.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"
#include "drv_buttons.h"
#include "drv_relay.h"
#include "nvs_config.h"
#include "espnow_link.h"
#include "pairing.h"
#include "telemetry.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    error_code_t err;

    ESP_LOGI(TAG, "===== BOOT START =====");
    ESP_LOGI(TAG, "Firmware %s  Built %s %s",
             FIRMWARE_VERSION_STRING, FIRMWARE_BUILD_DATE,
             FIRMWARE_BUILD_TIME);

    /* ── Phase 1: HAL ── */
    hal_gpio_init();
    hal_i2c_init(I2C_BUS_SENSORS);
    hal_i2c_init(I2C_BUS_SHARED);

    err = drv_lcd2004_init();
    ESP_LOGI(TAG, "LCD init: %d  detected=%d", err,
             drv_lcd2004_is_detected());

    err = drv_relay_init();
    ESP_LOGI(TAG, "Relay init: %d", err);

    hal_adc_init();
    hal_uart_init();

    /* ── Phase 2: NVS (must precede WiFi and any NVS consumers) ── */
    err = nvs_config_init();
    ESP_LOGI(TAG, "NVS init: %d", err);

    /* ── Phase 3: Core modules ── */
    system_status_init();
    config_manager_init();
    measurement_init();
    protection_init();
    fault_handler_init();
    redundancy_init();
    sensor_manager_init();

    /* ── Phase 4: Communication (ESP-NOW → pairing → telemetry) ── */
    err = espnow_link_init();
    ESP_LOGI(TAG, "ESP-NOW init: %d", err);

    err = pairing_init();
    ESP_LOGI(TAG, "Pairing init: %d  peers=%u", err,
             pairing_get_peer_count());

    err = telemetry_init();
    ESP_LOGI(TAG, "Telemetry init: %d", err);

    if (err == ERR_OK) {
        telemetry_start();
        ESP_LOGI(TAG, "Telemetry started");
    }

    /* ── Phase 5: HMI (last — everything else must be ready) ── */
    err = hmi_manager_init();
    ESP_LOGI(TAG, "HMI init: %d", err);

    ESP_LOGI(TAG, "===== SYSTEM RUNNING =====");

    /* ── Supervisor loop — watchdog + diagnostics ── */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        telemetry_stats_t tstat;
        telemetry_get_stats(&tstat);

        ESP_LOGI(TAG, "screen=%d role=%d state=%d faults=%lu "
                 "telem_fast=%lu slow=%lu peers=%u",
                 hmi_manager_get_current_screen(),
                 system_status_get_role(),
                 system_status_get_state(),
                 (unsigned long)fault_handler_get_total_count(),
                 (unsigned long)tstat.fast_packets_sent,
                 (unsigned long)tstat.slow_packets_sent,
                 pairing_get_peer_count());
    }
}
