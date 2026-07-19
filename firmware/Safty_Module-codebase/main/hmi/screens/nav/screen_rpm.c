/**
 * @file    screen_rpm.c
 * @brief   RPM tachometer display with bar graph.
 * @version 2.0.0
 * @date    2025-01-01
 */

#include "hmi/screens/screens.h"
#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/measurement/measurement.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>

static float s_prev_rpm = 0.0f;
static bool  s_has_prev = false;

static const char *trend_str(float current, float previous, bool valid)
{
    if (!valid) { return "     "; }
    float d = current - previous;
    if (d > 20.0f)  { return "^^^^^"; }
    if (d > 5.0f)   { return " /// "; }
    if (d < -20.0f) { return "vvvvv"; }
    if (d < -5.0f)  { return " \\\\\\ "; }
    return "-----";
}

void screen_rpm_enter(void)
{
    s_has_prev = false;
    drv_lcd2004_clear();
}

void screen_rpm_update(void)
{
    sensor_reading_t r;
    measurement_get_rpm(&r);

    float rpm = r.scaled_value;
    float limit = 1800.0f;
    bool stopped = (rpm < 10.0f);
    bool overspeed = (rpm > limit);

    char line[LCD_COLS + 1];

    /* Row 0 */
    if (overspeed) {
        snprintf(line, sizeof(line), "! OVERSPEED ! ~ !!");
    } else if (stopped) {
        snprintf(line, sizeof(line), "RPM MONITOR   ~ --");
    } else {
        snprintf(line, sizeof(line), "RPM MONITOR   ~ OK");
    }
    drv_lcd2004_write_line(0, line);

    /* Row 1 */
    if (stopped) {
        snprintf(line, sizeof(line), "    0 RPM   STOPPED");
    } else {
        const char *tr = trend_str(rpm, s_prev_rpm, s_has_prev);
        int rpm_i = (int)rpm;
        if (rpm_i > 9999) { rpm_i = 9999; }
        snprintf(line, sizeof(line), " %4d RPM   %s",
                 rpm_i, tr);
    }
    drv_lcd2004_write_line(1, line);

    /* Row 2: build manually to guarantee 20 chars */
    int pct = stopped ? 0 : (int)((rpm / limit) * 100.0f);
    if (pct > 99) { pct = 99; }
    if (pct < 0)  { pct = 0; }
    int lim_i = (int)limit;

    int filled = (pct * 8) / 100;
    char row2[LCD_COLS + 1];
    memset(row2, ' ', LCD_COLS);
    row2[LCD_COLS] = '\0';

    /* "Lm:1800 [========]99%" */
    int pos = snprintf(row2, sizeof(row2), "Lm:%d [", lim_i);
    for (int i = 0; i < 8; i++) {
        row2[pos++] = (i < filled) ? '=' : ' ';
    }
    row2[pos++] = ']';

    if (overspeed) {
        row2[pos++] = '!';
        row2[pos++] = '!';
    } else {
        char pstr[5];
        snprintf(pstr, sizeof(pstr), "%d%%", pct);
        for (int i = 0; pstr[i] && pos < LCD_COLS; i++) {
            row2[pos++] = pstr[i];
        }
    }
    row2[LCD_COLS] = '\0';
    drv_lcd2004_write_line(2, row2);

    /* Row 3 */
    if (overspeed) {
        drv_lcd2004_write_line(3, "<VIB   OK:ACK  SYS>");
    } else {
        drv_lcd2004_write_line(3, "<VIB   \x01MENU   SYS>");
    }

    s_prev_rpm = rpm;
    s_has_prev = true;
}

bool screen_rpm_handle_event(const button_event_t *e)
{
    UNUSED(e);
    return false;
}

void screen_rpm_exit(void)
{
}