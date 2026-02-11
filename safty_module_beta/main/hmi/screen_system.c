/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screen_system.c - System Info Screen                                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "screens.h"
#include "hmi_manager.h"
#include "drv_lcd2004.h"
#include "app_config.h"

#include "esp_system.h"
#include "esp_heap_caps.h"

extern const system_status_t* hmi_get_status(void);

void screen_system_render(bool full_redraw)
{
    const system_status_t *st = hmi_get_status();
    
    if (full_redraw) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "== SYSTEM INFO =====");
    }
    
    /* Firmware version */
    drv_lcd_printf_at(0, 1, "FW: %s", FIRMWARE_VERSION_STRING);
    
    /* Uptime */
    uint32_t uptime_s = (st != NULL) ? st->uptime_seconds : 0;
    uint32_t hours = uptime_s / 3600;
    uint32_t mins = (uptime_s % 3600) / 60;
    drv_lcd_printf_at(0, 2, "Up: %luh %lum", hours, mins);
    
    /* Free heap */
    uint32_t free_heap = esp_get_free_heap_size() / 1024;
    drv_lcd_printf_at(0, 3, "Heap: %lu KB free", free_heap);
}

screen_id_t screen_system_input(button_id_t button)
{
    switch (button) {
        case BTN_RIGHT: return SCREEN_HOME;
        case BTN_LEFT:  return SCREEN_SETTINGS;
        default:        return SCREEN_SYSTEM_INFO;
    }
}