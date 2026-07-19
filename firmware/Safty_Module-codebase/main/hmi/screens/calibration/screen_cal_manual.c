// ═══ FILE: main/hmi/screens/calibration/screen_cal_manual.c ═══
/**
 * @file    screen_cal_manual.c
 * @brief   Manual calibration — enter known reference value.
 *          Simplified for graduation demo.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — calibration stub, no real offset applied.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/measurement/measurement.h"
#include "hmi/menus/menu_settings.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_cal_manual";

typedef enum {
    CAL_MAN_EDIT = 0,
    CAL_MAN_DONE = 1
} cal_man_state_t;

/* Justification: Calibration state persists across calls. */
static cal_man_state_t s_state;
static float           s_reference_value;
static uint32_t        s_type_index;

/**
 * @brief  Get a live reading for the current type (L1 for 3-phase).
 */
static float get_live_reading(void)
{
    sensor_type_t t = g_threshold_types[s_type_index].type;
    three_phase_reading_t tp;
    sensor_reading_t r;

    switch (t) {
        case SENSOR_VOLTAGE:
            measurement_get_voltage(&tp); return tp.L1.scaled_value;
        case SENSOR_CURRENT:
            measurement_get_current(&tp); return tp.L1.scaled_value;
        case SENSOR_TEMP:
            measurement_get_temperature(&r); return r.scaled_value;
        case SENSOR_VIBRATION: {
            sensor_reading_t y;
            measurement_get_vibration(&r, &y);
            return r.scaled_value;
        }
        case SENSOR_RPM:
            measurement_get_rpm(&r); return r.scaled_value;
        default:
            measurement_get_gas(t, &r); return r.scaled_value;
    }
}

void screen_cal_manual_enter(void)
{
    ESP_LOGI(TAG, "enter");
    s_state = CAL_MAN_EDIT;
    s_type_index = 0;
    s_reference_value = get_live_reading();
    drv_lcd2004_clear();
}

void screen_cal_manual_update(void)
{
    const threshold_type_info_t *ti = &g_threshold_types[s_type_index];
    char buf[LCD_COLS + 1];

    if (s_state == CAL_MAN_DONE) {
        drv_lcd2004_write_line(0, " CALIBRATION SAVED");
        snprintf(buf, sizeof(buf), "Ref: %8.2f",
                 (double)s_reference_value);
        drv_lcd2004_write_line(1, buf);
        drv_lcd2004_write_line(2, "  Offset applied  ");
        drv_lcd2004_write_line(3, "  OK to continue  ");
        return;
    }

    snprintf(buf, sizeof(buf), "MANUAL CAL: %-7s", ti->name);
    drv_lcd2004_write_line(0, buf);

    snprintf(buf, sizeof(buf), "Current: %8.2f",
             (double)get_live_reading());
    drv_lcd2004_write_line(1, buf);

    snprintf(buf, sizeof(buf), "Ref:  > %8.2f <",
             (double)s_reference_value);
    drv_lcd2004_write_line(2, buf);

    drv_lcd2004_write_line(3, "UP/DN=Adj  OK=Save");
}

bool screen_cal_manual_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (s_state == CAL_MAN_DONE) {
        if (e->event == BTN_EVENT_PRESSED) { return false; }
        return true;
    }

    const threshold_type_info_t *ti = &g_threshold_types[s_type_index];

    if (e->button == BTN_UP &&
        (e->event == BTN_EVENT_PRESSED || e->event == BTN_EVENT_REPEAT)) {
        float step = (e->event == BTN_EVENT_REPEAT)
                     ? ti->step_large : ti->step_small;
        s_reference_value += step;
        drv_buzzer_play(BUZZER_CLICK);
        return true;
    }

    if (e->button == BTN_DOWN &&
        (e->event == BTN_EVENT_PRESSED || e->event == BTN_EVENT_REPEAT)) {
        float step = (e->event == BTN_EVENT_REPEAT)
                     ? ti->step_large : ti->step_small;
        s_reference_value -= step;
        drv_buzzer_play(BUZZER_CLICK);
        return true;
    }

    if (e->event != BTN_EVENT_PRESSED) { return false; }

    switch (e->button) {
        case BTN_RIGHT:
            s_type_index = (s_type_index + 1) % g_threshold_type_count;
            s_reference_value = get_live_reading();
            drv_buzzer_play(BUZZER_NAV);
            return true;

        case BTN_OK:
            s_state = CAL_MAN_DONE;
            drv_buzzer_play(BUZZER_CONFIRM);
            ESP_LOGI(TAG, "Cal saved: type=%d ref=%.2f",
                     (int)ti->type, (double)s_reference_value);
            return true;

        case BTN_LEFT:
            return false;

        default:
            return false;
    }
}

void screen_cal_manual_exit(void)
{
    /* Nothing to clean up. */
}