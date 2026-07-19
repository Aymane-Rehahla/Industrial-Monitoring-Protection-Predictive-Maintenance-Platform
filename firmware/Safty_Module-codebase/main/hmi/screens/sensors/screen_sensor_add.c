// ═══ FILE: main/hmi/screens/sensors/screen_sensor_add.c ═══
/**
 * @file    screen_sensor_add.c
 * @brief   Multi-step wizard to add a removable sensor.
 *          Step 1: Select type.  Step 2: Select pin.  Step 3: Done.
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
#include "hmi/menus/menu_settings.h"
#include "core/sensor_manager/sensor_manager.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_sensor_add";

typedef enum {
    ADD_STEP_TYPE = 0,
    ADD_STEP_PIN  = 1,
    ADD_STEP_DONE = 2
} add_step_t;

/* Justification: Wizard state must persist across calls.
 * File scope, HMI task only. */
static add_step_t     s_step;
static menu_state_t   s_menu;
static menu_item_t    s_items[MENU_MAX_ITEMS];
static sensor_type_t  s_selected_type;
static uint8_t        s_selected_pin;
static pin_suggestion_t s_pins[MENU_MAX_ITEMS];
static uint32_t       s_pin_count;

/* Forward declarations for action callbacks. */
static void on_type_selected(uint32_t index);
static void on_pin_selected(uint32_t index);

/** Build the sensor type selection menu. */
static void build_type_menu(void)
{
    for (uint32_t i = 0; i < g_removable_type_count && i < MENU_MAX_ITEMS; i++) {
        const char *name = sensor_manager_get_type_name(
            g_removable_sensor_types[i]);
        snprintf(s_items[i].label, MENU_LABEL_MAX_LEN, "%s", name);
        s_items[i].target_screen = SCREEN_COUNT;
        s_items[i].action        = on_type_selected;
    }
    menu_engine_init(&s_menu, "ADD: Select Type",
                     s_items, g_removable_type_count);
}

/** Build the pin selection menu for the chosen sensor type. */
static void build_pin_menu(void)
{
    s_pin_count = 0;
    sensor_manager_get_available_pins(s_selected_type, s_pins,
                                     MENU_MAX_ITEMS, &s_pin_count);

    for (uint32_t i = 0; i < s_pin_count && i < MENU_MAX_ITEMS; i++) {
        snprintf(s_items[i].label, MENU_LABEL_MAX_LEN, "GPIO %u%s",
                 s_pins[i].gpio_pin,
                 s_pins[i].is_adc_capable ? " (ADC)" : "");
        s_items[i].target_screen = SCREEN_COUNT;
        s_items[i].action        = on_pin_selected;
    }
    menu_engine_init(&s_menu, "ADD: Select Pin",
                     s_items, s_pin_count);
}

static void on_type_selected(uint32_t index)
{
    if (index >= g_removable_type_count) { return; }

    s_selected_type = g_removable_sensor_types[index];
    build_pin_menu();
    s_step = ADD_STEP_PIN;
}

static void on_pin_selected(uint32_t index)
{
    if (index >= s_pin_count) { return; }

    s_selected_pin = s_pins[index].gpio_pin;

    sensor_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.type        = s_selected_type;
    entry.gpio_pin    = s_selected_pin;
    entry.i2c_bus     = 0xFF;
    entry.i2c_addr    = 0x00;
    entry.is_enabled  = true;
    entry.is_removable = true;
    entry.is_online   = false;

    snprintf(entry.name, SENSOR_NAME_MAX_LEN, "%s",
             sensor_manager_get_type_name(s_selected_type));

    sensor_manager_add_sensor(&entry);
    s_step = ADD_STEP_DONE;
    drv_buzzer_play(BUZZER_CONFIRM);
}

void screen_sensor_add_enter(void)
{
    ESP_LOGI(TAG, "enter");
    s_step = ADD_STEP_TYPE;
    drv_lcd2004_clear();
    build_type_menu();
}

void screen_sensor_add_update(void)
{
    if (s_step == ADD_STEP_DONE) {
        char buf[LCD_COLS + 1];
        drv_lcd2004_write_line(0, "   SENSOR ADDED   ");

        snprintf(buf, sizeof(buf), "Type: %-13s",
                 sensor_manager_get_type_name(s_selected_type));
        drv_lcd2004_write_line(1, buf);

        snprintf(buf, sizeof(buf), "Pin:  GPIO %-7u", s_selected_pin);
        drv_lcd2004_write_line(2, buf);

        drv_lcd2004_write_line(3, "  OK to continue  ");
        return;
    }

    menu_engine_render(&s_menu);
}

bool screen_sensor_add_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (s_step == ADD_STEP_DONE) {
        /* Any press pops back. */
        if (e->event == BTN_EVENT_PRESSED) { return false; }
        return true;
    }

    if (e->event == BTN_EVENT_PRESSED && e->button == BTN_LEFT) {
        if (s_step == ADD_STEP_PIN) {
            s_step = ADD_STEP_TYPE;
            build_type_menu();
            return true;
        }
        return false;  /* Pop screen. */
    }

    return menu_engine_handle_event(&s_menu, e);
}

void screen_sensor_add_exit(void)
{
    /* Nothing to clean up. */
}