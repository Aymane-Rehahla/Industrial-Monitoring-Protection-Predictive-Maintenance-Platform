/**
 * @file  drv_buttons.c
 * @brief 5-button polling driver implementation.
 * @version 1.0.0
 *
 * @safety LOW
 *
 * Rule  2.1:  NULL checks on every public function with pointer args.
 * Rule  2.9:  button_id_t range-checked.
 * Rule  2.11: Array accesses bounds-checked via BTN_COUNT.
 * Rule 14.5:  Software debounce (30 ms hysteresis).
 */
#include "drv_buttons.h"
#include "hal_gpio.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "DRV_BTN";

/* ── Pin map (order matches button_id_t) ─────────────────────────────── */
static const uint8_t PIN_MAP[BTN_COUNT] = {
    [BTN_UP]    = PIN_BTN_UP,
    [BTN_DOWN]  = PIN_BTN_DOWN,
    [BTN_LEFT]  = PIN_BTN_LEFT,
    [BTN_RIGHT] = PIN_BTN_RIGHT,
    [BTN_OK]    = PIN_BTN_OK,
};

/* ── Per-button internal state ───────────────────────────────────────── */
typedef struct {
    bool     raw;                  /* current pin level (active low)   */
    bool     stable;               /* debounced state                  */
    bool     prev_stable;          /* previous cycle's stable state    */
    uint32_t last_change_ms;       /* last raw transition timestamp    */
    uint32_t press_start_ms;       /* when the current press began     */
    bool     repeat_started;       /* true after repeat delay elapsed  */
    uint32_t last_repeat_ms;       /* last repeat event timestamp      */
} btn_internal_t;

static btn_internal_t s_btn[BTN_COUNT];
static void (*s_press_cb)(button_id_t) = NULL;
static bool s_initialized = false;

/* ── Pointer to the right field in buttons_t by id ───────────────────── */

static button_state_t *field_ptr(buttons_t *b, button_id_t id)
{
    switch (id) {
        case BTN_UP:    return &b->up;
        case BTN_DOWN:  return &b->down;
        case BTN_LEFT:  return &b->left;
        case BTN_RIGHT: return &b->right;
        case BTN_OK:    return &b->ok;
        default:        return NULL;
    }
}

/* ── Read raw pin (active-low → returns true when pressed) ───────────── */

static bool read_raw(button_id_t id)
{
    bool level = true; /* default: not pressed */
    hal_gpio_get_input(PIN_MAP[id], &level);
    return !level;     /* invert: LOW = pressed */
}

/* ── Public: Init ────────────────────────────────────────────────────── */

error_code_t buttons_init(void)
{
    ESP_LOGI(TAG, "Initializing buttons...");
    memset(s_btn, 0, sizeof(s_btn));

    for (int i = 0; i < BTN_COUNT; i++) {
        s_btn[i].raw            = false;
        s_btn[i].stable         = false;
        s_btn[i].prev_stable    = false;
        s_btn[i].last_change_ms = get_time_ms();
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Buttons ready (%d buttons)", BTN_COUNT);
    return ERR_OK;
}

/* ── Debounce a single button ────────────────────────────────────────── */

static void debounce_one(button_id_t id, uint32_t now_ms)
{
    btn_internal_t *b = &s_btn[id];
    bool current_raw = read_raw(id);

    /* Detect raw-level change → restart debounce timer */
    if (current_raw != b->raw) {
        b->raw = current_raw;
        b->last_change_ms = now_ms;
    }

    /* Accept new stable state after debounce period */
    uint32_t elapsed = now_ms - b->last_change_ms;
    if (elapsed >= BTN_DEBOUNCE_MS) {
        b->prev_stable = b->stable;
        b->stable = b->raw;
    }
}

/* ── Compute edge and hold flags for one button ──────────────────────── */

static void compute_events(button_id_t id, uint32_t now_ms)
{
    btn_internal_t *b = &s_btn[id];

    /* just_pressed: rising edge */
    if (b->stable && !b->prev_stable) {
        b->press_start_ms  = now_ms;
        b->repeat_started  = false;
        b->last_repeat_ms  = now_ms;

        if (s_press_cb) { s_press_cb(id); }
    }

    /* just_released: falling edge — reset hold tracking */
    if (!b->stable && b->prev_stable) {
        b->repeat_started = false;
    }
}

/* ── Public: Update (call every 10-20 ms) ────────────────────────────── */

error_code_t buttons_update(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    uint32_t now = get_time_ms();

    for (int i = 0; i < BTN_COUNT; i++) {
        debounce_one((button_id_t)i, now);
        compute_events((button_id_t)i, now);
    }

    return ERR_OK;
}

/* ── Public: Get full state snapshot ──────────────────────────────────── */

error_code_t buttons_get_state(buttons_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    memset(out, 0, sizeof(*out));

    uint32_t now = get_time_ms();

    for (int i = 0; i < BTN_COUNT; i++) {
        const btn_internal_t *b = &s_btn[i];
        button_state_t *f = field_ptr(out, (button_id_t)i);
        if (f == NULL) { continue; }  /* Rule 2.1 */

        f->is_pressed    = b->stable;
        f->just_pressed  = b->stable && !b->prev_stable;
        f->just_released = !b->stable && b->prev_stable;
        f->press_start_ms = b->press_start_ms;

        if (b->stable) {
            f->hold_duration_ms = now - b->press_start_ms;
            f->is_held = (f->hold_duration_ms >= BTN_LONG_PRESS_MS);
        }
    }

    return ERR_OK;
}

/* ── Public: Edge queries ────────────────────────────────────────────── */

bool buttons_just_pressed(button_id_t id)
{
    if (id >= BTN_COUNT) { return false; }
    return s_btn[id].stable && !s_btn[id].prev_stable;
}

bool buttons_just_released(button_id_t id)
{
    if (id >= BTN_COUNT) { return false; }
    return !s_btn[id].stable && s_btn[id].prev_stable;
}

bool buttons_is_held(button_id_t id)
{
    if (id >= BTN_COUNT) { return false; }
    if (!s_btn[id].stable) { return false; }
    uint32_t held = get_time_ms() - s_btn[id].press_start_ms;
    return (held >= BTN_LONG_PRESS_MS);
}

/* ── Public: Auto-repeat ─────────────────────────────────────────────── */

bool buttons_should_repeat(button_id_t id)
{
    if (id >= BTN_COUNT) { return false; }
    btn_internal_t *b = &s_btn[id];
    if (!b->stable) { return false; }

    uint32_t held = get_time_ms() - b->press_start_ms;

    if (!b->repeat_started) {
        if (held >= BTN_REPEAT_DELAY_MS) {
            b->repeat_started = true;
            b->last_repeat_ms = get_time_ms();
            return true;
        }
        return false;
    }

    uint32_t since_repeat = get_time_ms() - b->last_repeat_ms;
    if (since_repeat >= BTN_REPEAT_RATE_MS) {
        b->last_repeat_ms = get_time_ms();
        return true;
    }
    return false;
}

/* ── Public: Any pressed ─────────────────────────────────────────────── */

bool buttons_any_pressed(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        if (s_btn[i].stable && !s_btn[i].prev_stable) { return true; }
    }
    return false;
}

/* ── Public: Press callback ──────────────────────────────────────────── */

void buttons_set_press_callback(void (*cb)(button_id_t id))
{
    s_press_cb = cb;
}