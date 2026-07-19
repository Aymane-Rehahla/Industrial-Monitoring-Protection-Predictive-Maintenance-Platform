// ═══ FILE: main/hmi/screens/sensors/screen_sensor_remove.c ═══
/**
 * @file    screen_sensor_remove.c
 * @brief   Remove a user-added (removable) sensor.
 *          Step 1: Select.  Step 2: Confirm.  Step 3: Done.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — sensor registry change, restart required.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "hmi/menu_engine.h"
#include "core/sensor_manager/sensor_manager.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_sensor_remove";

typedef enum {
    REM_STEP_SELECT  = 0,
    REM_STEP_CONFIRM = 1,
    REM_STEP_DONE    = 2
} remove_step_t;

/* Justification: Wizard state persists across calls.  File scope. */
static remove_step_t s_step;
static menu_state_t  s_menu;
static menu_item_t   s_items[MENU_MAX_ITEMS];
static uint32_t      s_registry_indices[MENU_MAX_ITEMS];
static uint32_t      s_selected_index;
static char          s_selected_name[SENSOR_NAME_MAX_LEN];

static void on_sensor_selected(uint32_t menu_index);

/** Build menu showing only removable sensors. */
static void build_removable_list(void)
{
    uint32_t total = 0;
    sensor_manager_get_sensor_count(&total);

    uint32_t menu_count = 0;

    for (uint32_t i = 0; i < total && menu_count < MENU_MAX_ITEMS; i++) {
        sensor_entry_t entry;
        if (sensor_manager_get_sensor(i, &entry) != ERR_OK) { continue; }
        if (!entry.is_removable) { continue; }

        snprintf(s_items[menu_count].label, MENU_LABEL_MAX_LEN,
                 "%s", entry.name);
        s_items[menu_count].target_screen = SCREEN_COUNT;
        s_items[menu_count].action        = on_sensor_selected;
        s_registry_indices[menu_count]    = i;
        menu_count++;
    }

    menu_engine_init(&s_menu, "REMOVE SENSOR", s_items, menu_count);
}

static void on_sensor_selected(uint32_t menu_index)
{
    if (menu_index >= MENU_MAX_ITEMS) { return; }

    s_selected_index = s_registry_indices[menu_index];

    sensor_entry_t entry;
    if (sensor_manager_get_sensor(s_selected_index, &entry) == ERR_OK) {
        strncpy(s_selected_name, entry.name, SENSOR_NAME_MAX_LEN - 1);
        s_selected_name[SENSOR_NAME_MAX_LEN - 1] = '\0';
    }

    s_step = REM_STEP_CONFIRM;
}

void screen_sensor_remove_enter(void)
{
    ESP_LOGI(TAG, "enter");
    s_step = REM_STEP_SELECT;
    drv_lcd2004_clear();
    build_removable_list();
}

void screen_sensor_remove_update(void)
{
    char buf[LCD_COLS + 1];

    switch (s_step) {
        case REM_STEP_SELECT:
            menu_engine_render(&s_menu);
            break;

        case REM_STEP_CONFIRM:
            drv_lcd2004_write_line(0, "  REMOVE SENSOR?  ");
            snprintf(buf, sizeof(buf), " %-18s", s_selected_name);
            drv_lcd2004_write_line(1, buf);
            drv_lcd2004_write_line(2, "");
            drv_lcd2004_write_line(3, "OK=Remove L=Cancel");
            break;

        case REM_STEP_DONE:
            drv_lcd2004_write_line(0, "  SENSOR REMOVED  ");
            snprintf(buf, sizeof(buf), " %-18s", s_selected_name);
            drv_lcd2004_write_line(1, buf);
            drv_lcd2004_write_line(2, " Restart to apply ");
            drv_lcd2004_write_line(3, "  OK to continue  ");
            break;
    }
}

bool screen_sensor_remove_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED && e->event != BTN_EVENT_REPEAT) {
        return false;
    }

    switch (s_step) {
        case REM_STEP_SELECT:
            if (e->button == BTN_LEFT) { return false; }
            return menu_engine_handle_event(&s_menu, e);

        case REM_STEP_CONFIRM:
            if (e->button == BTN_OK) {
                sensor_manager_remove_sensor(s_selected_index);
                drv_buzzer_play(BUZZER_CONFIRM);
                s_step = REM_STEP_DONE;
                return true;
            }
            if (e->button == BTN_LEFT) {
                s_step = REM_STEP_SELECT;
                build_removable_list();
                return true;
            }
            return true;

        case REM_STEP_DONE:
            return false;  /* Any press pops back. */
    }

    return false;
}

void screen_sensor_remove_exit(void)
{
    /* Nothing to clean up. */
}