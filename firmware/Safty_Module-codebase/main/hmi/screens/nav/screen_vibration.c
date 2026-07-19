/**
 * @file    screen_vibration.c
 * @brief   3-axis vibration display (X, Y, computed magnitude).
 * @version 2.0.0
 * @date    2025-01-01
 *
 * CHANGELOG:
 *   2.0.0  Replaced stub with X/Y/magnitude + peak tracking.
 */

#include "hmi/screens/screens.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/measurement/measurement.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ── View modes ──────────────────────────────────────────────────────── */
typedef enum {
    VIB_VIEW_SUMMARY = 0,
    VIB_VIEW_DETAIL  = 1
} vib_view_t;

static vib_view_t s_view      = VIB_VIEW_SUMMARY;
static float      s_peak_mag  = 0.0f;

/* ═══════════════════════════════════════════════════════════════════════
 *  Screen lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

void screen_vibration_enter(void)
{
    s_view     = VIB_VIEW_SUMMARY;
    s_peak_mag = 0.0f;
    drv_lcd2004_clear();
}

void screen_vibration_update(void)
{
    sensor_reading_t x, y;
    measurement_get_vibration(&x, &y);

    float mag = sqrtf(x.scaled_value * x.scaled_value +
                      y.scaled_value * y.scaled_value);
    if (mag > s_peak_mag) { s_peak_mag = mag; }

    bool is_alert = (mag > 1.0f);
    char line[LCD_COLS + 1];

    if (s_view == VIB_VIEW_SUMMARY) {
        /* Row 0 */
        if (is_alert) {
            snprintf(line, sizeof(line), "! HIGH VIBRATION ! ");
        } else {
            snprintf(line, sizeof(line), "VIBRATION      ~ OK");
        }
        drv_lcd2004_write_line(0, line);

        /* Row 1 */
        snprintf(line, sizeof(line), "X:%+5.2fg  Pk:%4.2fg",
                 (double)x.scaled_value, (double)s_peak_mag);
        drv_lcd2004_write_line(1, line);

        /* Row 2 */
        snprintf(line, sizeof(line), "Y:%+5.2fg Mag:%4.2fg",
                 (double)y.scaled_value, (double)mag);
        drv_lcd2004_write_line(2, line);

        /* Row 3 */
        drv_lcd2004_write_line(3, "<GAS   \x01MENU   RPM>");

    } else {
        /* Detail view with limit bar */
        float limit = 1.0f;  /* TODO: from config */
        uint8_t pct = (uint8_t)((mag / limit) * 100.0f);
        if (pct > 100) { pct = 100; }

        snprintf(line, sizeof(line), "VIB DETAIL     ~ %s",
                 is_alert ? "!!" : "OK");
        drv_lcd2004_write_line(0, line);

        snprintf(line, sizeof(line), "X:%+5.2f Y:%+5.2f   ",
                 (double)x.scaled_value, (double)y.scaled_value);
        drv_lcd2004_write_line(1, line);

        snprintf(line, sizeof(line), "Mag:%4.2f Pk:%4.2fg",
                 (double)mag, (double)s_peak_mag);
        drv_lcd2004_write_line(2, line);

        snprintf(line, sizeof(line), "Lim:%3.1fg        %3u%%",
                 (double)limit, pct);
        drv_lcd2004_write_line(3, line);
    }
}

bool screen_vibration_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED) { return false; }

    if (e->button == BTN_UP) {
        s_view = (s_view == VIB_VIEW_SUMMARY) ? VIB_VIEW_DETAIL
                                               : VIB_VIEW_SUMMARY;
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    if (e->button == BTN_OK) {
        s_peak_mag = 0.0f;
        drv_buzzer_play(BUZZER_CONFIRM);
        return true;
    }

    return false;
}

void screen_vibration_exit(void) { }