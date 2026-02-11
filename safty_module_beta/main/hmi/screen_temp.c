/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_temp.c - Temperature Screen                                          ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"

extern const sensor_set_t* hmi_get_sensors(void);

void screen_temp_render(bool full_redraw)
{
    const sensor_set_t *s = hmi_get_sensors();
    
    if (full_redraw) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== TEMPERATURE =====");
    }
    
    if (s != NULL) {
        drv_lcd_printf_at(0, 1, "Machine: %5.1f C    ", s->temp_machine.scaled_value);
        drv_lcd_printf_at(0, 2, "Ambient: %5.1f C    ", s->temp_ambient.scaled_value);
        drv_lcd_printf_at(0, 3, "Humidity:%5.1f %%   ", s->humidity.scaled_value);
    } else {
        drv_lcd_print_at(0, 1, "Machine: ---.-- C   ");
        drv_lcd_print_at(0, 2, "Ambient: ---.-- C   ");
        drv_lcd_print_at(0, 3, "Humidity: ---.-- %  ");
    }
}

screen_id_t screen_temp_input(button_id_t button)
{
    switch (button) {
        case BTN_RIGHT: return SCREEN_GAS;
        case BTN_LEFT:  return SCREEN_CURRENT;
        default:        return SCREEN_TEMPERATURE;
    }
}