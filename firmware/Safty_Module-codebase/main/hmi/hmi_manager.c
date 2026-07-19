// ═══ FILE: main/hmi/hmi_manager.c ═══
/**
 * @file    hmi_manager.c
 * @brief   Central HMI coordinator — real implementation.
 *          Runs the screen lifecycle, button dispatch, combos,
 *          idle timeout, buzzer feedback, and LED updates.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — HMI is not safety-critical.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/hmi_manager.h"
#include "system_types.h"
#include "app_config.h"

#include "hmi/screens/screens.h"
#include "hmi/led_status.h"
#include "hmi/ui_animations.h"

#include "drivers/interface/drv_lcd2004.h"
#include "drivers/interface/drv_buttons.h"
#include "drivers/actuators/drv_buzzer.h"
#include "drivers/actuators/drv_leds.h"

#include "core/system_status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdatomic.h>

static const char *TAG = "hmi_manager";

/* ── Navigation stack ────────────────────────────────────────────────── */
#define NAV_STACK_MAX  5

/* ── Screen handler table ────────────────────────────────────────────── */
/* Maps each screen_id_t to its lifecycle functions. */

#define HANDLER(name) { \
    screen_##name##_enter,        \
    screen_##name##_update,       \
    screen_##name##_handle_event, \
    screen_##name##_exit          \
}

static const screen_handler_t SCREEN_HANDLERS[SCREEN_COUNT] = {
    [SCREEN_BOOT]          = HANDLER(boot),
    [SCREEN_FAULT]         = HANDLER(fault),
    [SCREEN_HOME]          = HANDLER(home),
    [SCREEN_VOLTAGE]       = HANDLER(voltage),
    [SCREEN_CURRENT]       = HANDLER(current),
    [SCREEN_TEMP]          = HANDLER(temp),
    [SCREEN_GAS]           = HANDLER(gas),
    [SCREEN_VIBRATION]     = HANDLER(vibration),
    [SCREEN_RPM]           = HANDLER(rpm),
    [SCREEN_SYSTEM]        = HANDLER(system),
    [SCREEN_SETTINGS]      = HANDLER(settings),
    [SCREEN_THRESHOLD]     = HANDLER(threshold),
    [SCREEN_SENSOR_VIEW]   = HANDLER(sensor_view),
    [SCREEN_SENSOR_ADD]    = HANDLER(sensor_add),
    [SCREEN_SENSOR_REMOVE] = HANDLER(sensor_remove),
    [SCREEN_SENSOR_TEST]   = HANDLER(sensor_test),
    [SCREEN_CAL_SELECT]    = HANDLER(cal_select),
    [SCREEN_CAL_MANUAL]    = HANDLER(cal_manual),
    [SCREEN_CAL_AUTO]      = HANDLER(cal_auto),
    [SCREEN_PAIRING]       = HANDLER(pairing),
    [SCREEN_MAC_ENTRY]     = HANDLER(mac_entry),
    [SCREEN_DIAGNOSTICS]   = HANDLER(diagnostics),    /* NEW */
    [SCREEN_INJECT_FAULT]  = HANDLER(inject_fault),   /* NEW */
};

/* ── Module state ────────────────────────────────────────────────────── */

/* Justification: HMI state must persist across task loop iterations.
 * Only the HMI task writes (except s_requested_screen which is atomic).
 * Static file scope. */
static bool           s_initialized       = false;
static bool           s_is_informer       = false;
static screen_id_t    s_current_screen    = SCREEN_BOOT;
static screen_id_t    s_nav_stack[NAV_STACK_MAX];
static uint8_t        s_nav_depth         = 0;
static hmi_mode_t     s_hmi_mode          = HMI_MODE_OPERATOR;
static bool           s_alarm_muted       = false;
static uint32_t       s_last_input_ms     = 0;
static uint32_t       s_last_lcd_update_ms = 0;
static uint32_t       s_last_led_tick_ms  = 0;

/* Combo detection state */
static uint32_t       s_engineer_combo_start_ms = 0;
static bool           s_engineer_combo_active   = false;
static bool           s_engineer_combo_fired    = false;
static uint32_t       s_mute_combo_start_ms     = 0;
static bool           s_mute_combo_active       = false;
static bool           s_mute_combo_fired        = false;

/* Atomic screen request — can be written from any task. */
static atomic_int     s_requested_screen  = SCREEN_COUNT; /* SCREEN_COUNT = none */

/* ── Forward declarations ────────────────────────────────────────────── */
static void     hmi_task(void *arg);
static uint32_t get_tick_ms(void);
static void     switch_screen(screen_id_t new_screen);
static void     push_screen(screen_id_t new_screen);
static void     pop_screen(void);
static bool     is_nav_screen(screen_id_t id);
static void     handle_default_nav(const button_event_t *evt);
static void     check_combos(void);
static void     process_pending_request(void);
static void     check_fault_forcing(void);
static void     check_boot_done(void);
static void     check_idle_timeout(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  get_tick_ms
 * ═══════════════════════════════════════════════════════════════════════ */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  is_nav_screen — Returns true for screens in the LEFT/RIGHT cycle.
 * ═══════════════════════════════════════════════════════════════════════ */
static bool is_nav_screen(screen_id_t id)
{
    return (id >= SCREEN_NAV_FIRST && id <= SCREEN_NAV_LAST);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  switch_screen — Direct transition (no stack).
 * ═══════════════════════════════════════════════════════════════════════ */
static void switch_screen(screen_id_t new_screen)
{
    if (new_screen >= SCREEN_COUNT) { return; }

    if (SCREEN_HANDLERS[s_current_screen].exit != NULL) {
        SCREEN_HANDLERS[s_current_screen].exit();
    }

    s_current_screen = new_screen;

    if (SCREEN_HANDLERS[s_current_screen].enter != NULL) {
        SCREEN_HANDLERS[s_current_screen].enter();
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  push_screen — Push current onto stack, switch to new.
 * ═══════════════════════════════════════════════════════════════════════ */
static void push_screen(screen_id_t new_screen)
{
    if (s_nav_depth < NAV_STACK_MAX) {
        s_nav_stack[s_nav_depth] = s_current_screen;
        s_nav_depth++;
    }
    switch_screen(new_screen);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  pop_screen — Return to previous screen from stack.
 * ═══════════════════════════════════════════════════════════════════════ */
static void pop_screen(void)
{
    if (s_nav_depth > 0) {
        s_nav_depth--;
        switch_screen(s_nav_stack[s_nav_depth]);
    } else {
        switch_screen(SCREEN_HOME);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  process_pending_request — Check atomic request from other tasks.
 * ═══════════════════════════════════════════════════════════════════════ */
static void process_pending_request(void)
{
    int req = atomic_exchange(&s_requested_screen, (int)SCREEN_COUNT);

    if (req >= 0 && req < (int)SCREEN_COUNT) {
        push_screen((screen_id_t)req);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  handle_default_nav — Default LEFT/RIGHT/OK behaviour.
 *
 *  WHY: Most screens don't handle navigation themselves.  This
 *  provides consistent LEFT=prev, RIGHT=next, OK=settings behaviour
 *  for the nav cycle, and LEFT=back for menu screens.
 * ═══════════════════════════════════════════════════════════════════════ */
static void handle_default_nav(const button_event_t *evt)
{
    if (evt->event != BTN_EVENT_PRESSED &&
        evt->event != BTN_EVENT_REPEAT) {
        return;
    }

    if (is_nav_screen(s_current_screen)) {
        if (evt->button == BTN_LEFT) {
            screen_id_t prev = (s_current_screen == SCREEN_NAV_FIRST)
                               ? SCREEN_NAV_LAST
                               : (screen_id_t)(s_current_screen - 1);
            switch_screen(prev);
            drv_buzzer_play(BUZZER_NAV);
        } else if (evt->button == BTN_RIGHT) {
            screen_id_t next = (s_current_screen == SCREEN_NAV_LAST)
                               ? SCREEN_NAV_FIRST
                               : (screen_id_t)(s_current_screen + 1);
            switch_screen(next);
            drv_buzzer_play(BUZZER_NAV);
        } else if (evt->button == BTN_OK) {
            push_screen(SCREEN_SETTINGS);
            drv_buzzer_play(BUZZER_CONFIRM);
        }
    } else {
        /* Menu screen: LEFT = back. */
        if (evt->button == BTN_LEFT) {
            pop_screen();
            drv_buzzer_play(BUZZER_NAV);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  check_combos — Detect button combos (engineer mode, mute).
 *
 *  WHY combos: Prevent accidental activation.  Requiring two buttons
 *  held simultaneously for 3 seconds is unlikely to happen by accident.
 * ═══════════════════════════════════════════════════════════════════════ */
static void check_combos(void)
{
    uint32_t now = get_tick_ms();

    /* Engineer mode: UP + DOWN held for ENGINEER_MODE_HOLD_MS */
    bool eng_combo = drv_buttons_is_pressed(BTN_UP) &&
                     drv_buttons_is_pressed(BTN_DOWN);

    if (eng_combo && !s_engineer_combo_active) {
        s_engineer_combo_start_ms = now;
        s_engineer_combo_active   = true;
        s_engineer_combo_fired    = false;
    } else if (!eng_combo) {
        s_engineer_combo_active = false;
        s_engineer_combo_fired  = false;
    }

    if (s_engineer_combo_active && !s_engineer_combo_fired) {
        if ((now - s_engineer_combo_start_ms) >= ENGINEER_MODE_HOLD_MS) {
            s_hmi_mode = (s_hmi_mode == HMI_MODE_OPERATOR)
                         ? HMI_MODE_ENGINEER : HMI_MODE_OPERATOR;
            s_engineer_combo_fired = true;
            drv_buzzer_play(BUZZER_CONFIRM);
            ESP_LOGI(TAG, "Mode toggled to %s",
                     s_hmi_mode == HMI_MODE_ENGINEER ? "ENGINEER" : "OPERATOR");
        }
    }

    /* Mute: LEFT + RIGHT held for MUTE_COMBO_HOLD_MS */
    bool mute_combo = drv_buttons_is_pressed(BTN_LEFT) &&
                      drv_buttons_is_pressed(BTN_RIGHT);

    if (mute_combo && !s_mute_combo_active) {
        s_mute_combo_start_ms = now;
        s_mute_combo_active   = true;
        s_mute_combo_fired    = false;
    } else if (!mute_combo) {
        s_mute_combo_active = false;
        s_mute_combo_fired  = false;
    }

    if (s_mute_combo_active && !s_mute_combo_fired) {
        if ((now - s_mute_combo_start_ms) >= MUTE_COMBO_HOLD_MS) {
            s_alarm_muted = !s_alarm_muted;
            s_mute_combo_fired = true;
            if (s_alarm_muted) { drv_buzzer_stop(); }
            drv_buzzer_play(BUZZER_CONFIRM);
            ESP_LOGI(TAG, "Alarm %s", s_alarm_muted ? "MUTED" : "UNMUTED");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  check_fault_forcing
 * ═══════════════════════════════════════════════════════════════════════ */
static void check_fault_forcing(void)
{
    if (system_status_get_state() != SYS_STATE_FAULT) { return; }
    if (s_current_screen == SCREEN_FAULT) { return; }

    push_screen(SCREEN_FAULT);
    if (!s_alarm_muted) {
        drv_buzzer_play(BUZZER_ALARM);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  check_boot_done
 * ═══════════════════════════════════════════════════════════════════════ */
static void check_boot_done(void)
{
    if (s_current_screen != SCREEN_BOOT) { return; }
    if (!screen_boot_is_done()) { return; }

    switch_screen(SCREEN_HOME);
    ui_anim_load_runtime_chars();
    drv_buzzer_play(BUZZER_STARTUP);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  check_idle_timeout
 * ═══════════════════════════════════════════════════════════════════════ */
static void check_idle_timeout(void)
{
    if (s_current_screen == SCREEN_BOOT ||
        s_current_screen == SCREEN_FAULT) {
        return;
    }
    if (s_current_screen == SCREEN_HOME) { return; }

    uint32_t idle = get_tick_ms() - s_last_input_ms;
    if (idle >= HMI_IDLE_TIMEOUT_MS) {
        s_nav_depth = 0;  /* Clear stack. */
        switch_screen(SCREEN_HOME);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hmi_task — Main HMI loop.
 *
 *  Runs on Core 0 at TASK_PRIO_HMI.
 *  Period: BTN_SCAN_INTERVAL_MS (20 ms).
 * ═══════════════════════════════════════════════════════════════════════ */
static void hmi_task(void *arg)
{
    UNUSED(arg);

    TickType_t last_wake = xTaskGetTickCount();
    s_last_input_ms      = get_tick_ms();
    s_last_lcd_update_ms = get_tick_ms();
    s_last_led_tick_ms   = get_tick_ms();

    /* Start on boot screen. */
    if (SCREEN_HANDLERS[SCREEN_BOOT].enter != NULL) {
        SCREEN_HANDLERS[SCREEN_BOOT].enter();
    }

    for (;;) {
        uint32_t now = get_tick_ms();

        if (s_is_informer) {
            drv_buttons_scan();
        }

        drv_buzzer_tick();

        /* LED status tick at LED_UPDATE_INTERVAL_MS. */
        if ((now - s_last_led_tick_ms) >= LED_UPDATE_INTERVAL_MS) {
            s_last_led_tick_ms = now;
            led_status_tick();
        }

        if (!s_is_informer) {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BTN_SCAN_INTERVAL_MS));
            continue;
        }

        /* Process external screen request. */
        process_pending_request();

        /* Read and dispatch button events. */
        button_event_t evt;
        if (drv_buttons_get_event(&evt, 0) == ERR_OK) {
            s_last_input_ms = now;

            if (evt.event == BTN_EVENT_PRESSED) {
                drv_buzzer_play(BUZZER_CLICK);
            }

            bool consumed = false;
            if (SCREEN_HANDLERS[s_current_screen].handle_event != NULL) {
                consumed = SCREEN_HANDLERS[s_current_screen].handle_event(&evt);
            }

            if (!consumed) {
                handle_default_nav(&evt);
            }
        }

        check_combos();

        /* LCD update at LCD_UPDATE_INTERVAL_MS. */
        if ((now - s_last_lcd_update_ms) >= LCD_UPDATE_INTERVAL_MS) {
            s_last_lcd_update_ms = now;
            if (SCREEN_HANDLERS[s_current_screen].update != NULL) {
                SCREEN_HANDLERS[s_current_screen].update();
            }
        }

        check_fault_forcing();
        check_boot_done();
        check_idle_timeout();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BTN_SCAN_INTERVAL_MS));
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hmi_manager_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hmi_manager_init(void)
{
    s_is_informer = drv_lcd2004_is_detected();

    if (s_is_informer) {
        system_status_set_role(ROLE_INFORMER);
        ESP_LOGI(TAG, "LCD detected — INFORMER role");

        error_code_t rc = drv_buttons_init();
        if (rc != ERR_OK) {
            ESP_LOGE(TAG, "Button init failed: %d", rc);
        }
    } else {
        system_status_set_role(ROLE_SILENT);
        ESP_LOGI(TAG, "No LCD — SILENT role (LED-only status)");
    }

    drv_buzzer_init();
    drv_leds_init();
    led_status_init();

    s_current_screen = SCREEN_BOOT;
    s_nav_depth      = 0;
    atomic_store(&s_requested_screen, (int)SCREEN_COUNT);

    BaseType_t ok = xTaskCreatePinnedToCore(
        hmi_task,
        "hmi_task",
        TASK_STACK_HMI,
        NULL,
        TASK_PRIO_HMI,
        NULL,
        TASK_CORE_COMMS
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HMI task");
        return ERR_HW_INIT_FAILED;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "HMI manager initialised (role=%s)",
             s_is_informer ? "INFORMER" : "SILENT");

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hmi_manager_request_screen
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hmi_manager_request_screen(screen_id_t screen)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (screen >= SCREEN_COUNT) { return ERR_INVALID_ARG; }

    atomic_store(&s_requested_screen, (int)screen);
    return ERR_OK;
}

screen_id_t hmi_manager_get_current_screen(void)
{
    return s_current_screen;
}

bool hmi_manager_is_initialized(void)
{
    return s_initialized;
}

hmi_mode_t hmi_manager_get_mode(void)
{
    return s_hmi_mode;
}

bool hmi_manager_is_alarm_muted(void)
{
    return s_alarm_muted;
}