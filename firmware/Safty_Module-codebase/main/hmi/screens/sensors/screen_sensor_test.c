// ═══ FILE: main/hmi/screens/sensors/screen_sensor_test.c ═══
/**
 * @file    screen_sensor_test.c
 * @brief   Live sensor reading test.
 *          Step 1: Select type.  Step 2: Live reading display.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — read-only display.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "hmi/menu_engine.h"
#include "core/measurement/measurement.h"
#include "core/sensor_manager/sensor_manager.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_sensor_test";

typedef enum {
    TEST_STEP_SELECT = 0,
    TEST_STEP_LIVE   = 1
} test_step_t;

/* Justification: Test state persists across calls.  File scope. */
static test_step_t   s_step;
static menu_state_t  s_menu;
static menu_item_t   s_items[SENSOR_TYPE_COUNT];
static sensor_type_t s_test_type;

/* Types available for testing (SENSOR_VOLTAGE through SENSOR_AUDIO). */
#define TESTABLE_START  SENSOR_VOLTAGE
#define TESTABLE_END    SENSOR_AUDIO
#define TESTABLE_COUNT  (TESTABLE_END - TESTABLE_START + 1)

static void on_type_selected(uint32_t index);

/** Build the sensor type selection menu. */
static void build_type_menu(void)
{
    uint32_t count = 0;

    for (int t = TESTABLE_START; t <= TESTABLE_END && count < SENSOR_TYPE_COUNT; t++) {
        snprintf(s_items[count].label, MENU_LABEL_MAX_LEN, "%s",
                 sensor_manager_get_type_name((sensor_type_t)t));
        s_items[count].target_screen = SCREEN_COUNT;
        s_items[count].action        = on_type_selected;
        count++;
    }

    menu_engine_init(&s_menu, "TEST: Select Type", s_items, count);
}

static void on_type_selected(uint32_t index)
{
    s_test_type = (sensor_type_t)(TESTABLE_START + (int)index);
    s_step = TEST_STEP_LIVE;
}

/**
 * @brief  Fetch a reading for the selected sensor type.
 *
 * WHY unified helper: Avoids a long switch/case in update().
 * For 3-phase types we show L1 only — the full 3-phase view
 * is on the dedicated voltage/current screens.
 */
static void fetch_reading(sensor_reading_t *out)
{
    three_phase_reading_t tp;

    switch (s_test_type) {
        case SENSOR_VOLTAGE:
            measurement_get_voltage(&tp); *out = tp.L1; break;
        case SENSOR_CURRENT:
            measurement_get_current(&tp); *out = tp.L1; break;
        case SENSOR_TEMP:
            measurement_get_temperature(out); break;
        case SENSOR_HUMIDITY:
            measurement_get_humidity(out); break;
        case SENSOR_GAS_SMOKE:
            measurement_get_gas(SENSOR_GAS_SMOKE, out); break;
        case SENSOR_GAS_METHANE:
            measurement_get_gas(SENSOR_GAS_METHANE, out); break;
        case SENSOR_GAS_CO:
            measurement_get_gas(SENSOR_GAS_CO, out); break;
        case SENSOR_VIBRATION: {
            sensor_reading_t y;
            measurement_get_vibration(out, &y);
            break;
        }
        case SENSOR_RPM:
            measurement_get_rpm(out); break;
        case SENSOR_AUDIO:
            measurement_get_audio(out); break;
        default:
            memset(out, 0, sizeof(*out)); break;
    }
}

void screen_sensor_test_enter(void)
{
    ESP_LOGI(TAG, "enter");
    s_step = TEST_STEP_SELECT;
    drv_lcd2004_clear();
    build_type_menu();
}

void screen_sensor_test_update(void)
{
    if (s_step == TEST_STEP_SELECT) {
        menu_engine_render(&s_menu);
        return;
    }

    /* Live reading display. */
    sensor_reading_t r;
    fetch_reading(&r);
    char buf[LCD_COLS + 1];

    snprintf(buf, sizeof(buf), "TEST: %-13s",
             sensor_manager_get_type_name(s_test_type));
    drv_lcd2004_write_line(0, buf);

    snprintf(buf, sizeof(buf), "Val: %8.2f",
             (double)r.scaled_value);
    drv_lcd2004_write_line(1, buf);

    snprintf(buf, sizeof(buf), "Raw: %-6ld  Q:%3u",
             (long)r.raw_value, (unsigned)r.quality);
    drv_lcd2004_write_line(2, buf);

    drv_lcd2004_write_line(3, "< Back            ");
}

bool screen_sensor_test_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }

    if (s_step == TEST_STEP_LIVE) {
        if (e->event == BTN_EVENT_PRESSED && e->button == BTN_LEFT) {
            s_step = TEST_STEP_SELECT;
            build_type_menu();
            return true;
        }
        return false;
    }

    if (e->event == BTN_EVENT_PRESSED && e->button == BTN_LEFT) {
        return false;  /* Pop screen. */
    }

    return menu_engine_handle_event(&s_menu, e);
}

void screen_sensor_test_exit(void)
{
    /* Nothing to clean up. */
}