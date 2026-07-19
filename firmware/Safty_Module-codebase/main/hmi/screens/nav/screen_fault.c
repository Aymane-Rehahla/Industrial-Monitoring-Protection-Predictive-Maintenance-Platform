/**
 * @file    screen_fault.c
 * @brief   Fault log browser with scrolling and acknowledge.
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

static uint32_t s_index         = 0;
static bool     s_confirm_clear = false;
static uint32_t s_blink_tick    = 0;

static void render_empty(void)
{
    drv_lcd2004_write_line(0, "FAULT LOG      [0/0]");
    drv_lcd2004_write_line(1, "                    ");
    drv_lcd2004_write_line(2, "  No Faults Logged  ");
    drv_lcd2004_write_line(3, "<BACK       OK:HOME>");
}

static void render_confirm(void)
{
    drv_lcd2004_write_line(0, "! CLEAR ALL FAULTS ?");
    drv_lcd2004_write_line(1, "This cannot be      ");
    drv_lcd2004_write_line(2, "undone! OK=Yes      ");
    drv_lcd2004_write_line(3, "<CANCEL      OK:YES>");
}

static void render_entry(void)
{
    uint32_t count = 0;
    fault_handler_get_count(&count);

    if (s_index >= count) {
        s_index = (count > 0) ? count - 1 : 0;
    }

    fault_entry_t entry;
    error_code_t rc = fault_handler_get_entry(s_index, &entry);

    char line[LCD_COLS + 1];

    /* Build header manually — guaranteed 20 chars */
    memset(line, ' ', LCD_COLS);
    line[LCD_COLS] = '\0';
    memcpy(line, "FAULT LOG", 9);

    /* Position 13: [XX/XX] */
    uint8_t ci = (uint8_t)((s_index + 1) % 100U);
    uint8_t ct = (uint8_t)(count % 100U);
    char idx_buf[8];
    snprintf(idx_buf, sizeof(idx_buf), "[%2u/%2u]", ci, ct);
    memcpy(&line[13], idx_buf, 7);
    drv_lcd2004_write_line(0, line);

    if (rc != ERR_OK) {
        drv_lcd2004_write_line(1, " Error reading log  ");
        drv_lcd2004_write_line(2, "                    ");
        drv_lcd2004_write_line(3, "<PREV  OK:CLR NEXT>");
        return;
    }

    /* Row 1: fault name + measured > threshold */
    const char *fname = fault_to_str(entry.code);
    memset(line, ' ', LCD_COLS);
    line[LCD_COLS] = '\0';

    uint8_t nlen = 0;
    while (fname[nlen] && nlen < 10) { nlen++; }
    memcpy(line, fname, nlen);

    char val_buf[12];
    snprintf(val_buf, sizeof(val_buf), "%4d>%4d",
             (int)entry.measured_value % 10000,
             (int)entry.threshold_value % 10000);
    memcpy(&line[11], val_buf, 9);
    drv_lcd2004_write_line(1, line);

    /* Row 2: timestamp + severity + ack */
    uint32_t total_s = entry.timestamp_ms / 1000U;
    uint8_t hh = (uint8_t)((total_s / 3600U) % 100U);
    uint8_t mm = (uint8_t)((total_s % 3600U) / 60U);
    uint8_t ss = (uint8_t)(total_s % 60U);

    memset(line, ' ', LCD_COLS);
    line[LCD_COLS] = '\0';

    char time_buf[9];
    snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u", hh, mm, ss);
    memcpy(line, time_buf, 8);

    const char *sev = severity_to_str(entry.severity);
    uint8_t slen = 0;
    while (sev[slen] && slen < 4) { slen++; }
    memcpy(&line[9], sev, slen);

    const char *ack = entry.is_acknowledged ? "ACK" : "NEW";
    memcpy(&line[15], ack, 3);
    drv_lcd2004_write_line(2, line);

    drv_lcd2004_write_line(3, "<PREV  OK:CLR NEXT>");
}

void screen_fault_enter(void)
{
    uint32_t count = 0;
    fault_handler_get_count(&count);
    s_index = (count > 0) ? count - 1 : 0;
    s_confirm_clear = false;
    s_blink_tick = 0;
    drv_lcd2004_clear();
}

void screen_fault_update(void)
{
    s_blink_tick++;

    if (s_confirm_clear) {
        render_confirm();
        return;
    }

    uint32_t count = 0;
    fault_handler_get_count(&count);

    if (count == 0) {
        render_empty();
    } else {
        render_entry();
    }
}

bool screen_fault_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED) { return false; }

    if (s_confirm_clear) {
        if (e->button == BTN_OK) {
            fault_handler_acknowledge_all();
            s_confirm_clear = false;
            drv_lcd2004_clear();
            drv_buzzer_play(BUZZER_CONFIRM);
            return true;
        }
        if (e->button == BTN_LEFT) {
            s_confirm_clear = false;
            drv_lcd2004_clear();
            drv_buzzer_play(BUZZER_NAV);
            return true;
        }
        return true;
    }

    uint32_t count = 0;
    fault_handler_get_count(&count);

    if (e->button == BTN_LEFT || e->button == BTN_UP) {
        if (count > 0 && s_index > 0) {
            s_index--;
            drv_buzzer_play(BUZZER_NAV);
        }
        return true;
    }

    if (e->button == BTN_RIGHT || e->button == BTN_DOWN) {
        if (count > 0 && s_index < count - 1) {
            s_index++;
            drv_buzzer_play(BUZZER_NAV);
        }
        return true;
    }

    if (e->button == BTN_OK) {
        if (count > 0) {
            s_confirm_clear = true;
            drv_lcd2004_clear();
            drv_buzzer_play(BUZZER_WARNING);
        } else {
            hmi_manager_request_screen(SCREEN_HOME);
        }
        return true;
    }

    return false;
}

void screen_fault_exit(void)
{
    s_confirm_clear = false;
}