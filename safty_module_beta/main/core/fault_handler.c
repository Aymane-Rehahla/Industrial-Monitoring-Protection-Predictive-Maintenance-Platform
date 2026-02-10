/**
 * @file  fault_handler.c
 * @brief Fault handler implementation.
 * @version 1.0.0
 *
 * @safety CRITICAL
 *
 * Rule 4.1: Magic number MAGIC_FAULT_LOG on every entry.
 * Rule 4.2: CRC-16 on every entry.
 * Rule 5.5: Per-code occurrence counters.
 * Rule 5.6: 3+ warnings of same code → escalate to CRITICAL.
 * Rule 5.11: Log errors BEFORE attempting recovery.
 */
#include "fault_handler.h"
#include "state_machine.h"
#include "crc_utils.h"
#include "app_config.h"
#include "time_utils.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "FAULT";

/* ── Escalation threshold (Rule 5.6) ─────────────────────────────────── */
#define WARNING_ESCALATION_COUNT  3

/* ── Per-code occurrence counters ────────────────────────────────────── */
#define MAX_ERROR_CODE  0xA0   /* covers all defined codes */
static uint16_t s_code_counts[MAX_ERROR_CODE];

/* ── Module state ────────────────────────────────────────────────────── */
static fault_log_t       s_log;
static SemaphoreHandle_t s_mtx        = NULL;
static bool              s_initialized = false;

/* ── Compute and set checksum on an entry ────────────────────────────── */

static void stamp_entry(fault_entry_t *entry)
{
    entry->magic    = MAGIC_FAULT_LOG;
    entry->checksum = 0;
    entry->checksum = crc16_struct(entry, sizeof(fault_entry_t));
}

/* ── Verify checksum on an entry ─────────────────────────────────────── */

static bool verify_entry(const fault_entry_t *entry)
{
    if (entry->magic != MAGIC_FAULT_LOG) { return false; }

    fault_entry_t tmp;
    memcpy(&tmp, entry, sizeof(tmp));
    uint16_t stored = tmp.checksum;
    tmp.checksum = 0;
    uint16_t calc = crc16_struct(&tmp, sizeof(tmp));
    return (stored == calc);
}

/* ── Get pointer to entry by ring-buffer index ───────────────────────── */

static fault_entry_t *entry_at(uint8_t ring_index)
{
    if (ring_index >= FAULT_LOG_SIZE) { return NULL; }
    return &s_log.entries[ring_index];
}

/* ── Public: Init ────────────────────────────────────────────────────── */

error_code_t fault_init(void)
{
    s_mtx = xSemaphoreCreateMutex();
    if (s_mtx == NULL) { return ERR_MEMORY_CORRUPT; }

    memset(&s_log, 0, sizeof(s_log));
    memset(s_code_counts, 0, sizeof(s_code_counts));

    s_initialized = true;
    ESP_LOGI(TAG, "Fault handler initialized (capacity=%d)", FAULT_LOG_SIZE);
    return ERR_OK;
}

/* ── Auto-escalate severity (Rule 5.6) ───────────────────────────────── */

static severity_t maybe_escalate(error_code_t code, severity_t severity)
{
    if (code >= MAX_ERROR_CODE) { return severity; }

    uint16_t count = s_code_counts[code];

    if (severity == SEVERITY_WARNING && count >= WARNING_ESCALATION_COUNT) {
        ESP_LOGW(TAG, "Escalating 0x%02X: %d warnings → CRITICAL", code, count);
        return SEVERITY_CRITICAL;
    }

    return severity;
}

/* ── Public: Log fault ───────────────────────────────────────────────── */

error_code_t fault_log(error_code_t code, severity_t severity,
                        float value, float threshold)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ERR_I2C_TIMEOUT;
    }

    /* Rule 5.5: increment counter */
    if (code < MAX_ERROR_CODE) { s_code_counts[code]++; }

    /* Rule 5.6: auto-escalate */
    severity = maybe_escalate(code, severity);

    /* Build entry */
    fault_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.timestamp_ms  = get_time_ms();
    entry.error_code    = code;
    entry.severity      = severity;
    entry.value_at_fault = value;
    entry.threshold     = threshold;
    entry.state_before  = sm_get_state();
    entry.was_cleared   = false;
    stamp_entry(&entry);

    /* Push into ring buffer */
    s_log.entries[s_log.head] = entry;
    s_log.head = (s_log.head + 1) % FAULT_LOG_SIZE;
    if (s_log.count < FAULT_LOG_SIZE) { s_log.count++; }
    s_log.total_faults++;

    /* Rule 5.11: Log BEFORE recovery */
    ESP_LOGE(TAG, "FAULT #%lu: code=0x%02X sev=%d val=%.2f thr=%.2f",
             (unsigned long)s_log.total_faults,
             code, severity, value, threshold);

    xSemaphoreGive(s_mtx);
    return ERR_OK;
}

/* ── Public: Counts ──────────────────────────────────────────────────── */

uint32_t fault_get_total_count(void)
{
    return s_log.total_faults;
}

uint8_t fault_get_active_count(void)
{
    uint8_t active = 0;
    for (uint8_t i = 0; i < s_log.count && i < FAULT_LOG_SIZE; i++) {
        if (!s_log.entries[i].was_cleared) { active++; }
    }
    return active;
}

/* ── Public: Get entry (0 = oldest in buffer) ────────────────────────── */

error_code_t fault_get_entry(uint8_t index, fault_entry_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (index >= s_log.count) { return ERR_INVALID_PARAMETER; }

    /* Map logical index to physical ring position */
    uint8_t phys;
    if (s_log.count < FAULT_LOG_SIZE) {
        phys = index;
    } else {
        phys = (s_log.head + index) % FAULT_LOG_SIZE;
    }

    fault_entry_t *src = entry_at(phys);
    if (src == NULL) { return ERR_INVALID_PARAMETER; }

    memcpy(out, src, sizeof(fault_entry_t));
    return ERR_OK;
}

/* ── Public: Get latest ──────────────────────────────────────────────── */

error_code_t fault_get_latest(fault_entry_t *out)
{
    if (s_log.count == 0) { return ERR_INVALID_PARAMETER; }

    uint8_t latest = (s_log.head == 0) ? (FAULT_LOG_SIZE - 1)
                                        : (s_log.head - 1);

    if (out != NULL) {
        memcpy(out, &s_log.entries[latest], sizeof(fault_entry_t));
    }
    return ERR_OK;
}

/* ── Public: Acknowledge latest ──────────────────────────────────────── */

error_code_t fault_acknowledge_latest(void)
{
    if (s_log.count == 0) { return ERR_INVALID_PARAMETER; }
    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ERR_I2C_TIMEOUT;
    }

    uint8_t latest = (s_log.head == 0) ? (FAULT_LOG_SIZE - 1)
                                        : (s_log.head - 1);
    s_log.entries[latest].was_cleared = true;
    stamp_entry(&s_log.entries[latest]);  /* re-stamp CRC */

    ESP_LOGI(TAG, "Fault #%d acknowledged", latest);
    xSemaphoreGive(s_mtx);
    return ERR_OK;
}

/* ── Public: Acknowledge all ─────────────────────────────────────────── */

error_code_t fault_acknowledge_all(void)
{
    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ERR_I2C_TIMEOUT;
    }

    for (uint8_t i = 0; i < s_log.count && i < FAULT_LOG_SIZE; i++) {
        s_log.entries[i].was_cleared = true;
        stamp_entry(&s_log.entries[i]);
    }

    ESP_LOGI(TAG, "All %d faults acknowledged", s_log.count);
    xSemaphoreGive(s_mtx);
    return ERR_OK;
}

/* ── Public: Clear all ───────────────────────────────────────────────── */

error_code_t fault_clear_all(void)
{
    if (xSemaphoreTake(s_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ERR_I2C_TIMEOUT;
    }

    memset(&s_log, 0, sizeof(s_log));
    memset(s_code_counts, 0, sizeof(s_code_counts));

    ESP_LOGI(TAG, "Fault log cleared");
    xSemaphoreGive(s_mtx);
    return ERR_OK;
}

/* ── Public: Default severity mapping ────────────────────────────────── */

severity_t fault_default_severity(error_code_t code)
{
    /* Rule 5.2: documented response for each domain */
    if (code >= 0x40 && code <= 0x44) { return SEVERITY_CRITICAL; }
    if (code >= 0x50 && code <= 0x53) { return SEVERITY_CATASTROPHIC; }
    if (code >= 0x30 && code <= 0x33) { return SEVERITY_WARNING; }
    if (code >= 0x80 && code <= 0x81) { return SEVERITY_INFO; }
    return SEVERITY_WARNING;
}

/* ── Public: Per-code count ──────────────────────────────────────────── */

uint32_t fault_get_code_count(error_code_t code)
{
    if (code >= MAX_ERROR_CODE) { return 0; }
    return s_code_counts[code];
}

/* ── Public: Get log handle ──────────────────────────────────────────── */

const 