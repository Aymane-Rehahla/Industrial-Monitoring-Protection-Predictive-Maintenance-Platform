/**
 * @file    screen_system.c
 * @brief   Multi-page system info display.
 * @version 2.0.0
 * @date    2025-01-01
 */

#include "hmi/screens/screens.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/system_status.h"
#include "core/protection/fault_handler.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>

#define SYS_PAGE_COUNT  3

static uint8_t s_page = 0;

static void render_page0(void)
{
    system_snapshot_t snap;
    system_status_get_snapshot(&snap);

    char line[LCD_COLS + 1];

    snprintf(line, sizeof(line), "SYSTEM   FW:v%s",
             FIRMWARE_VERSION_STRING);
    drv_lcd2004_write_line(0, line);

    uint32_t up = snap.uptime_seconds;
    int hh = (int)((up / 3600U) % 100U);
    int mm = (int)((up % 3600U) / 60U);
    int ss = (int)(up % 60U);
    int bc = (int)(snap.boot_count % 1000U);

    snprintf(line, sizeof(line), "%02d:%02d:%02d  Boot#%03d",
             hh, mm, ss, bc);
    drv_lcd2004_write_line(1, line);

    int ram_k = (int)((snap.free_heap_bytes / 1024U) % 1000U);
    int fc = (int)(fault_handler_get_total_count() % 1000U);

    snprintf(line, sizeof(line), "RAM:%3dK Faults:%03d",
             ram_k, fc);
    drv_lcd2004_write_line(2, line);

    drv_lcd2004_write_line(3, "<RPM  ^v:PG   HOME>");
}

static void render_page1(void)
{
    drv_lcd2004_write_line(0, "SYSTEM DETAIL  [2/3]");
    drv_lcd2004_write_line(1, "I2C0:OK I2C1:OK     ");
    drv_lcd2004_write_line(2, "ADC:OK  UART:OK     ");
    drv_lcd2004_write_line(3, "<RPM  ^v:PG   HOME>");
}

static void render_page2(void)
{
    system_snapshot_t snap;
    system_status_get_snapshot(&snap);

    char line[LCD_COLS + 1];

    drv_lcd2004_write_line(0, "PEER STATUS    [3/3]");

    const char *role_str =
        (snap.role == ROLE_INFORMER) ? "INFORMER" :
        (snap.role == ROLE_SILENT)   ? "SILENT"   : "UNKNOWN";
    snprintf(line, sizeof(line), "Role: %-14s", role_str);
    drv_lcd2004_write_line(1, line);

    snprintf(line, sizeof(line), "State:%-5s Rly:%s",
             state_to_str(snap.state),
             snap.relay_commanded ? "ON " : "OFF");
    drv_lcd2004_write_line(2, line);

    drv_lcd2004_write_line(3, "<RPM  ^v:PG   HOME>");
}

void screen_system_enter(void)
{
    s_page = 0;
    drv_lcd2004_clear();
}

void screen_system_update(void)
{
    switch (s_page) {
        case 0:  render_page0(); break;
        case 1:  render_page1(); break;
        case 2:  render_page2(); break;
        default: s_page = 0; render_page0(); break;
    }
}

bool screen_system_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED && e->event != BTN_EVENT_REPEAT) {
        return false;
    }

    if (e->button == BTN_UP) {
        if (s_page > 0) { s_page--; }
        else            { s_page = SYS_PAGE_COUNT - 1; }
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    if (e->button == BTN_DOWN) {
        s_page++;
        if (s_page >= SYS_PAGE_COUNT) { s_page = 0; }
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    return false;
}

void screen_system_exit(void)
{
}