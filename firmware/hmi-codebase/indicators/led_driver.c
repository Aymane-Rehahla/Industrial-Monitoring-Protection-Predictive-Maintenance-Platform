/**
 * @file    led_driver.c
 * @brief   4-LED GPIO driver with blink support.
 * @version 1.0.0
 */

#include "indicators/led_driver.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "led_driver";

static const uint8_t s_pins[LED_COUNT] = {
    PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_ORANGE, PIN_LED_RED
};

/* Justification: LED mode and blink state persist across ticks.
 * Single writer (LED task), read by tick function. */
static led_mode_t s_mode[LED_COUNT];
static bool       s_blink_state[LED_COUNT];
static uint32_t   s_blink_counter[LED_COUNT];

error_code_t led_driver_init(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << s_pins[i]),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE
        };
        gpio_config(&cfg);
        gpio_set_level(s_pins[i], 0);
        s_mode[i]          = LED_OFF;
        s_blink_state[i]   = false;
        s_blink_counter[i] = 0;
    }

    ESP_LOGI(TAG, "LEDs initialised — pins %d,%d,%d,%d",
             s_pins[0], s_pins[1], s_pins[2], s_pins[3]);
    return ERR_OK;
}

void led_driver_set(led_id_t led, led_mode_t mode)
{
    if (led >= LED_COUNT) { return; }
    s_mode[led] = mode;
    s_blink_counter[led] = 0;

    if (mode == LED_OFF) {
        gpio_set_level(s_pins[led], 0);
        s_blink_state[led] = false;
    } else if (mode == LED_ON) {
        gpio_set_level(s_pins[led], 1);
        s_blink_state[led] = true;
    }
}

void led_driver_all_off(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        led_driver_set((led_id_t)i, LED_OFF);
    }
}

void led_driver_tick(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        if (s_mode[i] != LED_BLINK_SLOW && s_mode[i] != LED_BLINK_FAST) {
            continue;
        }

        uint32_t period = (s_mode[i] == LED_BLINK_SLOW)
                          ? (LED_BLINK_SLOW_MS / LED_UPDATE_INTERVAL_MS)
                          : (LED_BLINK_FAST_MS / LED_UPDATE_INTERVAL_MS);

        s_blink_counter[i]++;
        if (s_blink_counter[i] >= period) {
            s_blink_counter[i] = 0;
            s_blink_state[i] = !s_blink_state[i];
            gpio_set_level(s_pins[i], s_blink_state[i] ? 1 : 0);
        }
    }
}