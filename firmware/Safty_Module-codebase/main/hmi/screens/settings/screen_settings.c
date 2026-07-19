/**
 * @file    screen_settings.c
 * @brief   Main settings menu — scrollable via menu_engine.
 * @version 2.0.0
 * @date    2025-01-01
 * @safety  LOW — menu navigation only.
 *
 * CHANGELOG:
 *   2.0.0  2025-01-01  Added Diagnostics, reordered menu items.
 *   1.1.0  2025-01-01  Fixed: stubs were overriding this file.
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "hmi/menu_engine.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "esp_log.h"
#include "system_types.h"

static const char *TAG = "SCR_SETTINGS";
static menu_state_t s_menu;

static const menu_item_t s_local_items[] = {
    { "Diagnostics",   SCREEN_DIAGNOSTICS,   NULL },
    { "Thresholds",    SCREEN_THRESHOLD,      NULL },
    { "View Sensors",  SCREEN_SENSOR_VIEW,    NULL },
    { "Add Sensor",    SCREEN_SENSOR_ADD,     NULL },
    { "Remove Sensor", SCREEN_SENSOR_REMOVE,  NULL },
    { "Sensor Test",   SCREEN_SENSOR_TEST,    NULL },
    { "Calibration",   SCREEN_CAL_SELECT,     NULL },
    { "Pairing",       SCREEN_PAIRING,        NULL },
    { "System Info",   SCREEN_SYSTEM,         NULL },
};

#define LOCAL_ITEM_COUNT  (sizeof(s_local_items) / sizeof(s_local_items[0]))

void screen_settings_enter(void)
{
    ESP_LOGI(TAG, "enter");
    drv_lcd2004_clear();
    menu_engine_init(&s_menu, "SETTINGS",
                     s_local_items, LOCAL_ITEM_COUNT);
    menu_engine_render(&s_menu);
}

void screen_settings_update(void)
{
    menu_engine_render(&s_menu);
}

bool screen_settings_handle_event(const button_event_t *e)
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

void screen_settings_exit(void)
{
    ESP_LOGI(TAG, "exit");
}