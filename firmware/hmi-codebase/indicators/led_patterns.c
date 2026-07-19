/**
 * @file    led_patterns.c
 * @brief   LED pattern logic — priority-based pattern selection.
 * @version 1.0.0
 */

#include "indicators/led_patterns.h"
#include "indicators/led_driver.h"
#include "esp_log.h"

static const char *TAG = "led_patterns";

error_code_t led_patterns_init(void)
{
    led_driver_set(LED_GREEN, LED_BLINK_SLOW);
    led_driver_set(LED_YELLOW, LED_OFF);
    led_driver_set(LED_ORANGE, LED_OFF);
    led_driver_set(LED_RED, LED_OFF);
    ESP_LOGI(TAG, "LED patterns init — boot blink");
    return ERR_OK;
}

void led_patterns_update(bool s3a_online, bool s3b_online,
                         bool peer_online, bool ipad_connected,
                         bool data_mismatch, bool fault_active,
                         bool fault_critical)
{
    /* ── RED: fault status (highest priority) ── */
    if (fault_critical) {
        led_driver_set(LED_RED, LED_BLINK_FAST);
    } else if (fault_active) {
        led_driver_set(LED_RED, LED_BLINK_SLOW);
    } else {
        led_driver_set(LED_RED, LED_OFF);
    }

    /* ── ORANGE: data validation ── */
    if (data_mismatch) {
        led_driver_set(LED_ORANGE, LED_BLINK_FAST);
    } else {
        led_driver_set(LED_ORANGE, LED_OFF);
    }

    /* ── YELLOW: communication health ── */
    if (!s3a_online && !s3b_online) {
        led_driver_set(LED_YELLOW, LED_ON);
    } else if (!s3a_online || !s3b_online || !peer_online) {
        led_driver_set(LED_YELLOW, LED_BLINK_SLOW);
    } else {
        led_driver_set(LED_YELLOW, LED_OFF);
    }

    /* ── GREEN: overall system status ── */
    if (!s3a_online && !s3b_online) {
        led_driver_set(LED_GREEN, LED_OFF);
    } else if (!ipad_connected) {
        led_driver_set(LED_GREEN, LED_BLINK_SLOW);
    } else {
        led_driver_set(LED_GREEN, LED_ON);
    }
}