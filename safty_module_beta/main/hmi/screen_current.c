/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_current.c - Current Detail Screen                                    ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"
#include "default_config.h"

extern const sensor_set_t* hmi_get_sensors(void);

void screen_current_render(bool full_redraw)
{
    const sensor_set_t *s = hmi_get_sensors();
    
    if (full_redraw) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== CURRENT (A) =====");
    }
    
    float max_a = DEFAULT_OVERCURRENT_LIMIT_A;
    
    if (s != NULL) {
        /* Convert mA to A for display */
        float i1 = s->current_L1.scaled_value / 1000.0f;
        float i2 = s->current_L2.scaled_value / 1000.0f;
        float i3 = s->current_L3.scaled_value / 1000.0f;
        
        drv_lcd_printf_at(0, 1, "L1:%5.1f A", i1);
        drv_lcd_printf_at(0, 2, "L2:%5.1f A", i2);
        drv_lcd_printf_at(0, 3, "L3:%5.1f A", i3);
    } else {
        drv_lcd_print_at(0, 1, "L1: ---.-- A        ");
        drv_lcd_print_at(0, 2, "L2: ---.-- A        ");
        drv_lcd_print_at(0, 3, "L3: ---.-- A        ");
    }
}

screen_id_t screen_current_input(button_id_t button)
{
    switch (button) {
        case BTN_RIGHT: return SCREEN_TEMPERATURE;
        case BTN_LEFT:  return SCREEN_VOLTAGE;
        default:        return SCREEN_CURRENT;
    }
}