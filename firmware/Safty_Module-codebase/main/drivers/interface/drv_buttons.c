// ═══ FILE: main/drivers/interface/drv_buttons.c ═══
/**
 * @file    drv_buttons.c
 * @brief   Polled button driver with debounce, hold, and auto-repeat.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — informational only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "drivers/interface/drv_buttons.h"
#include "system_types.h"
#include "app_config.h"
#include "hal/hal_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "drv_buttons";

/* ── Event queue depth ───────────────────────────────────────────────── */
#define BTN_QUEUE_DEPTH  16

/* ── GPIO-to-button mapping (static const, ordered by button_id_t) ─── */
static const uint8_t BTN_GPIO_MAP[BTN_COUNT] = {
    [BTN_LEFT]  = PIN_BTN_LEFT,   /* GPIO 10 */
    [BTN_RIGHT] = PIN_BTN_RIGHT,  /* GPIO 11 */
    [BTN_UP]    = PIN_BTN_UP,     /* GPIO 12 */
    [BTN_DOWN]  = PIN_BTN_DOWN,   /* GPIO 21 */
    [BTN_OK]    = PIN_BTN_OK      /* GPIO 42 */
};

/* ── Per-button internal state ───────────────────────────────────────── */
typedef struct {
    bool     raw_level;           /* Last raw GPIO reading               */
    bool     debounced_pressed;   /* Debounced: true = pressed           */
    bool     prev_debounced;      /* Previous scan's debounced state     */
    uint32_t raw_change_ms;       /* When raw state last changed         */
    uint32_t press_start_ms;      /* When button was pressed (debounced) */
    bool     held_sent;           /* HELD event sent this press?         */
    uint32_t last_repeat_ms;      /* Last REPEAT event timestamp         */
} button_internal_t;

/* Justification: Per-button debounce/hold state must persist across scans.
 * Static file scope — only drv_buttons functions access this. */
static button_internal_t s_buttons[BTN_COUNT];

/* Justification: FreeRTOS queue handle for button events.  Created once
 * at init, never deleted.  Read by HMI task via get_event(). */
static QueueHandle_t s_event_queue = NULL;

/* Justification: Module init flag.  Prevents use before GPIO/queue setup. */
static bool s_initialized = false;

/* ── Forward declarations ────────────────────────────────────────────── */
static uint32_t get_tick_ms(void);
static void     push_event(button_id_t btn, button_event_type_t type,
                           uint32_t hold_ms);

/* ═══════════════════════════════════════════════════════════════════════
 *  get_tick_ms — Current time in milliseconds (from FreeRTOS ticks).
 * ═══════════════════════════════════════════════════════════════════════ */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  push_event — Build and enqueue a button event (non-blocking).
 *
 *  WHY non-blocking: If the HMI task falls behind, we drop events
 *  rather than blocking the scan loop.  Buttons are not safety-critical.
 * ═══════════════════════════════════════════════════════════════════════ */
static void push_event(button_id_t btn, button_event_type_t type,
                       uint32_t hold_ms)
{
    if (s_event_queue == NULL) { return; }

    button_event_t evt = {
        .button       = btn,
        .event        = type,
        .hold_time_ms = hold_ms,
        .timestamp_ms = get_tick_ms()
    };

    /* timeout=0 → don't block.  If queue full, event is silently dropped. */
    if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) {
        ESP_LOGD(TAG, "Event queue full — dropped btn %d event %d", btn, type);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_buttons_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_buttons_init(void)
{
    /* Configure each button GPIO as input with pull-up. */
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        error_code_t rc = hal_gpio_config_input(BTN_GPIO_MAP[i], true);
        if (rc != ERR_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO %d for btn %d",
                     BTN_GPIO_MAP[i], i);
            return ERR_HW_INIT_FAILED;
        }

        /* Initialise internal state: all buttons released. */
        s_buttons[i].raw_level         = !BTN_ACTIVE_LEVEL;
        s_buttons[i].debounced_pressed = false;
        s_buttons[i].prev_debounced    = false;
        s_buttons[i].raw_change_ms     = 0;
        s_buttons[i].press_start_ms    = 0;
        s_buttons[i].held_sent         = false;
        s_buttons[i].last_repeat_ms    = 0;
    }

    /* Create event queue — depth 16, created once, never deleted. */
    s_event_queue = xQueueCreate(BTN_QUEUE_DEPTH, sizeof(button_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ERR_HW_INIT_FAILED;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Buttons initialised: %d buttons, queue depth %d",
             BTN_COUNT, BTN_QUEUE_DEPTH);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  scan_one_button — Debounce + event generation for a single button.
 *
 *  WHY separated: Keeps drv_buttons_scan() under 50 lines and makes
 *  the per-button logic independently testable.
 * ═══════════════════════════════════════════════════════════════════════ */
static void scan_one_button(uint8_t idx)
{
    button_internal_t *btn = &s_buttons[idx];
    uint32_t now_ms = get_tick_ms();

    /* Read raw GPIO. Active LOW: pressed when level == BTN_ACTIVE_LEVEL. */
    bool gpio_level = true;
    hal_gpio_read(BTN_GPIO_MAP[idx], &gpio_level);
    bool raw_pressed = (gpio_level == (bool)BTN_ACTIVE_LEVEL);

    /* Detect raw state change → start debounce timer. */
    if (raw_pressed != btn->raw_level) {
        btn->raw_level      = raw_pressed;
        btn->raw_change_ms  = now_ms;
    }

    /* Check if raw state has been stable for BTN_DEBOUNCE_MS. */
    bool stable = ((now_ms - btn->raw_change_ms) >= BTN_DEBOUNCE_MS);
    if (!stable) { return; }

    /* Update debounced state if it differs from raw. */
    btn->prev_debounced = btn->debounced_pressed;
    btn->debounced_pressed = btn->raw_level;

    /* Generate edge events. */
    if (btn->debounced_pressed && !btn->prev_debounced) {
        /* Just pressed. */
        btn->press_start_ms = now_ms;
        btn->held_sent      = false;
        btn->last_repeat_ms = now_ms;
        push_event((button_id_t)idx, BTN_EVENT_PRESSED, 0);
        return;
    }
    if (!btn->debounced_pressed && btn->prev_debounced) {
        /* Just released. */
        uint32_t hold = now_ms - btn->press_start_ms;
        push_event((button_id_t)idx, BTN_EVENT_RELEASED, hold);
        return;
    }

    /* Button held — generate HELD and REPEAT events. */
    if (!btn->debounced_pressed) { return; }

    uint32_t hold_duration = now_ms - btn->press_start_ms;

    if (!btn->held_sent && hold_duration >= BTN_LONG_PRESS_MS) {
        btn->held_sent = true;
        push_event((button_id_t)idx, BTN_EVENT_HELD, hold_duration);
    }

    if (hold_duration >= BTN_REPEAT_DELAY_MS) {
        if ((now_ms - btn->last_repeat_ms) >= BTN_REPEAT_RATE_MS) {
            btn->last_repeat_ms = now_ms;
            push_event((button_id_t)idx, BTN_EVENT_REPEAT, hold_duration);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_buttons_scan
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_buttons_scan(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    /* Bounded loop: exactly BTN_COUNT (5) iterations, never more. */
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        scan_one_button(i);
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_buttons_get_event
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_buttons_get_event(button_event_t *event_out,
                                   uint32_t timeout_ms)
{
    if (event_out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized)    { return ERR_NOT_INITIALIZED; }

    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(s_event_queue, event_out, ticks) == pdTRUE) {
        return ERR_OK;
    }

    /* No event within timeout — return a "nothing happened" event. */
    event_out->button       = BTN_NONE;
    event_out->event        = BTN_EVENT_NONE;
    event_out->hold_time_ms = 0;
    event_out->timestamp_ms = get_tick_ms();

    return ERR_TIMEOUT;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_buttons_is_pressed
 * ═══════════════════════════════════════════════════════════════════════ */
bool drv_buttons_is_pressed(button_id_t btn)
{
    if (btn >= BTN_COUNT) { return false; }
    if (!s_initialized)   { return false; }

    return s_buttons[btn].debounced_pressed;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_buttons_any_pressed
 * ═══════════════════════════════════════════════════════════════════════ */
bool drv_buttons_any_pressed(void)
{
    if (!s_initialized) { return false; }

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (s_buttons[i].debounced_pressed) {
            return true;
        }
    }

    return false;
}