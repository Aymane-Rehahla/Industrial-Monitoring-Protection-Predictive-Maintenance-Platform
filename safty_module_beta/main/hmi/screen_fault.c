/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_fault.c - Fault Log Screen                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"
#include "nvs_config.h"

static fault_log_t s_log;
static uint8_t s_view_index = 0;

void screen_fault_render(bool full_redraw)
{
    if (full_redraw) {
        nvs_config_load_fault_log(&s_log);
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== FAULT LOG =======");
    }
    
    if (s_log.count == 0) {
        drv_lcd_print_at(0, 1, "  No faults logged  ");
        drv_lcd_print_at(0, 2, "                    ");
        drv_lcd_print_at(0, 3, "                    ");
        return;
    }
    
    drv_lcd_printf_at(0, 1, "Entry %d of %d       ", s_view_index + 1, s_log.count);
    
    /* Get fault entry (ring buffer) */
    uint8_t idx = (s_log.head - s_log.count + s_view_index + FAULT_LOG_SIZE) % FAULT_LOG_SIZE;
    const fault_entry_t *f = &s_log.entries[idx];
    
    const char *name = "UNKNOWN";
    switch (f->error_code) {
        case ERR_OVERCURRENT:  name = "OVERCURRENT"; break;
        case ERR_OVERVOLTAGE:  name = "OVERVOLTAGE"; break;
        case ERR_UNDERVOLTAGE: name = "UNDERVOLT"; break;
        case ERR_OVERTEMP:     name = "OVERTEMP"; break;
        case ERR_GAS_DETECTED: name = "GAS"; break;
        default: break;
    }
    
    drv_lcd_printf_at(0, 2, "%-12s %.1f", name, f->value_at_fault);
    drv_lcd_printf_at(0, 3, "T+%lu sec", f->timestamp_ms / 1000);
}

screen_id_t screen_fault_input(button_id_t button)
{
    switch (button) {
        case BTN_UP:
            if (s_view_index > 0) {
                s_view_index--;
                hmi_manager_request_redraw();
            }
            return SCREEN_FAULT_LOG;
            
        case BTN_DOWN:
            if (s_view_index < s_log.count - 1) {
                s_view_index++;
                hmi_manager_request_redraw();
            }
            return SCREEN_FAULT_LOG;
            
        case BTN_RIGHT: return SCREEN_SETTINGS;
        case BTN_LEFT:  return SCREEN_GAS;
        default:        return SCREEN_FAULT_LOG;
    }
}