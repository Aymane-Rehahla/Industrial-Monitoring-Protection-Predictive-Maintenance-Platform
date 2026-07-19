// ═══ FILE: main/hmi/screens/settings/screen_threshold.c ═══
/**
 * @file    screen_threshold.c
 * @brief   Threshold value editor — browse and edit high/low limits
 *          for each sensor type.  Most complex HMI screen.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — changing thresholds affects protection behaviour.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/config_manager.h"
#include "hmi/hmi_manager.h"
#include "hmi/menus/menu_settings.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_threshold";

typedef enum {
    THRESH_MODE_BROWSE = 0,
    THRESH_MODE_EDIT   = 1
} thresh_mode_t;

/* Justification: Threshold editing state must persist across update()
 * and handle_event() calls.  Only HMI task.  File scope. */
static uint32_t          s_type_index  = 0;
static uint32_t          s_field_index = 0;   /* 0=high, 1=low */
static thresh_mode_t     s_mode        = THRESH_MODE_BROWSE;
static sensor_threshold_t s_thresh;
static float             s_original_value = 0.0f;

/** Load threshold for the currently selected sensor type. */
static void load_current_threshold(void)
{
    if (s_type_index < g_threshold_type_count) {
        config_manager_get_threshold(
            g_threshold_types[s_type_index].type, &s_thresh);
    }
}

/** Get pointer to the field being edited (high or low limit). */
static float *get_active_field(void)
{
    return (s_field_index == 0) ? &s_thresh.high_limit : &s_thresh.low_limit;
}

void screen_threshold_enter(void)
{
    ESP_LOGI(TAG, "enter");
    s_type_index  = 0;
    s_field_index = 0;
    s_mode        = THRESH_MODE_BROWSE;
    drv_lcd2004_clear();
    load_current_threshold();
}

/** Render the browse view — shows thresholds for current type. */
static void render_browse(void)
{
    const threshold_type_info_t *ti = &g_threshold_types[s_type_index];
    char buf[LCD_COLS + 1];

    snprintf(buf, sizeof(buf), "THRESH: %-11s", ti->name);
    drv_lcd2004_write_line(0, buf);

    snprintf(buf, sizeof(buf), "%cHigh: %7.1f %-3s",
             (s_field_index == 0) ? '>' : ' ',
             (double)s_thresh.high_limit, ti->unit_str);
    drv_lcd2004_write_line(1, buf);

    snprintf(buf, sizeof(buf), "%cLow:  %7.1f %-3s",
             (s_field_index == 1) ? '>' : ' ',
             (double)s_thresh.low_limit, ti->unit_str);
    drv_lcd2004_write_line(2, buf);

    drv_lcd2004_write_line(3, "<Back  R=Next  OK  ");
}

/** Render the edit view — shows editable value with adjustment hints. */
static void render_edit(void)
{
    const threshold_type_info_t *ti = &g_threshold_types[s_type_index];
    const char *field_name = (s_field_index == 0) ? "High" : "Low";
    float value = *get_active_field();
    char buf[LCD_COLS + 1];

    snprintf(buf, sizeof(buf), "EDIT: %-7s %-4s",
             ti->name, field_name);
    drv_lcd2004_write_line(0, buf);

    snprintf(buf, sizeof(buf), "  >>> %8.1f <<<",
             (double)value);
    drv_lcd2004_write_line(1, buf);

    snprintf(buf, sizeof(buf), "  Step: %-6.1f %-3s",
             (double)ti->step_small, ti->unit_str);
    drv_lcd2004_write_line(2, buf);

    drv_lcd2004_write_line(3, "L=Cancel    OK=Save");
}

void screen_threshold_update(void)
{
    if (s_mode == THRESH_MODE_BROWSE) {
        render_browse();
    } else {
        render_edit();
    }
}

/** Handle button events in browse mode. */
static bool handle_browse(const button_event_t *e)
{
    if (e->event != BTN_EVENT_PRESSED) { return false; }

    switch (e->button) {
        case BTN_RIGHT:
            s_type_index = (s_type_index + 1) % g_threshold_type_count;
            s_field_index = 0;
            load_current_threshold();
            drv_buzzer_play(BUZZER_NAV);
            return true;

        case BTN_UP:
            s_field_index = 0;
            return true;

        case BTN_DOWN:
            s_field_index = 1;
            return true;

        case BTN_OK:
            s_mode = THRESH_MODE_EDIT;
            s_original_value = *get_active_field();
            drv_buzzer_play(BUZZER_CLICK);
            return true;

        case BTN_LEFT:
            return false;  /* Pop back. */

        default:
            return false;
    }
}

/** Handle button events in edit mode. */
static bool handle_edit(const button_event_t *e)
{
    const threshold_type_info_t *ti = &g_threshold_types[s_type_index];
    float *val = get_active_field();

    if (e->button == BTN_UP) {
        float step = (e->event == BTN_EVENT_REPEAT)
                     ? ti->step_large : ti->step_small;
        if (e->event == BTN_EVENT_PRESSED ||
            e->event == BTN_EVENT_REPEAT) {
            *val += step;
            drv_buzzer_play(BUZZER_CLICK);
            return true;
        }
    }

    if (e->button == BTN_DOWN) {
        float step = (e->event == BTN_EVENT_REPEAT)
                     ? ti->step_large : ti->step_small;
        if (e->event == BTN_EVENT_PRESSED ||
            e->event == BTN_EVENT_REPEAT) {
            *val -= step;
            drv_buzzer_play(BUZZER_CLICK);
            return true;
        }
    }

    if (e->event != BTN_EVENT_PRESSED) { return false; }

    if (e->button == BTN_OK) {
        config_manager_set_threshold(ti->type, &s_thresh);
        config_manager_save();
        s_mode = THRESH_MODE_BROWSE;
        drv_buzzer_play(BUZZER_CONFIRM);
        return true;
    }

    if (e->button == BTN_LEFT) {
        *val = s_original_value;
        s_mode = THRESH_MODE_BROWSE;
        drv_buzzer_play(BUZZER_ERROR);
        return true;
    }

    return false;
}

bool screen_threshold_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (s_mode == THRESH_MODE_BROWSE) {
        return handle_browse(e);
    }
    return handle_edit(e);
}

void screen_threshold_exit(void)
{
    /* If user exits mid-edit, discard changes. */
    s_mode = THRESH_MODE_BROWSE;
}