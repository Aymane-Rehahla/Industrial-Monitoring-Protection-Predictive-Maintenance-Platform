/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_settings.c - Settings Screen                                         ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"
#include "nvs_config.h"
#include "drv_buzzer.h"

extern protection_config_t* hmi_get_config(void);

static uint8_t s_selected_item = 0;
static bool s_editing = false;

#define SETTINGS_ITEMS  4

void screen_settings_enter(void)
{
    s_selected_item = 0;
    s_editing = false;
}

void screen_settings_exit(void)
{
    /* Save if we were editing */
    if (s_editing) {
        protection_config_t *cfg = hmi_get_config();
        if (cfg != NULL) {
            nvs_config_save(cfg);
            drv_buzzer_play(BEEP_CONFIRM);
        }
        s_editing = false;
    }
}

void screen_settings_render(bool full_redraw)
{
    protection_config_t *cfg = hmi_get_config();
    
    if (full_redraw) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== SETTINGS ========");
    }
    
    if (cfg == NULL) {
        drv_lcd_print_at(0, 1, " Config unavailable ");
        return;
    }
    
    /* Show 3 items starting from selected */
    const char *items[SETTINGS_ITEMS] = {
        "Max Current",
        "Max Voltage", 
        "Min Voltage",
        "Max Temp"
    };
    float *values[SETTINGS_ITEMS] = {
        &cfg->overcurrent_limit_A,
        &cfg->overvoltage_limit_V,
        &cfg->undervoltage_limit_V,
        &cfg->overtemp_limit_C
    };
    const char *units[SETTINGS_ITEMS] = {"A", "V", "V", "C"};
    
    for (int row = 0; row < 3; row++) {
        int item = (s_selected_item + row) % SETTINGS_ITEMS;
        char marker = (row == 0) ? '>' : ' ';
        char edit = (row == 0 && s_editing) ? '*' : ' ';
        
        drv_lcd_printf_at(0, row + 1, "%c%-11s%6.1f%s%c",
                          marker, items[item], *values[item], units[item], edit);
    }
}

screen_id_t screen_settings_input(button_id_t button)
{
    protection_config_t *cfg = hmi_get_config();
    if (cfg == NULL) {
        return SCREEN_HOME;
    }
    
    float *values[SETTINGS_ITEMS] = {
        &cfg->overcurrent_limit_A,
        &cfg->overvoltage_limit_V,
        &cfg->undervoltage_limit_V,
        &cfg->overtemp_limit_C
    };
    float steps[SETTINGS_ITEMS] = {1.0f, 5.0f, 5.0f, 5.0f};
    
    if (s_editing) {
        /* In edit mode */
        switch (button) {
            case BTN_UP:
                *values[s_selected_item] += steps[s_selected_item];
                hmi_manager_request_redraw();
                break;
            case BTN_DOWN:
                *values[s_selected_item] -= steps[s_selected_item];
                hmi_manager_request_redraw();
                break;
            case BTN_OK:
                s_editing = false;
                nvs_config_save(cfg);
                drv_buzzer_play(BEEP_CONFIRM);
                hmi_manager_request_redraw();
                break;
            default:
                break;
        }
    } else {
        /* Navigation mode */
        switch (button) {
            case BTN_UP:
                s_selected_item = (s_selected_item + SETTINGS_ITEMS - 1) % SETTINGS_ITEMS;
                hmi_manager_request_redraw();
                break;
            case BTN_DOWN:
                s_selected_item = (s_selected_item + 1) % SETTINGS_ITEMS;
                hmi_manager_request_redraw();
                break;
            case BTN_OK:
                s_editing = true;
                hmi_manager_request_redraw();
                break;
            case BTN_RIGHT:
                return SCREEN_SYSTEM_INFO;
            case BTN_LEFT:
                return SCREEN_FAULT_LOG;
            default:
                break;
        }
    }
    
    return SCREEN_SETTINGS;
}