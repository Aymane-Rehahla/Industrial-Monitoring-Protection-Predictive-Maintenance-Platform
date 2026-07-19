/**
 * @file    screen_inject_fault.c
 * @brief   Test fault injection screen — pick a fault type, press OK to inject.
 *          System transitions to FAULT state and shows fault detail screen.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — demo/test only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "hmi/menu_engine.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/protection/fault_handler.h"
#include "core/system_status.h"
#include "esp_log.h"
#include "system_types.h"
#include <string.h>

static const char *TAG = "SCR_INJECT";
static menu_state_t s_menu;

/*
 * Each test scenario: what the menu shows, and what gets injected.
 * Values are realistic for a 3-phase industrial system demo.
 */
typedef struct {
    const char  *label;         /* Menu display text (max 19 chars)     */
    fault_code_t code;
    severity_t   severity;
    float        measured;
    float        threshold;
    bool         forgivable;
} inject_scenario_t;

static const inject_scenario_t s_scenarios[] = {
    { "OverVoltage  280V", FAULT_OVERVOLTAGE,    SEVERITY_CRITICAL,      280.0f, 240.0f, false },
    { "UnderVolt    180V", FAULT_UNDERVOLTAGE,   SEVERITY_WARNING,       180.0f, 200.0f, true  },
    { "OverCurrent   15A", FAULT_OVERCURRENT,    SEVERITY_CRITICAL,       15.0f,  10.0f, false },
    { "OverTemp      95C", FAULT_OVERTEMP,       SEVERITY_WARNING,        95.0f,  80.0f, true  },
    { "Gas Leak   900ppm", FAULT_GAS_LEVEL,      SEVERITY_CATASTROPHIC,  900.0f, 500.0f, false },
    { "Vibration   5.2g",  FAULT_VIBRATION,      SEVERITY_WARNING,         5.2f,   2.5f, true  },
    { "RPM High    3800",  FAULT_RPM_HIGH,       SEVERITY_CRITICAL,     3800.0f,3500.0f, false },
    { "Peer Lost",         FAULT_PEER_HEARTBEAT, SEVERITY_CRITICAL,        0.0f,   0.0f, false },
};

#define SCENARIO_COUNT  (sizeof(s_scenarios) / sizeof(s_scenarios[0]))

/*
 * Action callback — called by menu_engine when OK is pressed.
 * Injects the selected fault and triggers system FAULT state.
 */
static void action_inject(uint32_t item_index)
{
    if (item_index >= SCENARIO_COUNT) { return; }

    const inject_scenario_t *s = &s_scenarios[item_index];

    ESP_LOGE(TAG, "INJECTING: %s (code=%d sev=%d val=%.1f thresh=%.1f)",
             s->label, s->code, s->severity, s->measured, s->threshold);

    fault_handler_inject(s->code,
                         s->severity,
                         s->measured,
                         s->threshold,
                         s->forgivable);

    system_status_set_state(SYS_STATE_FAULT);
    drv_buzzer_play(BUZZER_ALARM);
}

/*
 * Build menu items from scenarios.
 * Each item has action = action_inject, no target_screen.
 */
static menu_item_t s_menu_items[SCENARIO_COUNT];

static void build_menu_items(void)
{
    for (uint32_t i = 0; i < SCENARIO_COUNT; i++) {
        /* Copy label safely. */
        size_t len = strlen(s_scenarios[i].label);
        if (len >= MENU_LABEL_MAX_LEN) {
            len = MENU_LABEL_MAX_LEN - 1;
        }
        memcpy(s_menu_items[i].label, s_scenarios[i].label, len);
        s_menu_items[i].label[len] = '\0';

        s_menu_items[i].target_screen = SCREEN_COUNT; /* No auto-navigate */
        s_menu_items[i].action        = action_inject;
    }
}

void screen_inject_fault_enter(void)
{
    ESP_LOGI(TAG, "enter — %d scenarios", (int)SCENARIO_COUNT);
    drv_lcd2004_clear();
    build_menu_items();
    menu_engine_init(&s_menu, "INJECT TEST FAULT",
                     s_menu_items, SCENARIO_COUNT);
    menu_engine_render(&s_menu);
}

void screen_inject_fault_update(void)
{
    menu_engine_render(&s_menu);
}

bool screen_inject_fault_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (e->event == BTN_EVENT_PRESSED && e->button == BTN_LEFT) {
        return false;
    }

    bool handled = menu_engine_handle_event(&s_menu, e);

    if (handled) {
        menu_engine_render(&s_menu);
    }

    return handled;
}

void screen_inject_fault_exit(void)
{
    ESP_LOGI(TAG, "exit");
}