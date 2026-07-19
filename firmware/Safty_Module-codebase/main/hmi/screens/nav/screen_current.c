/**
 * @file    screen_current.c
 * @brief   3-phase current with calculated power display.
 * @version 2.0.0
 * @date    2025-01-01
 *
 * CHANGELOG:
 *   2.0.0  Replaced stub with 3-phase current + power calculation.
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

/* ── View mode ───────────────────────────────────────────────────────── */
typedef enum {
    CURR_VIEW_3PHASE = 0,
    CURR_VIEW_DETAIL_L1,
    CURR_VIEW_DETAIL_L2,
    CURR_VIEW_DETAIL_L3,
    CURR_VIEW_COUNT
} curr_view_t;

static curr_view_t s_view = CURR_VIEW_3PHASE;

/* ── Power factor assumption (stub — no real PF measurement) ────────── */
#define ASSUMED_PF  0.93f

/* ═══════════════════════════════════════════════════════════════════════
 *  Render: 3-phase overview with power
 * ═══════════════════════════════════════════════════════════════════════ */
static void render_3phase(void)
{
    three_phase_reading_t c, v;
    measurement_get_current(&c);
    measurement_get_voltage(&v);

    char line[LCD_COLS + 1];

    /* Row 0: Title */
    snprintf(line, sizeof(line), "CURRENT+POWER  ~ %s",
             c.all_valid ? "OK" : "!!");
    drv_lcd2004_write_line(0, line);

    /* Row 1: 3-phase currents */
    snprintf(line, sizeof(line), "A:%4.1f B:%4.1f C:%4.1f",
             (double)c.L1.scaled_value,
             (double)c.L2.scaled_value,
             (double)c.L3.scaled_value);
    drv_lcd2004_write_line(1, line);

    /* Row 2: Total power (3-phase) and power factor */
    float p_total = 0.0f;
    const sensor_reading_t *vp[3] = { &v.L1, &v.L2, &v.L3 };
    const sensor_reading_t *cp[3] = { &c.L1, &c.L2, &c.L3 };
    for (int i = 0; i < 3; i++) {
        p_total += vp[i]->scaled_value * cp[i]->scaled_value * ASSUMED_PF;
    }
    p_total /= 1000.0f;  /* Convert to kW */

    snprintf(line, sizeof(line), "P:%5.1fkW  PF:%4.2f",
             (double)p_total, (double)ASSUMED_PF);
    drv_lcd2004_write_line(2, line);

    /* Row 3: Nav */
    drv_lcd2004_write_line(3, "<VOLT  \x01MENU  TEMP>");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Render: Single phase detail with power and bar
 * ═══════════════════════════════════════════════════════════════════════ */
static void render_detail(int phase)
{
    three_phase_reading_t c, v;
    measurement_get_current(&c);
    measurement_get_voltage(&v);

    const sensor_reading_t *cr[3] = { &c.L1, &c.L2, &c.L3 };
    const sensor_reading_t *vr[3] = { &v.L1, &v.L2, &v.L3 };
    const char names[] = "ABC";
    char line[LCD_COLS + 1];

    float amps = cr[phase]->scaled_value;
    float power_kw = (vr[phase]->scaled_value * amps * ASSUMED_PF) / 1000.0f;

    /* Row 0 */
    snprintf(line, sizeof(line), "CURRENT PH-%c   ~ %s",
             names[phase],
             cr[phase]->is_valid ? "OK" : "!!");
    drv_lcd2004_write_line(0, line);

    /* Row 1: Current + power */
    snprintf(line, sizeof(line), " %5.2fA    %4.2fkW",
             (double)amps, (double)power_kw);
    drv_lcd2004_write_line(1, line);

    /* Row 2: Limit and percentage bar */
    float limit = 20.0f;  /* TODO: read from config */
    uint8_t pct = (uint8_t)((amps / limit) * 100.0f);
    if (pct > 100) { pct = 100; }

    snprintf(line, sizeof(line), "Lim:%4.1fA       %3u%%",
             (double)limit, pct);
    drv_lcd2004_write_line(2, line);

    /* Row 3 */
    drv_lcd2004_write_line(3, "<PREV \x00""3PH    NEXT>");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Screen lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

void screen_current_enter(void)
{
    s_view = CURR_VIEW_3PHASE;
    drv_lcd2004_clear();
}

void screen_current_update(void)
{
    if (s_view == CURR_VIEW_3PHASE) {
        render_3phase();
    } else {
        render_detail((int)s_view - 1);
    }
}

bool screen_current_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED && e->event != BTN_EVENT_REPEAT) {
        return false;
    }

    if (e->button == BTN_UP) {
        s_view = (s_view == CURR_VIEW_3PHASE) ? CURR_VIEW_DETAIL_L1
                                               : CURR_VIEW_3PHASE;
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    if (e->button == BTN_DOWN && s_view != CURR_VIEW_3PHASE) {
        s_view = (curr_view_t)(((int)s_view) + 1);
        if (s_view >= CURR_VIEW_COUNT) { s_view = CURR_VIEW_DETAIL_L1; }
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    return false;
}

void screen_current_exit(void) { }