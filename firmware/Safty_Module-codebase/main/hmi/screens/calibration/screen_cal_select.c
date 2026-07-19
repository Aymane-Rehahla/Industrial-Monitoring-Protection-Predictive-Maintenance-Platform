// ═══ FILE: main/hmi/screens/calibration/screen_cal_select.c ═══
/**
 * @file    screen_cal_select.c
 * @brief   Calibration method selection — Auto or Manual.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — menu navigation only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "hmi/menu_engine.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_cal_select";

static menu_state_t s_menu;

static const menu_item_t CAL_ITEMS[] = {
    { "Auto Calibrate",   SCREEN_CAL_AUTO,   NULL },
    { "Manual Calibrate", SCREEN_CAL_MANUAL,  NULL },
};

void screen_cal_select_enter(void)
{
    ESP_LOGI(TAG, "enter");
    drv_lcd2004_clear();
    menu_engine_init(&s_menu, "CALIBRATION",
                     CAL_ITEMS, ARRAY_SIZE(CAL_ITEMS));
}

void screen_cal_select_update(void)
{
    menu_engine_render(&s_menu);
}

bool screen_cal_select_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (e->event == BTN_EVENT_PRESSED && e->button == BTN_LEFT) {
        return false;
    }

    return menu_engine_handle_event(&s_menu, e);
}

void screen_cal_select_exit(void)
{
    /* Nothing to clean up. */
}