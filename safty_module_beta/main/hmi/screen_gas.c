/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_gas.c - Gas Sensors Screen                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"

extern const sensor_set_t* hmi_get_sensors(void);

void screen_gas_render(bool full_redraw)
{
    const sensor_set_t *s = hmi_get_sensors();
    
    if (full_redraw) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== GAS SENSORS =====");
    }
    
    if (s != NULL) {
        drv_lcd_printf_at(0, 1, "MQ2 (Smoke): %4d   ", (int)s->gas_mq2.raw_value);
        drv_lcd_printf_at(0, 2, "MQ4 (CH4):   %4d   ", (int)s->gas_mq4.raw_value);
        drv_lcd_printf_at(0, 3, "MQ9 (CO):    %4d   ", (int)s->gas_mq9.raw_value);
    } else {
        drv_lcd_print_at(0, 1, "MQ2 (Smoke): ----   ");
        drv_lcd_print_at(0, 2, "MQ4 (CH4):   ----   ");
        drv_lcd_print_at(0, 3, "MQ9 (CO):    ----   ");
    }
}

screen_id_t screen_gas_input(button_id_t button)
{
    switch (button) {
        case BTN_RIGHT: return SCREEN_FAULT_LOG;
        case BTN_LEFT:  return SCREEN_TEMPERATURE;
        default:        return SCREEN_GAS;
    }
}