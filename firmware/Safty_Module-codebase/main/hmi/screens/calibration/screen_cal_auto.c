// ═══ FILE: main/hmi/screens/calibration/screen_cal_auto.c ═══
/**
 * @file    screen_cal_auto.c
 * @brief   Automatic calibration — simulated sampling with progress bar.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — demo/stub, no real calibration performed.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "hmi/ui_animations.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_cal_auto";

typedef enum {
    CAL_AUTO_IDLE    = 0,
    CAL_AUTO_RUNNING = 1,
    CAL_AUTO_DONE    = 2
} cal_auto_state_t;

/* Justification: Calibration state persists across calls. */
static cal_auto_state_t s_state;
static uint32_t         s_frame;

void screen_cal_auto_enter(void)
{
    ESP_LOGI(TAG, "enter");
    s_state = CAL_AUTO_IDLE;
    s_frame = 0;
    drv_lcd2004_clear();
}

void screen_cal_auto_update(void)
{
    char buf[LCD_COLS + 1];

    switch (s_state) {
        case CAL_AUTO_IDLE:
            drv_lcd2004_write_line(0, "  AUTO CALIBRATE  ");
            drv_lcd2004_write_line(1, " Run machine at   ");
            drv_lcd2004_write_line(2, " normal load first");
            drv_lcd2004_write_line(3, "OK=Start    < Back");
            break;

        case CAL_AUTO_RUNNING: {
            s_frame++;
            uint8_t pct = (uint8_t)(s_frame * 5U);
            if (pct > 100) { pct = 100; }

            drv_lcd2004_write_line(0, "  CALIBRATING...  ");
            ui_anim_draw_progress_bar(1, 0, LCD_COLS, pct);

            snprintf(buf, sizeof(buf), "  Sampling: %3u%%", pct);
            drv_lcd2004_write_line(2, buf);

            /* Spinner on row 3. */
            drv_lcd2004_write_line(3, "  Please wait...  ");
            ui_anim_draw_spinner(3, 18, s_frame);

            if (pct >= 100) {
                s_state = CAL_AUTO_DONE;
                drv_buzzer_play(BUZZER_CONFIRM);
            }
            break;
        }

        case CAL_AUTO_DONE:
            drv_lcd2004_write_line(0, " CALIBRATION DONE ");
            drv_lcd2004_write_line(1, " Baseline stored  ");
            drv_lcd2004_write_line(2, "");
            drv_lcd2004_write_line(3, "  OK to continue  ");
            break;
    }
}

bool screen_cal_auto_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED) { return false; }

    switch (s_state) {
        case CAL_AUTO_IDLE:
            if (e->button == BTN_OK) {
                s_state = CAL_AUTO_RUNNING;
                s_frame = 0;
                drv_buzzer_play(BUZZER_CLICK);
                return true;
            }
            if (e->button == BTN_LEFT) { return false; }
            return true;

        case CAL_AUTO_RUNNING:
            /* Consume all events while running — user cannot interrupt. */
            return true;

        case CAL_AUTO_DONE:
            return false;  /* Any press pops back. */
    }

    return false;
}

void screen_cal_auto_exit(void)
{
    /* Nothing to clean up. */
}