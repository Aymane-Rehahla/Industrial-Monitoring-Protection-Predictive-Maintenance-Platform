/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_voltage.c - Voltage Detail Screen                                    ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"
#include "default_config.h"

extern const sensor_set_t* hmi_get_sensors(void);

/* ═══════════════════════════════════════════════════════════════════════════════ */

void screen_voltage_render(bool full_redraw)
{
    const sensor_set_t *s = hmi_get_sensors();
    
    if (full_redraw) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== VOLTAGE (V) =====");
    }
    
    float max_v = DEFAULT_OVERVOLTAGE_LIMIT_V;
    
    if (s != NULL) {
        /* L1 */
        int bar1 = (int)((s->voltage_L1.scaled_value / max_v) * 10);
        if (bar1 > 10) bar1 = 10;
        if (bar1 < 0) bar1 = 0;
        drv_lcd_printf_at(0, 1, "L1:%5.1f [%-10.*s]", 
                          s->voltage_L1.scaled_value, bar1, "==========");
        
        /* L2 */
        int bar2 = (int)((s->voltage_L2.scaled_value / max_v) * 10);
        if (bar2 > 10) bar2 = 10;
        if (bar2 < 0) bar2 = 0;
        drv_lcd_printf_at(0, 2, "L2:%5.1f [%-10.*s]", 
                          s->voltage_L2.scaled_value, bar2, "==========");
        
        /* L3 */
        int bar3 = (int)((s->voltage_L3.scaled_value / max_v) * 10);
        if (bar3 > 10) bar3 = 10;
        if (bar3 < 0) bar3 = 0;
        drv_lcd_printf_at(0, 3, "L3:%5.1f [%-10.*s]", 
                          s->voltage_L3.scaled_value, bar3, "==========");
    } else {
        drv_lcd_print_at(0, 1, "L1: ---.-- [          ]");
        drv_lcd_print_at(0, 2, "L2: ---.-- [          ]");
        drv_lcd_print_at(0, 3, "L3: ---.-- [          ]");
    }
}

screen_id_t screen_voltage_input(button_id_t button)
{
    switch (button) {
        case BTN_RIGHT: return SCREEN_CURRENT;
        case BTN_LEFT:  return SCREEN_HOME;
        default:        return SCREEN_VOLTAGE;
    }
}