/**
 * @file    screen_temp.c
 * @brief   Temperature + humidity dual display with zone indicators.
 * @version 2.0.0
 * @date    2025-01-01
 *
 * CHANGELOG:
 *   2.0.0  Replaced stub with dual display + trend + zones.
 */

#include "hmi/screens/screens.h"
#include "drivers/interface/drv_lcd2004.h"
#include "core/measurement/measurement.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>

/* ── Zone classification ─────────────────────────────────────────────── */

static const char *temp_zone(float t)
{
    if (t < 10.0f) { return "COLD"; }
    if (t < 25.0f) { return "COOL"; }
    if (t < 40.0f) { return "NORM"; }
    if (t < 60.0f) { return "WARM"; }
    return "HOT!";
}

static const char *humi_zone(float h)
{
    if (h < 20.0f) { return " DRY"; }
    if (h < 60.0f) { return "NORM"; }
    if (h < 80.0f) { return "DAMP"; }
    return " WET";
}

/* ── Trend tracking ──────────────────────────────────────────────────── */
static float s_prev_temp = 0.0f;
static float s_prev_humi = 0.0f;
static bool  s_has_prev  = false;

static char trend_char(float current, float previous, bool valid)
{
    if (!valid) { return ' '; }
    float d = current - previous;
    if (d > 0.3f)  { return '^'; }
    if (d < -0.3f) { return 'v'; }
    return '-';
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Screen lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

void screen_temp_enter(void)
{
    s_has_prev = false;
    drv_lcd2004_clear();
}

void screen_temp_update(void)
{
    sensor_reading_t t, h;
    measurement_get_temperature(&t);
    measurement_get_humidity(&h);

    char t_trend = trend_char(t.scaled_value, s_prev_temp, s_has_prev);
    char h_trend = trend_char(h.scaled_value, s_prev_humi, s_has_prev);

    bool is_alert = (t.scaled_value > 60.0f);
    char line[LCD_COLS + 1];

    /* Row 0: Title + status */
    if (is_alert) {
        snprintf(line, sizeof(line), "! OVER TEMP !  ~ !!");
    } else {
        snprintf(line, sizeof(line), "ENVIRONMENT    ~ OK");
    }
    drv_lcd2004_write_line(0, line);

    /* Row 1: Temperature + trend + zone */
    snprintf(line, sizeof(line), "Temp: %5.1fC  %c %4s",
             (double)t.scaled_value,
             t_trend,
             temp_zone(t.scaled_value));
    drv_lcd2004_write_line(1, line);

    /* Row 2: Humidity + trend + zone */
    snprintf(line, sizeof(line), "Humi: %5.1f%%  %c %4s",
             (double)h.scaled_value,
             h_trend,
             humi_zone(h.scaled_value));
    drv_lcd2004_write_line(2, line);

    /* Row 3: Nav */
    drv_lcd2004_write_line(3, "<CURR  \x01MENU   GAS>");

    /* Save for next trend calculation */
    s_prev_temp = t.scaled_value;
    s_prev_humi = h.scaled_value;
    s_has_prev  = true;
}

bool screen_temp_handle_event(const button_event_t *e)
{
    UNUSED(e);
    return false;  /* All nav handled by default handler */
}

void screen_temp_exit(void) { }