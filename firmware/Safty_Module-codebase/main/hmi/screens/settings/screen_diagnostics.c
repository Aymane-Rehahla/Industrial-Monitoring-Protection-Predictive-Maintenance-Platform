/**
 * @file    screen_diagnostics.c
 * @brief   Diagnostics sub-menu — Fault History, Inject Fault, System Health.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — menu navigation only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "hmi/menu_engine.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "esp_log.h"
#include "system_types.h"

static const char *TAG = "SCR_DIAG";
static menu_state_t s_menu;

static const menu_item_t s_diag_items[] = {
    { "Fault History",  SCREEN_FAULT,         NULL },
    { "Inject Fault",   SCREEN_INJECT_FAULT,  NULL },
    { "System Health",  SCREEN_SYSTEM,        NULL },
};

#define DIAG_ITEM_COUNT  (sizeof(s_diag_items) / sizeof(s_diag_items[0]))

void screen_diagnostics_enter(void)
{
    ESP_LOGI(TAG, "enter");
    drv_lcd2004_clear();
    menu_engine_init(&s_menu, "DIAGNOSTICS",
                     s_diag_items, DIAG_ITEM_COUNT);
    menu_engine_render(&s_menu);
}

void screen_diagnostics_update(void)
{
    menu_engine_render(&s_menu);
}

bool screen_diagnostics_handle_event(const button_event_t *e)
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

void screen_diagnostics_exit(void)
{
    ESP_LOGI(TAG, "exit");
}