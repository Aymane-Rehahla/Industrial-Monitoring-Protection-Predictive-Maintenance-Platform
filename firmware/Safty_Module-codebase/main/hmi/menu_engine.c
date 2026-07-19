// ═══ FILE: main/hmi/menu_engine.c ═══
/**
 * @file    menu_engine.c
 * @brief   Generic scrollable menu renderer for 20×4 LCD — real implementation.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — display only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/menu_engine.h"
#include "system_types.h"
#include "app_config.h"

#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "menu_engine";

/* ═══════════════════════════════════════════════════════════════════════
 *  menu_engine_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t menu_engine_init(menu_state_t *menu,
                              const char *title,
                              const menu_item_t *items,
                              uint32_t count)
{
    if (menu == NULL || title == NULL || items == NULL) {
        return ERR_NULL_POINTER;
    }
    if (count > MENU_MAX_ITEMS) {
        ESP_LOGE(TAG, "init: count %lu exceeds MENU_MAX_ITEMS",
                 (unsigned long)count);
        return ERR_INVALID_ARG;
    }

    menu->title         = title;
    menu->item_count    = count;
    menu->cursor_index  = 0;
    menu->scroll_offset = 0;

    /* Copy items into the menu's own storage. */
    for (uint32_t i = 0; i < count && i < MENU_MAX_ITEMS; i++) {
        menu->items[i] = items[i];
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  menu_engine_render
 *
 *  WHY explicit padding: Prevents ghost characters from previous screen
 *  content.  Every row is written to its full LCD_COLS width.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t menu_engine_render(const menu_state_t *menu)
{
    if (menu == NULL) { return ERR_NULL_POINTER; }

    /* Row 0: title. */
    drv_lcd2004_write_line(0, menu->title);

    /* Rows 1-3: visible menu items. */
    for (uint8_t row = 0; row < MENU_VISIBLE_ROWS; row++) {
        uint32_t idx = menu->scroll_offset + row;
        char line[LCD_COLS + 1];

        if (idx < menu->item_count) {
            char cursor = (idx == menu->cursor_index) ? '>' : ' ';
            snprintf(line, sizeof(line), "%c%-19s",
                     cursor, menu->items[idx].label);
        } else {
            memset(line, ' ', LCD_COLS);
            line[LCD_COLS] = '\0';
        }

        /* row+1 because row 0 is the title. */
        drv_lcd2004_write_line((uint8_t)(row + 1), line);
    }

    return ERR_OK;
}

/**
 * @brief  Clamp scroll_offset so the cursor is always visible.
 *
 * WHY: The cursor can move beyond the visible window when the user
 * presses UP at the top or DOWN at the bottom.  This keeps the
 * viewport tracking the cursor.
 */
static void clamp_scroll(menu_state_t *menu)
{
    if (menu->item_count == 0) { return; }

    if (menu->cursor_index < menu->scroll_offset) {
        menu->scroll_offset = menu->cursor_index;
    }

    if (menu->cursor_index >= menu->scroll_offset + MENU_VISIBLE_ROWS) {
        menu->scroll_offset = menu->cursor_index - MENU_VISIBLE_ROWS + 1;
    }

    /* Upper bound for scroll_offset. */
    uint32_t max_offset = 0;
    if (menu->item_count > MENU_VISIBLE_ROWS) {
        max_offset = menu->item_count - MENU_VISIBLE_ROWS;
    }
    if (menu->scroll_offset > max_offset) {
        menu->scroll_offset = max_offset;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  menu_engine_handle_event
 * ═══════════════════════════════════════════════════════════════════════ */
bool menu_engine_handle_event(menu_state_t *menu,
                              const button_event_t *event)
{
    if (menu == NULL || event == NULL) { return false; }
    if (menu->item_count == 0) { return false; }

    /* Only act on PRESSED and REPEAT events. */
    if (event->event != BTN_EVENT_PRESSED &&
        event->event != BTN_EVENT_REPEAT) {
        return false;
    }

    if (event->button == BTN_UP) {
        if (menu->cursor_index > 0) {
            menu->cursor_index--;
        }
        clamp_scroll(menu);
        return true;
    }

    if (event->button == BTN_DOWN) {
        if (menu->cursor_index < menu->item_count - 1) {
            menu->cursor_index++;
        }
        clamp_scroll(menu);
        return true;
    }

    if (event->button == BTN_OK) {
        const menu_item_t *item = &menu->items[menu->cursor_index];

        if (item->target_screen < SCREEN_COUNT) {
            hmi_manager_request_screen(item->target_screen);
        } else if (item->action != NULL) {
            item->action(menu->cursor_index);
        }
        drv_buzzer_play(BUZZER_CONFIRM);
        return true;
    }

    return false;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  menu_engine_reset
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t menu_engine_reset(menu_state_t *menu)
{
    if (menu == NULL) { return ERR_NULL_POINTER; }

    menu->cursor_index  = 0;
    menu->scroll_offset = 0;

    return ERR_OK;
}