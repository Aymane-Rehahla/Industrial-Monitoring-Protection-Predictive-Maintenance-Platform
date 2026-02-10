/**
 * @file  drv_buzzer.c
 * @brief Buzzer pattern engine — non-blocking, priority-aware.
 * @version 1.0.0
 *
 * @safety LOW
 *
 * Patterns are defined as arrays of {on_ms, off_ms} pairs.
 * A pair of {0, 0} marks the end.
 *
 * Rule 1.1: Functions ≤ 50 lines.
 * Rule 1.7: No magic numbers — all durations named.
 * Rule 2.3: All loops bounded (max steps).
 */
#include "drv_buzzer.h"
#include "hal_gpio.h"
#include "app_config.h"
#include "time_utils.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "DRV_BZR";

/* ── Tone step: on_ms=0 && off_ms=0 → end sentinel ──────────────────── */
typedef struct {
    uint16_t on_ms;
    uint16_t off_ms;
} tone_step_t;

/* ── Maximum steps per pattern (Rule 2.3: bounded) ───────────────────── */
#define MAX_STEPS  8

/* ── Pattern definitions ─────────────────────────────────────────────── */

static const tone_step_t PAT_CLICK[] = {
    {15, 0}, {0, 0}
};

static const tone_step_t PAT_CONFIRM[] = {
    {40, 40}, {40, 0}, {0, 0}
};

static const tone_step_t PAT_ERROR[] = {
    {150, 100}, {150, 0}, {0, 0}
};

static const tone_step_t PAT_WARNING[] = {
    {80, 80}, {80, 80}, {80, 0}, {0, 0}
};

static const tone_step_t PAT_ALARM[] = {
    {200, 100}, {200, 100}, {200, 100}, {200, 100}, {200, 0}, {0, 0}
};

static const tone_step_t PAT_STARTUP[] = {
    {50, 50}, {50, 50}, {100, 0}, {0, 0}
};

/* ── Pattern table (order matches beep_pattern_t) ────────────────────── */
static const tone_step_t *PATTERNS[BEEP_PATTERN_COUNT] = {
    [BEEP_CLICK]   = PAT_CLICK,
    [BEEP_CONFIRM] = PAT_CONFIRM,
    [BEEP_ERROR]   = PAT_ERROR,
    [BEEP_WARNING] = PAT_WARNING,
    [BEEP_ALARM]   = PAT_ALARM,
    [BEEP_STARTUP] = PAT_STARTUP,
};

/* ── Priority (higher = wins) ────────────────────────────────────────── */
static const uint8_t PRIORITY[BEEP_PATTERN_COUNT] = {
    [BEEP_CLICK]   = 1,
    [BEEP_CONFIRM] = 2,
    [BEEP_ERROR]   = 3,
    [BEEP_WARNING] = 4,
    [BEEP_ALARM]   = 5,
    [BEEP_STARTUP] = 2,
};

/* ── Module state ────────────────────────────────────────────────────── */
typedef struct {
    const tone_step_t *pattern;     /* current pattern (NULL = idle)   */
    beep_pattern_t     pattern_id;
    uint8_t            step;        /* current step index              */
    bool               in_on_phase; /* true = buzzer ON phase          */
    uint32_t           phase_start; /* timestamp of current phase      */
    bool               enabled;     /* global mute flag                */
    bool               output;      /* current GPIO state              */
} buzzer_state_t;

static buzzer_state_t s_buz;
static bool s_initialized = false;

/* ── Set physical output ─────────────────────────────────────────────── */

static void set_output(bool on)
{
    s_buz.output = on;
    hal_gpio_set_output(PIN_BUZZER, on);
}

/* ── Public: Init ────────────────────────────────────────────────────── */

error_code_t buzzer_init(void)
{
    ESP_LOGI(TAG, "Initializing buzzer...");

    memset(&s_buz, 0, sizeof(s_buz));
    s_buz.enabled = true;
    set_output(false);

    s_initialized = true;
    ESP_LOGI(TAG, "Buzzer ready");
    return ERR_OK;
}

/* ── Public: Play ────────────────────────────────────────────────────── */

error_code_t buzzer_play(beep_pattern_t pattern)
{
    if (pattern >= BEEP_PATTERN_COUNT) { return ERR_INVALID_PARAMETER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (!s_buz.enabled) { return ERR_OK; }  /* muted — silent success */

    /* Priority check: only interrupt if new pattern ≥ current */
    if (s_buz.pattern != NULL) {
        if (PRIORITY[pattern] < PRIORITY[s_buz.pattern_id]) {
            return ERR_OK;  /* lower priority — ignore */
        }
    }

    s_buz.pattern     = PATTERNS[pattern];
    s_buz.pattern_id  = pattern;
    s_buz.step        = 0;
    s_buz.in_on_phase = true;
    s_buz.phase_start = get_time_ms();
    set_output(true);

    return ERR_OK;
}

/* ── Advance to next step or finish ──────────────────────────────────── */

static void advance_step(void)
{
    s_buz.step++;

    /* Rule 2.3: bounded check */
    if (s_buz.step >= MAX_STEPS) {
        s_buz.pattern = NULL;
        set_output(false);
        return;
    }

    const tone_step_t *step = &s_buz.pattern[s_buz.step];

    /* End sentinel */
    if (step->on_ms == 0 && step->off_ms == 0) {
        s_buz.pattern = NULL;
        set_output(false);
        return;
    }

    /* Start ON phase of new step */
    s_buz.in_on_phase = true;
    s_buz.phase_start = get_time_ms();
    set_output(true);
}

/* ── Public: Update (non-blocking state machine) ─────────────────────── */

error_code_t buzzer_update(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (s_buz.pattern == NULL) { return ERR_OK; }  /* idle */

    uint32_t elapsed = get_time_ms() - s_buz.phase_start;
    const tone_step_t *step = &s_buz.pattern[s_buz.step];

    if (s_buz.in_on_phase) {
        /* ON phase expired? */
        if (elapsed >= step->on_ms) {
            if (step->off_ms > 0) {
                /* Transition to OFF phase */
                s_buz.in_on_phase = false;
                s_buz.phase_start = get_time_ms();
                set_output(false);
            } else {
                /* No off time → advance immediately */
                advance_step();
            }
        }
    } else {
        /* OFF phase expired? */
        if (elapsed >= step->off_ms) {
            advance_step();
        }
    }

    return ERR_OK;
}

/* ── Public: Stop ────────────────────────────────────────────────────── */

error_code_t buzzer_stop(void)
{
    s_buz.pattern = NULL;
    set_output(false);
    return ERR_OK;
}

/* ── Public: Is playing ──────────────────────────────────────────────── */

bool buzzer_is_playing(void)
{
    return (s_buz.pattern != NULL);
}

/* ── Public: Global enable/disable ───────────────────────────────────── */

void buzzer_set_enabled(bool enabled)
{
    s_buz.enabled = enabled;
    if (!enabled) { buzzer_stop(); }
}

bool buzzer_is_enabled(void)
{
    return s_buz.enabled;
}