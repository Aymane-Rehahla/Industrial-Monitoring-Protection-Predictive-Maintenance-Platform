/**
 * @file    menu_engine.h
 * @brief   Generic scrollable menu renderer for 20x4 LCD.
 * @version 1.0.1
 * @date    2025-01-01
 * @safety  LOW — display only.
 *
 * CHANGELOG:
 *   1.0.1  2025-01-01  Added #include "app_config.h" for MENU_LABEL_MAX_LEN.
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef MENU_ENGINE_H
#define MENU_ENGINE_H

#include "system_types.h"
#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

#define MENU_MAX_ITEMS    16
#define MENU_VISIBLE_ROWS  3

typedef void (*menu_action_fn)(uint32_t item_index);

typedef struct {
    char         label[MENU_LABEL_MAX_LEN];
    screen_id_t  target_screen;
    menu_action_fn action;
} menu_item_t;

typedef struct {
    const char  *title;
    menu_item_t  items[MENU_MAX_ITEMS];
    uint32_t     item_count;
    uint32_t     cursor_index;
    uint32_t     scroll_offset;
} menu_state_t;

error_code_t menu_engine_init(menu_state_t *menu,
                              const char *title,
                              const menu_item_t *items,
                              uint32_t count);

error_code_t menu_engine_render(const menu_state_t *menu);

bool menu_engine_handle_event(menu_state_t *menu,
                              const button_event_t *event);

error_code_t menu_engine_reset(menu_state_t *menu);

#endif /* MENU_ENGINE_H */