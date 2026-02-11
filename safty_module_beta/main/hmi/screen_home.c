/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_home.c - Home/Overview Screen                                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"

/* External accessor */
extern const sensor_set_t* hmi_get_sensors(void);
extern const system_status_t* hmi_get_status(void);

/* ═══════════════════════════════════════════════════════════════════════════════ */

void screen_home_render(bool full_redraw)
{
    const sensor_set_t *s = hmi_get_sensors();
    const system_status_t *st = hmi_get_status();
    
    if (full_redraw) {
        drv_lcd_clear();
    }
    
    /* Line 0: Title + Status */
    const char *status_str = "???";
    if (st != NULL) {
        switch (st->state) {
            case STATE_BOOT:      status_str = "BOOT"; break;
            case STATE_INIT:      status_str = "INIT"; break;
            case STATE_SELF_TEST: status_str = "TEST"; break;
            case STATE_READY:     status_str = "READY"; break;
            case STATE_RUNNING:   status_str = "RUN"; break;
            case STATE_WARNING:   status_str = "WARN"; break;
            case STATE_FAULT:     status_str = "FAULT"; break;
            case STATE_SAFE_MODE: status_str = "SAFE"; break;
        }
    }
    drv_lcd_printf_at(0, 0, "SAFETY MODULE  [%4s]", status_str);
    
    /* Line 1: Voltage (3 phases) */
    if (s != NULL) {
        drv_lcd_printf_at(0, 1, "V:%3.0f %3.0f %3.0f V",
                          s->voltage_L1.scaled_value,
                          s->voltage_L2.scaled_value,
                          s->voltage_L3.scaled_value);
    } else {
        drv_lcd_print_at(0, 1, "V: --- --- --- V    ");
    }
    
    /* Line 2: Current (3 phases) */
    if (s != NULL) {
        drv_lcd_printf_at(0, 2, "I:%4.1f%4.1f%4.1f A",
                          s->current_L1.scaled_value / 1000.0f,
                          s->current_L2.scaled_value / 1000.0f,
                          s->current_L3.scaled_value / 1000.0f);
    } else {
        drv_lcd_print_at(0, 2, "I: --- --- --- A    ");
    }
    
    /* Line 3: Temp + RPM */
    if (s != NULL) {
        drv_lcd_printf_at(0, 3, "T:%4.1fC RPM:%5.0f",
                          s->temp_machine.scaled_value,
                          s->rpm);
    } else {
        drv_lcd_print_at(0, 3, "T:---C RPM:-----    ");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════ */

screen_id_t screen_home_input(button_id_t button)
{
    switch (button) {
        case BTN_RIGHT: return SCREEN_VOLTAGE;
        case BTN_LEFT:  return SCREEN_SYSTEM_INFO;
        case BTN_DOWN:  return SCREEN_SETTINGS;
        default:        return SCREEN_HOME;
    }
}