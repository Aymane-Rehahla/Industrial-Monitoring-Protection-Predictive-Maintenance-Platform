/**
 * @file  drv_leds.c
 * @brief LED driver implementation.
 * @version 1.0.0
 *
 * @safety LOW
 *
 * Rule 1.7: No magic numbers — blink periods named.
 * Rule 2.9: led_id_t range-checked.
 */
#include "drv_leds.h"
#include "hal_gpio.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"

static const char *TAG = "DRV_LED";

/* ── Blink period definitions (ms) ───────────────────────────────────── */
#define BLINK_SLOW_PERIOD_MS       1000
#define BLINK_FAST_PERIOD_MS       400
#define BLINK_VERY_FAST_PERIOD_MS  150

/* ── Pin map (order matches led_id_t) ────────────────────────────────── */
static const uint8_t LED_PIN[LED_COUNT] = {
    [LED_GREEN] = PIN_LED_GREEN,
    [LED_RED]   = PIN_LED_RED,
};

/* ── Per-LED state ───────────────────────────────────────────────────── */
typedef struct {
    led_mode_t mode;
    bool       output;           /* current physical state */
    uint32_t   last_toggle_ms;
} led_state_t;

static led_state_t s_led[LED_COUNT];
static bool s_initialized = false;

/* ── Get blink period for a mode (0 = no blink) ──────────────────────── */

static uint32_t period_for_mode(led_mode_t mode)
{
    switch (mode) {
        case LED_MODE_BLINK_SLOW:      return BLINK_SLOW_PERIOD_MS;
        case LED_MODE_BLINK_FAST:      return BLINK_FAST_PERIOD_MS;
        case LED_MODE_BLINK_VERY_FAST: return BLINK_VERY_FAST_PERIOD_MS;
        default:                       return 0;
    }
}

/* ── Public: Init ────────────────────────────────────────────────────── */

error_code_t leds_init(void)
{
    ESP_LOGI(TAG, "Initializing LEDs...");

    for (int i = 0; i < LED_COUNT; i++) {
        s_led[i].mode           = LED_MODE_OFF;
        s_led[i].output         = false;
        s_led[i].last_toggle_ms = get_time_ms();
        hal_gpio_set_output(LED_PIN[i], false);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LEDs ready");
    return ERR_OK;
}

/* ── Public: Set mode ────────────────────────────────────────────────── */

error_code_t leds_set_mode(led_id_t id, led_mode_t mode)
{
    if (id >= LED_COUNT) { return ERR_INVALID_PARAMETER; }

    s_led[id].mode = mode;

    /* Immediate update for static modes */
    if (mode == LED_MODE_OFF) {
        s_led[id].output = false;
        hal_gpio_set_output(LED_PIN[id], false);
    } else if (mode == LED_MODE_ON) {
        s_led[id].output = true;
        hal_gpio_set_output(LED_PIN[id], true);
    } else {
        /* Blink modes start ON */
        s_led[id].output = true;
        s_led[id].last_toggle_ms = get_time_ms();
        hal_gpio_set_output(LED_PIN[id], true);
    }

    return ERR_OK;
}

/* ── Public: Update (call periodically) ──────────────────────────────── */

error_code_t leds_update(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    uint32_t now = get_time_ms();

    for (int i = 0; i < LED_COUNT; i++) {
        uint32_t period = period_for_mode(s_led[i].mode);
        if (period == 0) { continue; }  /* static mode — skip */

        uint32_t half = period / 2;
        uint32_t elapsed = now - s_led[i].last_toggle_ms;

        if (elapsed >= half) {
            s_led[i].output = !s_led[i].output;
            s_led[i].last_toggle_ms = now;
            hal_gpio_set_output(LED_PIN[i], s_led[i].output);
        }
    }

    return ERR_OK;
}

/* ── Public: Get mode ────────────────────────────────────────────────── */

led_mode_t leds_get_mode(led_id_t id)
{
    if (id >= LED_COUNT) { return LED_MODE_OFF; }
    return s_led[id].mode;
}

/* ── Public: All off ─────────────────────────────────────────────────── */

error_code_t leds_all_off(void)
{
    for (int i = 0; i < LED_COUNT; i++) {
        s_led[i].mode   = LED_MODE_OFF;
        s_led[i].output = false;
        hal_gpio_set_output(LED_PIN[i], false);
    }
    return ERR_OK;
}

/* ── Public: System state → LED pattern ──────────────────────────────── */

error_code_t leds_set_system_pattern(system_state_t state)
{
    switch (state) {
        case STATE_BOOT:
        case STATE_INIT:
        case STATE_SELF_TEST:
            leds_set_mode(LED_GREEN, LED_MODE_BLINK_SLOW);
            leds_set_mode(LED_RED,   LED_MODE_OFF);
            break;

        case STATE_READY:
        case STATE_RUNNING:
            leds_set_mode(LED_GREEN, LED_MODE_ON);
            leds_set_mode(LED_RED,   LED_MODE_OFF);
            break;

        case STATE_WARNING:
            leds_set_mode(LED_GREEN, LED_MODE_ON);
            leds_set_mode(LED_RED,   LED_MODE_BLINK_SLOW);
            break;

        case STATE_FAULT:
            leds_set_mode(LED_GREEN, LED_MODE_OFF);
            leds_set_mode(LED_RED,   LED_MODE_BLINK_FAST);
            break;

        case STATE_SAFE_MODE:
            leds_set_mode(LED_GREEN, LED_MODE_BLINK_VERY_FAST);
            leds_set_mode(LED_RED,   LED_MODE_BLINK_VERY_FAST);
            break;

        default:
            leds_set_mode(LED_GREEN, LED_MODE_OFF);
            leds_set_mode(LED_RED,   LED_MODE_ON);
            break;
    }

    return ERR_OK;
}