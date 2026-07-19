// ═══ FILE: main/hmi/menus/menu_main.h ═══
/**
 * @file    menu_main.h
 * @brief   Main settings menu item definitions.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — menu data only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef MENU_MAIN_H
#define MENU_MAIN_H

#include "hmi/menu_engine.h"
#include <stdint.h>

/** Menu items for the main settings screen. */
extern const menu_item_t g_settings_menu_items[];

/** Number of items in g_settings_menu_items[]. */
extern const uint32_t g_settings_menu_count;

#endif /* MENU_MAIN_H */