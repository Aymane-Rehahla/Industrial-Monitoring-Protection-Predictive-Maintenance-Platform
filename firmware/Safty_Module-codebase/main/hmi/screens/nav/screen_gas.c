/**
 * @file    screen_gas.c
 * @brief   Multi-sensor gas display with warmup countdown.
 * @version 2.0.0
 * @date    2025-01-01
 *
 * CHANGELOG:
 *   2.0.0  Replaced stub with multi-sensor warmup-aware display.
 */

#include "hmi/screens/screens.h"
#include "drivers/interface/drv_lcd2004.h"
#include "core/measurement/measurement.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  Warmup time helper
 * ═══════════════════════════════════════════════════════════════════════ */

static bool is_warmed_up(void)
{
    uint32_t up_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    return (up_ms >= MQ_WARMUP_MS);
}

static void get_warmup_remaining(uint32_t *min, uint32_t *sec)
{
    uint32_t up_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (up_ms >= MQ_WARMUP_MS) {
        *min = 0; *sec = 0;
        return;
    }
    uint32_t remain_s = (MQ_WARMUP_MS - up_ms) / 1000U;
    *min = remain_s / 60U;
    *sec = remain_s % 60U;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Render: Warming up
 * ═══════════════════════════════════════════════════════════════════════ */

static void render_warmup(void)
{
    uint32_t m, s;
    get_warmup_remaining(&m, &s);

    char line[LCD_COLS + 1];

    drv_lcd2004_write_line(0, "GAS SENSORS WARMING ");

    drv_lcd2004_write_line(1, "MQ2:WAIT MQ4:WAIT   ");

    snprintf(line, sizeof(line), "MQ9:WAIT  Time:%02lu:%02lu",
             (unsigned long)m, (unsigned long)s);
    drv_lcd2004_write_line(2, line);

    drv_lcd2004_write_line(3, "<TEMP  \x01MENU   VIB>");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Render: Normal (post-warmup)
 * ═══════════════════════════════════════════════════════════════════════ */

static void render_normal(void)
{
    sensor_reading_t smoke, methane, co;
    measurement_get_gas(SENSOR_GAS_SMOKE,   &smoke);
    measurement_get_gas(SENSOR_GAS_METHANE, &methane);
    measurement_get_gas(SENSOR_GAS_CO,      &co);

    /* Determine overall status */
    bool any_high = (smoke.scaled_value > 400.0f) ||
                    (methane.scaled_value > 400.0f) ||
                    (co.scaled_value > 100.0f);
    char line[LCD_COLS + 1];

    /* Row 0 */
    if (any_high) {
        snprintf(line, sizeof(line), "!! GAS WARNING !! ");
    } else {
        snprintf(line, sizeof(line), "GAS DETECT     ~ OK");
    }
    drv_lcd2004_write_line(0, line);

    /* Row 1: MQ2 + MQ4 */
    char mq2_flag = (smoke.scaled_value > 400.0f) ? '!' : ' ';
    snprintf(line, sizeof(line), "MQ2:%3.0fp%c MQ4:%3.0fp ",
             (double)smoke.scaled_value, mq2_flag,
             (double)methane.scaled_value);
    drv_lcd2004_write_line(1, line);

    /* Row 2: MQ9 + overall */
    const char *overall = any_high ? "WARNING!" : "ALL NORM";
    snprintf(line, sizeof(line), "MQ9:%3.0fp  %s",
             (double)co.scaled_value, overall);
    drv_lcd2004_write_line(2, line);

    /* Row 3 */
    if (any_high) {
        drv_lcd2004_write_line(3, "<TEMP  OK:ACK  VIB>");
    } else {
        drv_lcd2004_write_line(3, "<TEMP  \x01MENU   VIB>");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Screen lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

void screen_gas_enter(void)
{
    drv_lcd2004_clear();
}

void screen_gas_update(void)
{
    if (!is_warmed_up()) {
        render_warmup();
    } else {
        render_normal();
    }
}

bool screen_gas_handle_event(const button_event_t *e)
{
    UNUSED(e);
    return false;
}

void screen_gas_exit(void) { }