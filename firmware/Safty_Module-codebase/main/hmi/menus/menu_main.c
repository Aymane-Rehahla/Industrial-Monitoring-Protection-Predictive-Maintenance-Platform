// ═══ FILE: main/hmi/menus/menu_main.c ═══
/**
 * @file    menu_main.c
 * @brief   Main settings menu item table.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — menu data only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/menus/menu_main.h"
#include "system_types.h"

#include "esp_log.h"

const menu_item_t g_settings_menu_items[] = {
    { "Thresholds",    SCREEN_THRESHOLD,     NULL },
    { "View Sensors",  SCREEN_SENSOR_VIEW,   NULL },
    { "Add Sensor",    SCREEN_SENSOR_ADD,    NULL },
    { "Remove Sensor", SCREEN_SENSOR_REMOVE, NULL },
    { "Test Sensor",   SCREEN_SENSOR_TEST,   NULL },
    { "Calibration",   SCREEN_CAL_SELECT,    NULL },
    { "Pairing",       SCREEN_PAIRING,       NULL },
};

const uint32_t g_settings_menu_count = ARRAY_SIZE(g_settings_menu_items);