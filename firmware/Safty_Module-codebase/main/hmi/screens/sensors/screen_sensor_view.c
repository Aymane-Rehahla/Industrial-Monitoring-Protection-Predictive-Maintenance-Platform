// ═══ FILE: main/hmi/screens/sensors/screen_sensor_view.c ═══
/**
 * @file    screen_sensor_view.c
 * @brief   Display list of all registered sensors with online status.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — display only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "hmi/menu_engine.h"
#include "core/sensor_manager/sensor_manager.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_sensor_view";

/* Justification: Menu state and item buffer must persist across calls.
 * File scope, HMI task only. */
static menu_state_t s_menu;
static menu_item_t  s_items[MENU_MAX_ITEMS];

/** Build the sensor list menu from the registry. */
static void build_sensor_list(void)
{
    uint32_t count = 0;
    sensor_manager_get_sensor_count(&count);

    if (count > MENU_MAX_ITEMS) { count = MENU_MAX_ITEMS; }

    for (uint32_t i = 0; i < count; i++) {
        sensor_entry_t entry;
        if (sensor_manager_get_sensor(i, &entry) != ERR_OK) { break; }



snprintf(s_items[i].label,
         MENU_LABEL_MAX_LEN,
         "%-8.8s %4s",
         entry.name,
         entry.is_online ? "[ON]" : "[--]");


        s_items[i].target_screen = SCREEN_COUNT;  /* View only. */
        s_items[i].action        = NULL;
    }

    menu_engine_init(&s_menu, "SENSORS", s_items, count);
}

void screen_sensor_view_enter(void)
{
    ESP_LOGI(TAG, "enter");
    drv_lcd2004_clear();
    build_sensor_list();
}

void screen_sensor_view_update(void)
{
    menu_engine_render(&s_menu);
}

bool screen_sensor_view_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (e->event == BTN_EVENT_PRESSED && e->button == BTN_LEFT) {
        return false;  /* Pop back. */
    }

    return menu_engine_handle_event(&s_menu, e);
}

void screen_sensor_view_exit(void)
{
    /* Nothing to clean up. */
}