/**
 * @file    screen_voltage.c
 * @brief   3-phase voltage with min/max/avg tracking and detail toggle.
 * @version 2.0.0
 * @date    2025-01-01
 */

#include "hmi/screens/screens.h"
#include "hmi/hmi_manager.h"
#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/measurement/measurement.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include <float.h>

typedef enum {
    VIEW_3PHASE = 0,
    VIEW_DETAIL_L1,
    VIEW_DETAIL_L2,
    VIEW_DETAIL_L3,
    VIEW_MODE_COUNT
} volt_view_t;

typedef struct {
    float min_val;
    float max_val;
    float sum;
    uint32_t count;
} phase_stats_t;

static volt_view_t   s_view = VIEW_3PHASE;
static phase_stats_t s_stats[3];
static float         s_prev_avg = 0.0f;

static void stats_reset(void)
{
    for (int i = 0; i < 3; i++) {
        s_stats[i].min_val = FLT_MAX;
        s_stats[i].max_val = -FLT_MAX;
        s_stats[i].sum     = 0.0f;
        s_stats[i].count   = 0;
    }
    s_prev_avg = 0.0f;
}

static void stats_update(const three_phase_reading_t *v)
{
    const sensor_reading_t *phases[3] = { &v->L1, &v->L2, &v->L3 };

    for (int i = 0; i < 3; i++) {
        float val = phases[i]->scaled_value;
        if (val < s_stats[i].min_val) { s_stats[i].min_val = val; }
        if (val > s_stats[i].max_val) { s_stats[i].max_val = val; }
        s_stats[i].sum += val;
        s_stats[i].count++;
    }
}

static float stats_avg(int phase)
{
    if (s_stats[phase].count == 0) { return 0.0f; }
    return s_stats[phase].sum / (float)s_stats[phase].count;
}

static char get_trend_char(float current, float previous)
{
    float diff = current - previous;
    if (diff > 2.0f)       { return '^'; }
    if (diff > 0.5f)       { return '/'; }
    if (diff < -2.0f)      { return 'v'; }
    if (diff < -0.5f)      { return '\\'; }
    return '-';
}

static void render_3phase(const three_phase_reading_t *v)
{
    char line[LCD_COLS + 1];

    int va = (int)v->L1.scaled_value;
    int vb = (int)v->L2.scaled_value;
    int vc = (int)v->L3.scaled_value;

    snprintf(line, sizeof(line), "VOLTAGE 3PH   ~  %s",
             v->all_valid ? "OK" : "!!");
    drv_lcd2004_write_line(0, line);

    snprintf(line, sizeof(line), "A:%3dV B:%3dV C:%3dV", va, vb, vc);
    drv_lcd2004_write_line(1, line);

    float g_min = s_stats[0].min_val;
    float g_max = s_stats[0].max_val;
    float g_avg = 0.0f;
    for (int i = 0; i < 3; i++) {
        if (s_stats[i].min_val < g_min) { g_min = s_stats[i].min_val; }
        if (s_stats[i].max_val > g_max) { g_max = s_stats[i].max_val; }
        g_avg += stats_avg(i);
    }
    g_avg /= 3.0f;

    snprintf(line, sizeof(line), "Lo:%3d Hi:%3d Av:%3d",
             (int)g_min, (int)g_max, (int)g_avg);
    drv_lcd2004_write_line(2, line);

    drv_lcd2004_write_line(3, "<HOME  OK:RST CURR>");
}

static void render_detail(const three_phase_reading_t *v, int phase)
{
    const sensor_reading_t *readings[3] = { &v->L1, &v->L2, &v->L3 };
    const char phase_names[] = "ABC";
    char line[LCD_COLS + 1];

    float val = readings[phase]->scaled_value;
    float avg_now = (v->L1.scaled_value + v->L2.scaled_value +
                     v->L3.scaled_value) / 3.0f;
    char trend = get_trend_char(avg_now, s_prev_avg);

    snprintf(line, sizeof(line), "VOLT PH-%c     ~  %s",
             phase_names[phase],
             readings[phase]->is_valid ? "OK" : "!!");
    drv_lcd2004_write_line(0, line);

    snprintf(line, sizeof(line), "NOW: %5.1fV     %c%c%c",
             (double)val, trend, trend, trend);
    drv_lcd2004_write_line(1, line);

    snprintf(line, sizeof(line), "Min:%5.1f Max:%5.1f",
             (double)s_stats[phase].min_val,
             (double)s_stats[phase].max_val);
    drv_lcd2004_write_line(2, line);

    drv_lcd2004_write_line(3, "<PREV  ^:3PH  NEXT>");

    s_prev_avg = avg_now;
}

void screen_voltage_enter(void)
{
    s_view = VIEW_3PHASE;
    stats_reset();
    drv_lcd2004_clear();
}

void screen_voltage_update(void)
{
    three_phase_reading_t v;
    measurement_get_voltage(&v);
    stats_update(&v);

    if (s_view == VIEW_3PHASE) {
        render_3phase(&v);
    } else {
        int phase = (int)s_view - 1;
        render_detail(&v, phase);
    }
}

bool screen_voltage_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED && e->event != BTN_EVENT_REPEAT) {
        return false;
    }

    if (e->button == BTN_UP) {
        if (s_view == VIEW_3PHASE) {
            s_view = VIEW_DETAIL_L1;
        } else {
            s_view = VIEW_3PHASE;
        }
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    if (e->button == BTN_DOWN && s_view != VIEW_3PHASE) {
        s_view = (volt_view_t)(((int)s_view) + 1);
        if (s_view >= VIEW_MODE_COUNT) { s_view = VIEW_DETAIL_L1; }
        drv_lcd2004_clear();
        drv_buzzer_play(BUZZER_NAV);
        return true;
    }

    if (e->button == BTN_OK && e->event == BTN_EVENT_PRESSED) {
        stats_reset();
        drv_buzzer_play(BUZZER_CONFIRM);
        return true;
    }

    return false;
}

void screen_voltage_exit(void)
{
}