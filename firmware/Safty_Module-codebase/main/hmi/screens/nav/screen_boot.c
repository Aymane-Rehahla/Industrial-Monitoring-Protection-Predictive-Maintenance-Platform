/**
 * @file    screen_boot.c
 * @brief   3-phase boot screen: progress bar, checklist, splash.
 * @version 2.0.0
 * @date    2025-01-01
 */

#include "hmi/screens/screens.h"
#include "hmi/ui_animations.h"
#include "drivers/interface/drv_lcd2004.h"
#include "core/system_status.h"
#include "core/protection/fault_handler.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    BOOT_PHASE_PROGRESS  = 0,
    BOOT_PHASE_CHECKLIST = 1,
    BOOT_PHASE_SPLASH    = 2,
    BOOT_PHASE_DONE      = 3
} boot_phase_t;

#define PHASE1_TICKS       8
#define PHASE2_TICKS      16
#define PHASE2_ITEMS       8
#define PHASE3_TICKS      22
#define SPLASH_STAR_RATE   2

static boot_phase_t s_phase     = BOOT_PHASE_PROGRESS;
static uint32_t     s_tick      = 0;
static bool         s_done      = false;

typedef struct {
    uint8_t     row;
    uint8_t     col;
    const char *label;
} check_item_t;

static const check_item_t CHECKLIST[PHASE2_ITEMS] = {
    { 0,  0, "I2C"  },
    { 0,  5, "ADC"  },
    { 0, 10, "GPIO" },
    { 0, 15, "NVS"  },
    { 1,  0, "ADS1" },
    { 1,  5, "ADS2" },
    { 1, 10, "SHT"  },
    { 1, 15, "MQ"   },
};

static void render_phase_progress(void)
{
    drv_lcd2004_write_line(0, " SAFETY MODULE v1.0 ");
    drv_lcd2004_write_line(1, " ESP32-S3-N16R8     ");

    uint8_t pct = (uint8_t)((s_tick * 70U) / PHASE1_TICKS);
    if (pct > 70) { pct = 70; }

    ui_anim_draw_progress_bar(3, 1, 14, pct);

    char pct_str[6];
    snprintf(pct_str, sizeof(pct_str), "%3u%%", pct);
    drv_lcd2004_print_at(3, 16, pct_str);

    drv_lcd2004_print_at(2, 1, "Initializing");
    ui_anim_draw_spinner(2, 14, s_tick);
}

static void render_phase_checklist(void)
{
    uint32_t phase2_tick = s_tick - PHASE1_TICKS;
    uint32_t phase2_total = PHASE2_TICKS - PHASE1_TICKS;
    uint32_t items_done = (phase2_tick * PHASE2_ITEMS) / phase2_total;
    if (items_done > PHASE2_ITEMS) { items_done = PHASE2_ITEMS; }

    char row0[LCD_COLS + 1];
    char row1[LCD_COLS + 1];
    memset(row0, ' ', LCD_COLS); row0[LCD_COLS] = '\0';
    memset(row1, ' ', LCD_COLS); row1[LCD_COLS] = '\0';

    for (uint32_t i = 0; i < PHASE2_ITEMS; i++) {
        const check_item_t *ci = &CHECKLIST[i];
        char marker = (i < items_done) ? '\x07' : '\xA5';
        char *row = (ci->row == 0) ? row0 : row1;
        row[ci->col] = marker;
        const char *lbl = ci->label;
        for (uint8_t c = 0; lbl[c] != '\0' && (ci->col + 1 + c) < LCD_COLS; c++) {
            row[ci->col + 1 + c] = lbl[c];
        }
    }

    drv_lcd2004_write_line(0, row0);
    drv_lcd2004_write_line(1, row1);

    char row2[LCD_COLS + 1];
    memset(row2, ' ', LCD_COLS); row2[LCD_COLS] = '\0';
    const char *hmi_items[] = { "LCD", "BTN", "BUZ", "LED" };
    uint8_t hmi_cols[] = { 0, 5, 10, 15 };
    for (int h = 0; h < 4; h++) {
        bool done = (items_done >= (uint32_t)(PHASE2_ITEMS));
        row2[hmi_cols[h]] = done ? '\x07' : '\xA5';
        const char *hl = hmi_items[h];
        for (uint8_t c = 0; hl[c] && (hmi_cols[h] + 1 + c) < LCD_COLS; c++) {
            row2[hmi_cols[h] + 1 + c] = hl[c];
        }
    }
    drv_lcd2004_write_line(2, row2);

    uint8_t pct = 70 + (uint8_t)((phase2_tick * 25U) / phase2_total);
    if (pct > 95) { pct = 95; }

    char line3[LCD_COLS + 1];
    snprintf(line3, sizeof(line3), " INIT          %3u%%", pct);
    drv_lcd2004_write_line(3, line3);
    ui_anim_draw_spinner(3, 6, s_tick);
}

static void render_phase_splash(void)
{
    uint32_t splash_tick = s_tick - PHASE2_TICKS;
    bool star_on = ((splash_tick / SPLASH_STAR_RATE) % 2U) == 0;

    drv_lcd2004_write_line(0, "                    ");

    char line1[LCD_COLS + 1];
    snprintf(line1, sizeof(line1), "  %c SAFETY MODULE %c",
             star_on ? '*' : ' ', star_on ? '*' : ' ');
    drv_lcd2004_write_line(1, line1);

    ui_anim_center_text(2, "System Ready");

    int bc = (int)(fault_handler_get_total_count() % 1000U);
    char line3[LCD_COLS + 1];
    snprintf(line3, sizeof(line3), "  Boot#001  Flt:%03d", bc);
    drv_lcd2004_write_line(3, line3);
}

void screen_boot_enter(void)
{
    s_phase = BOOT_PHASE_PROGRESS;
    s_tick  = 0;
    s_done  = false;
    drv_lcd2004_clear();
    ui_anim_load_boot_chars();
}

void screen_boot_update(void)
{
    if (s_done) { return; }

    if (s_tick < PHASE1_TICKS) {
        if (s_phase != BOOT_PHASE_PROGRESS) {
            s_phase = BOOT_PHASE_PROGRESS;
            drv_lcd2004_clear();
        }
        render_phase_progress();
    } else if (s_tick < PHASE2_TICKS) {
        if (s_phase != BOOT_PHASE_CHECKLIST) {
            s_phase = BOOT_PHASE_CHECKLIST;
            drv_lcd2004_clear();
        }
        render_phase_checklist();
    } else if (s_tick < PHASE3_TICKS) {
        if (s_phase != BOOT_PHASE_SPLASH) {
            s_phase = BOOT_PHASE_SPLASH;
            drv_lcd2004_clear();
        }
        render_phase_splash();
    } else {
        s_phase = BOOT_PHASE_DONE;
        s_done  = true;
    }

    s_tick++;
}

bool screen_boot_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->button == BTN_OK && e->event == BTN_EVENT_PRESSED) {
        s_done = true;
        return true;
    }
    return false;
}

void screen_boot_exit(void)
{
    drv_lcd2004_clear();
}

bool screen_boot_is_done(void)
{
    return s_done;
}