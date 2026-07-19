/**
 * @file    fault_handler.c
 * @brief   Fault handler with ring buffer storage.
 *          Supports injecting test faults for demo/debug.
 * @version 2.0.0
 * @date    2025-01-01
 * @safety  HIGH — Real fault logging for production use.
 *
 * CHANGELOG:
 *   2.0.0  2025-01-01  Replaced stub with real ring buffer implementation.
 *                      Added fault_handler_inject() for test/demo.
 *   1.0.0  2025-01-01  STUB release.
 */

#include "core/protection/fault_handler.h"
#include "system_types.h"
#include "app_config.h"
#include "core/system_status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "fault_handler";

/* ── Ring buffer storage ─────────────────────────────────────────────── */
static fault_entry_t s_log[FAULT_LOG_MAX_ENTRIES];
static uint32_t      s_count       = 0;   /* Active entries              */
static uint32_t      s_total_count = 0;   /* Lifetime total              */
static bool          s_initialized = false;

/* ── Helper: current time in ms ──────────────────────────────────────── */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_init(void)
{
    memset(s_log, 0, sizeof(s_log));
    s_count       = 0;
    s_total_count = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "Fault handler initialised (capacity=%d)",
             FAULT_LOG_MAX_ENTRIES);
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_inject — Add a fault to the log.
 *
 *  Used by protection module in production, and by test button
 *  during demo.  Appends to ring buffer, overwrites oldest if full.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_inject(fault_code_t code,
                                  severity_t severity,
                                  float measured,
                                  float threshold,
                                  bool forgivable)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    uint32_t idx;

    if (s_count < FAULT_LOG_MAX_ENTRIES) {
        idx = s_count;
        s_count++;
    } else {
        /* Ring buffer full — overwrite oldest (shift left by 1). */
        for (uint32_t i = 0; i < FAULT_LOG_MAX_ENTRIES - 1; i++) {
            s_log[i] = s_log[i + 1];
        }
        idx = FAULT_LOG_MAX_ENTRIES - 1;
    }

    fault_entry_t *e = &s_log[idx];
    e->magic           = 0xFA17;
    e->timestamp_ms    = get_tick_ms();
    e->code            = code;
    e->severity        = severity;
    e->measured_value  = measured;
    e->threshold_value = threshold;
    e->is_forgivable   = forgivable;
    e->is_acknowledged = false;
    e->checksum        = 0; /* TODO: CRC-16 */

    s_total_count++;

    ESP_LOGW(TAG, "FAULT INJECTED: code=%d sev=%d val=%.1f thresh=%.1f",
             code, severity, measured, threshold);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_get_count
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_get_count(uint32_t *count_out)
{
    if (count_out == NULL) { return ERR_NULL_POINTER; }
    *count_out = s_count;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_get_entry
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_get_entry(uint32_t index,
                                     fault_entry_t *entry_out)
{
    if (entry_out == NULL) { return ERR_NULL_POINTER; }
    if (index >= s_count)  { return ERR_NOT_FOUND; }

    *entry_out = s_log[index];
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_get_latest
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_get_latest(fault_entry_t *entry_out)
{
    if (entry_out == NULL) { return ERR_NULL_POINTER; }
    if (s_count == 0)      { return ERR_NOT_FOUND; }

    *entry_out = s_log[s_count - 1];
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_acknowledge_all
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_acknowledge_all(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    for (uint32_t i = 0; i < s_count; i++) {
        s_log[i].is_acknowledged = true;
    }

    /* Clear all acknowledged faults. */
    s_count = 0;

    /*
     * FIX: Return system to RUN state so hmi_manager stops
     * forcing the fault screen.  Without this, check_fault_forcing()
     * keeps pushing SCREEN_FAULT even though faults are cleared.
     */
    system_status_set_state(SYS_STATE_RUN);

    ESP_LOGI(TAG, "All faults acknowledged and cleared — state -> RUN");
    return ERR_OK;
}
/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_clear_forgivable
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t fault_handler_clear_forgivable(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    /* Compact: keep only non-forgivable faults. */
    uint32_t write = 0;
    for (uint32_t read = 0; read < s_count; read++) {
        if (!s_log[read].is_forgivable) {
            if (write != read) {
                s_log[write] = s_log[read];
            }
            write++;
        }
    }
    s_count = write;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_has_unacknowledged
 * ═══════════════════════════════════════════════════════════════════════ */
bool fault_handler_has_unacknowledged(void)
{
    for (uint32_t i = 0; i < s_count; i++) {
        if (!s_log[i].is_acknowledged) {
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  fault_handler_get_total_count
 * ═══════════════════════════════════════════════════════════════════════ */
uint32_t fault_handler_get_total_count(void)
{
    return s_total_count;
}