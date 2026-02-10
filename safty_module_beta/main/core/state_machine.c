/**
 * @file  state_machine.c
 * @brief State machine implementation — validated transitions, logging.
 * @version 1.0.0
 *
 * @safety CRITICAL
 *
 * Rule 0.2: Every transition logged with timestamp and reason.
 * Rule 1.1: All functions ≤ 50 lines.
 * Rule 2.11: Array accesses bounds-checked.
 * Rule 5.7: FAULT entry opens relay.
 * Rule 5.10: SAFE_MODE has no exit transitions.
 * Rule 8.4: sm_force_safe_mode uses direct register writes.
 * Rule 8.11: Relay opened before anything else in FAULT/SAFE_MODE.
 */
#include "state_machine.h"
#include "hal_gpio.h"
#include "app_config.h"
#include "time_utils.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "SM";

/* ── Module state ────────────────────────────────────────────────────── */
static volatile system_state_t s_state        = STATE_BOOT;
static uint32_t                s_state_enter   = 0;
static uint32_t                s_boot_time     = 0;
static SemaphoreHandle_t       s_mtx           = NULL;
static bool                    s_initialized   = false;

/* ── Transition history ring buffer ──────────────────────────────────── */
static sm_transition_record_t  s_history[SM_HISTORY_SIZE];
static uint8_t                 s_hist_head  = 0;
static uint8_t                 s_hist_count = 0;

/* ── State name table (Rule 1.4: readable names) ─────────────────────── */
static const char *STATE_NAMES[] = {
    [STATE_BOOT]        = "BOOT",
    [STATE_INIT]        = "INIT",
    [STATE_SELF_TEST]   = "SELF_TEST",
    [STATE_READY]       = "READY",
    [STATE_RUNNING]     = "RUNNING",
    [STATE_WARNING]     = "WARNING",
    [STATE_FAULT]       = "FAULT",
    [STATE_SAFE_MODE]   = "SAFE_MODE",
    [STATE_SLEEP]       = "SLEEP",
    [STATE_CALIBRATION] = "CALIBRATION",
};

/* ── Validate transition (Rule 5.10: SAFE_MODE never exits) ──────────── */

static bool is_valid_transition(system_state_t from, system_state_t to)
{
    /* SAFE_MODE is a dead end (Rule 5.10) */
    if (from == STATE_SAFE_MODE) { return false; }

    /* Any state can go to SAFE_MODE (catastrophic) */
    if (to == STATE_SAFE_MODE) { return true; }

    /* Any operational state can go to FAULT */
    if (to == STATE_FAULT) {
        return (from != STATE_BOOT);
    }

    switch (from) {
        case STATE_BOOT:        return (to == STATE_INIT);
        case STATE_INIT:        return (to == STATE_SELF_TEST);
        case STATE_SELF_TEST:   return (to == STATE_READY);
        case STATE_READY:       return (to == STATE_RUNNING ||
                                        to == STATE_CALIBRATION ||
                                        to == STATE_SLEEP);
        case STATE_RUNNING:     return (to == STATE_READY ||
                                        to == STATE_WARNING ||
                                        to == STATE_SLEEP);
        case STATE_WARNING:     return (to == STATE_RUNNING ||
                                        to == STATE_SLEEP);
        case STATE_FAULT:       return (to == STATE_READY);
        case STATE_CALIBRATION: return (to == STATE_READY);
        case STATE_SLEEP:       return (to == STATE_READY ||
                                        to == STATE_INIT);
        default:                return false;
    }
}

/* ── Record transition in history ring buffer ────────────────────────── */

static void record_transition(system_state_t from, system_state_t to,
                               const char *reason)
{
    sm_transition_record_t *rec = &s_history[s_hist_head];
    rec->from         = from;
    rec->to           = to;
    rec->timestamp_ms = get_time_ms();
    rec->reason       = reason;

    s_hist_head = (s_hist_head + 1) % SM_HISTORY_SIZE;
    if (s_hist_count < SM_HISTORY_SIZE) { s_hist_count++; }
}

/* ── Entry action for FAULT state ────────────────────────────────────── */

static void on_enter_fault(void)
{
    /* Rule 8.11: First action — open relay */
    hal_gpio_set_relay(false);
    ESP_LOGE(TAG, "!!! FAULT STATE — RELAY OPENED !!!");
}

/* ── Entry action for SAFE_MODE ──────────────────────────────────────── */

static void on_enter_safe_mode(void)
{
    /* Rule 8.4: Direct register writes, bypass everything */
    hal_gpio_emergency_safe();
    ESP_LOGE(TAG, "!!! SAFE MODE — ALL OUTPUTS DISABLED !!!");
    ESP_LOGE(TAG, "!!! PHYSICAL RESET REQUIRED !!!");
}

/* ── Entry action dispatch ───────────────────────────────────────────── */

static void execute_entry_action(system_state_t state)
{
    switch (state) {
        case STATE_FAULT:     on_enter_fault();     break;
        case STATE_SAFE_MODE: on_enter_safe_mode(); break;
        case STATE_SLEEP:
            hal_gpio_set_relay(false);
            ESP_LOGI(TAG, "Entering sleep — relay opened, low-power mode");
            break;
        case STATE_CALIBRATION:
            ESP_LOGI(TAG, "Entering calibration — baseline learning active");
            break;
        default:
            break;
    }
}

/* ── Exit action dispatch ────────────────────────────────────────────── */

static void execute_exit_action(system_state_t state)
{
    switch (state) {
        case STATE_FAULT:
            ESP_LOGI(TAG, "Exiting FAULT — fault acknowledged");
            break;
        case STATE_CALIBRATION:
            ESP_LOGI(TAG, "Exiting CALIBRATION");
            break;
        case STATE_SLEEP:
            ESP_LOGI(TAG, "Exiting SLEEP — power restored");
            break;
        default:
            break;
    }
}

/* ── Public: Init ────────────────────────────────────────────────────── */

error_code_t sm_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    if (s_mtx == NULL) { return ERR_MEMORY_CORRUPT; }

    s_state       = STATE_BOOT;
    s_boot_time   = get_time_ms();
    s_state_enter = s_boot_time;
    s_hist_head   = 0;
    s_hist_count  = 0;
    s_initialized = true;

    ESP_LOGI(TAG, "State machine initialized (BOOT)");
    return ERR_OK;
}

/* ── Public: Request transition ──────────────────────────────────────── */

error_code_t sm_request_transition(system_state_t new_state,
                                    const char *reason)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (new_state >= STATE_COUNT) { return ERR_INVALID_PARAMETER; }

    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ERR_I2C_TIMEOUT;  /* reuse timeout code */
    }

    system_state_t old = s_state;

    if (old == new_state) {
        xSemaphoreGive(s_mtx);
        return ERR_OK;  /* already there */
    }

    if (!is_valid_transition(old, new_state)) {
        ESP_LOGW(TAG, "REJECTED: %s → %s (%s)",
                 sm_state_to_string(old),
                 sm_state_to_string(new_state),
                 reason ? reason : "?");
        xSemaphoreGive(s_mtx);
        return ERR_INVALID_PARAMETER;
    }

    /* Rule 0.2: Log before executing */
    ESP_LOGW(TAG, "TRANSITION: %s → %s (%s)",
             sm_state_to_string(old),
             sm_state_to_string(new_state),
             reason ? reason : "?");

    execute_exit_action(old);

    s_state       = new_state;
    s_state_enter = get_time_ms();

    record_transition(old, new_state, reason);
    execute_entry_action(new_state);

    xSemaphoreGive(s_mtx);
    return ERR_OK;
}

/* ── Public: Force safe mode (ISR-safe path) ─────────────────────────── */

error_code_t sm_force_safe_mode(const char *reason)
{
    /* Rule 8.4: Immediate hardware action, no mutex needed */
    hal_gpio_emergency_safe();

    system_state_t old = s_state;
    s_state       = STATE_SAFE_MODE;
    s_state_enter = get_time_ms();

    ESP_LOGE(TAG, "FORCED SAFE MODE: %s → SAFE_MODE (%s)",
             sm_state_to_string(old),
             reason ? reason : "catastrophic");

    /* Best-effort history recording (no mutex) */
    record_transition(old, STATE_SAFE_MODE, reason);

    return ERR_OK;
}

/* ── Public: Getters ─────────────────────────────────────────────────── */

system_state_t sm_get_state(void)
{
    return s_state;  /* atomic read on 32-bit ARM */
}

uint32_t sm_get_state_duration_ms(void)
{
    return get_time_ms() - s_state_enter;
}

bool sm_is_operational(void)
{
    system_state_t s = s_state;
    return (s == STATE_READY   || s == STATE_RUNNING ||
            s == STATE_WARNING || s == STATE_CALIBRATION);
}

bool sm_is_faulted(void)
{
    system_state_t s = s_state;
    return (s == STATE_FAULT || s == STATE_SAFE_MODE);
}

bool sm_can_transition_to(system_state_t target)
{
    return is_valid_transition(s_state, target);
}

const char *sm_state_to_string(system_state_t state)
{
    if (state >= STATE_COUNT) { return "UNKNOWN"; }
    return STATE_NAMES[state];
}

uint32_t sm_get_uptime_seconds(void)
{
    return (get_time_ms() - s_boot_time) / 1000;
}

/* ── Public: History ─────────────────────────────────────────────────── */

error_code_t sm_get_history(sm_transition_record_t *out,
                             size_t max, size_t *count)
{
    if (out == NULL || count == NULL) { return ERR_NULL_POINTER; }

    *count = 0;
    if (s_hist_count == 0) { return ERR_OK; }

    /* Copy oldest-first */
    size_t start;
    if (s_hist_count < SM_HISTORY_SIZE) {
        start = 0;
    } else {
        start = s_hist_head;  /* oldest is at head (just wrapped) */
    }

    size_t to_copy = (s_hist_count < max) ? s_hist_count : max;

    for (size_t i = 0; i < to_copy; i++) {
        size_t idx = (start + i) % SM_HISTORY_SIZE;
        out[i] = s_history[idx];
    }

    *count = to_copy;
    return ERR_OK;
}