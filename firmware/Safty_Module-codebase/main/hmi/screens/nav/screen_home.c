/**
 * @file    screen_home.c
 * @brief   Home dashboard — industrial layout following ISA-101 principles.
 *          Row 0: State + security mode + uptime
 *          Row 1: Voltage, current, power (computed)
 *          Row 2: Warning summary with rotation
 *          Row 3: Navigation footer
 * @version 4.1.0
 * @date    2025-01-01
 * @safety  LOW — display only.
 *
 * CHANGELOG:
 *   4.1.0  2025-01-01  Fix state-mode gap, row1 layout, centered N/A.
 *   4.0.0  2025-01-01  Industrial redesign. No icons, no animation,
 *                      no hearts. Clean fixed-width columns.
 *   3.0.0  Previous icon-based version (removed).
 */

#include "hmi/screens/screens.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "core/system_status.h"
#include "core/measurement/measurement.h"
#include "core/protection/fault_handler.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Timing ──────────────────────────────────────────────────────────── */

static uint32_t s_tick = 0;

/*
 * Warning rotation: cycle through active faults every N ticks.
 * At LCD_UPDATE_INTERVAL_MS = 250ms, 12 ticks = 3 seconds.
 */
#define WARNING_ROTATE_TICKS  12

/* ── State-to-string mapping ─────────────────────────────────────────── */

/**
 * @brief  Get short state string for Row 0.
 *
 * Only states that can appear on the home screen are mapped.
 * BOOT and FAULT are handled by other screens.
 */
static const char *state_short_str(system_state_t state)
{
    switch (state) {
        case SYS_STATE_CONFIG:   return "CNFG";
        case SYS_STATE_VALIDATE: return "CHCK";
        case SYS_STATE_READY:    return "READY";
        case SYS_STATE_RUN:      return "RUN";
        default:                 return "---";
    }
}

/**
 * @brief  Get 2-char security mode tag.
 */
static const char *security_short_str(security_mode_t mode)
{
    switch (mode) {
        case SECURITY_HIGH:   return "HI";
        case SECURITY_NORMAL: return "NR";
        case SECURITY_LOW:    return "LO";
        case SECURITY_CUSTOM: return "CU";
        default:              return "??";
    }
}

/* ── Row 0: State + Mode + Uptime ────────────────────────────────────── */

static void render_row0(void)
{
    system_state_t  state = system_status_get_state();
    security_mode_t mode  = system_status_get_security_mode();
    uint32_t uptime_s     = system_status_get_uptime_seconds();

    /* Clamp uptime to 99:59:59 for display */
    if (uptime_s > 359999U) { uptime_s = 359999U; }

    uint32_t hh = uptime_s / 3600U;
    uint32_t mm = (uptime_s % 3600U) / 60U;
    uint32_t ss = uptime_s % 60U;

    const char *state_str = state_short_str(state);
    const char *mode_str  = security_short_str(mode);

    /*
     * Build tag without internal padding so state and mode
     * stick together:
     *   "RUN-HI"    (6)    "READY-HI"  (8)
     *   "CNFG"      (4)    "CHCK"      (4)
     */
    char tag[12];
    if (state == SYS_STATE_RUN || state == SYS_STATE_READY) {
        snprintf(tag, sizeof(tag), "%s-%s", state_str, mode_str);
    } else {
        snprintf(tag, sizeof(tag), "%s", state_str);
    }

    /*
     * Layout — 20 characters:
     *   "RUN-HI      00:14:32"
     *   "READY-HI    00:00:05"
     *   "CNFG        00:00:02"
     */
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "%-12s%02lu:%02lu:%02lu",
             tag,
             (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);

    drv_lcd2004_write_line(0, line);
}

/* ── Row 1: Voltage + Current + Power ────────────────────────────────── */

static void render_row1(const measurement_snapshot_t *snap)
{
    /*
     * Average 3-phase voltage and current.
     * Power = V * I * sqrt(3) / 1000  (3-phase apparent power in kW).
     */
    float v_avg = (snap->voltage.L1.scaled_value +
                   snap->voltage.L2.scaled_value +
                   snap->voltage.L3.scaled_value) / 3.0f;

    float i_avg = (snap->current.L1.scaled_value +
                   snap->current.L2.scaled_value +
                   snap->current.L3.scaled_value) / 3.0f;

    float power_kw = (v_avg * i_avg * 1.732f) / 1000.0f;

    /* Clamp to display ranges */
    int v_display = (int)v_avg;
    if (v_display < 0)    { v_display = 0; }
    if (v_display > 999)  { v_display = 999; }

    if (i_avg < 0.0f)     { i_avg = 0.0f; }
    if (i_avg > 99.9f)    { i_avg = 99.9f; }

    if (power_kw < 0.0f)  { power_kw = 0.0f; }
    if (power_kw > 99.9f) { power_kw = 99.9f; }

    /* Convert to fixed-point tenths — avoids %f truncation warnings */
    int i_x10 = (int)(i_avg * 10.0f);
    int p_x10 = (int)(power_kw * 10.0f);

    /* Belt-and-suspenders clamp for GCC static analysis */
    if (i_x10 < 0)   { i_x10 = 0; }
    if (i_x10 > 999) { i_x10 = 999; }
    if (p_x10 < 0)   { p_x10 = 0; }
    if (p_x10 > 999) { p_x10 = 999; }

    /*
     * Fixed-column layout — exactly 20 characters:
     *
     * Cols:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
     *        2  1  9  V        _  1  2  .  6  A        _  4  .  7  k  W
     *        3  8  0  V        _  1  5  .  0  A        _  2  .  8  k  W
     *        9  9  9  V        _  9  9  .  9  A        9  9  .  9  k  W
     *
     * Format: %3dV  %3d.%dA  %2d.%dkW
     *         ────  ───────  ────────
     *          4  2    6    2    6     = 20
     */
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "%3dV  %3d.%dA  %2d.%dkW",
             v_display,
             i_x10 / 10, i_x10 % 10,
             p_x10 / 10, p_x10 % 10);

    drv_lcd2004_write_line(1, line);
}

/* ── Row 2: Warning Summary ──────────────────────────────────────────── */

static void render_row2(void)
{
    uint32_t fault_count = 0;
    fault_handler_get_count(&fault_count);

    char line[LCD_COLS + 1];

    if (fault_count == 0) {
        /*
         * No warnings — centered N/A with 4 spaces each side.
         * "Warnings:    N/A    "
         *  ─────────╶4╴───╶4──╴
         */
        snprintf(line, sizeof(line), "Warnings:    N/A    ");
        drv_lcd2004_write_line(2, line);
        return;
    }

    /*
     * Warnings exist — rotate through them.
     * Calculate which fault to show based on tick counter.
     */
    uint32_t display_index = (s_tick / WARNING_ROTATE_TICKS) % fault_count;

    fault_entry_t entry;
    error_code_t rc = fault_handler_get_entry(display_index, &entry);

    if (rc != ERR_OK) {
        /* Can't read entry — show count only */
        snprintf(line, sizeof(line), "Warnings:         %02d",
                 (int)(fault_count % 100U));
        drv_lcd2004_write_line(2, line);
        return;
    }

    /*
     * Show fault name + index/total.
     *
     * Layout:
     *   "! OVERCURRENT    1/3"
     *
     * Position: 01234567890123456789
     *           ! XXXXXXXXXXXX NN/NN
     *
     * Fault name: max 12 chars (from fault_to_str).
     * Index: right-aligned.
     */
    const char *fname = fault_to_str(entry.code);

    uint32_t show_num   = display_index + 1;
    uint32_t show_total = fault_count;

    /* Clamp to 2 digits for display */
    if (show_num > 99)   { show_num = 99; }
    if (show_total > 99) { show_total = 99; }

    snprintf(line, sizeof(line), "! %-12s %2lu/%2lu",
             fname,
             (unsigned long)show_num,
             (unsigned long)show_total);

    drv_lcd2004_write_line(2, line);
}

/* ── Row 3: Navigation Footer ────────────────────────────────────────── */

static void render_row3(void)
{
    /*
     * Fixed footer — never changes.
     * Maps to physical buttons:
     *   LEFT  → <VOLT  (go to voltage screen)
     *   OK    → MENU   (enter settings)
     *   RIGHT → CURR>  (go to current screen)
     *
     * Position: 01234567890123456789
     *           <VOLT   MENU   CURR>
     */
    drv_lcd2004_write_line(3, "<VOLT   MENU   CURR>");
}

/* ── Screen Lifecycle ────────────────────────────────────────────────── */

void screen_home_enter(void)
{
    s_tick = 0;
    drv_lcd2004_clear();
}

void screen_home_update(void)
{
    measurement_snapshot_t snap;
    measurement_get_snapshot(&snap);

    render_row0();
    render_row1(&snap);
    render_row2();
    render_row3();

    s_tick++;
}

bool screen_home_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED && e->event != BTN_EVENT_REPEAT) {
        return false;
    }

    /*
     * DOWN button → enter settings menu.
     * LEFT/RIGHT are handled by hmi_manager default nav
     * (cycles through nav screens).
     * OK is handled by hmi_manager default nav (pushes SETTINGS).
     */
    if (e->button == BTN_DOWN) {
        hmi_manager_request_screen(SCREEN_SETTINGS);
        return true;
    }

    return false;
}

void screen_home_exit(void)
{
    /* Nothing to clean up */
}