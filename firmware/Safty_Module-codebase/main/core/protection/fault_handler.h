/**
 * @file    fault_handler.h
 * @brief   Fault recording, storage, and management.
 *          Maintains ring buffer of recent faults for display and NVS.
 * @version 2.0.0
 * @date    2025-01-01
 * @safety  HIGH — Fault history aids post-incident investigation.
 *
 * CHANGELOG:
 *   2.0.0  2025-01-01  Added fault_handler_inject() for real fault logging.
 *                      fault_to_str() and severity_to_str() live in
 *                      screens.h (not duplicated here).
 *   1.0.0  2025-01-01  Initial release — API definition.
 */

#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/** Maximum entries in the fault log ring buffer. */
#define FAULT_LOG_MAX_ENTRIES  32

/**
 * @brief  Single fault log entry with integrity fields.
 */
typedef struct {
    uint16_t     magic;            /* MAGIC_FAULT_LOG (0xFA17)           */
    uint32_t     timestamp_ms;     /* When fault occurred                */
    fault_code_t code;             /* Which fault                        */
    severity_t   severity;         /* How bad                            */
    float        measured_value;   /* What was measured                  */
    float        threshold_value;  /* What the limit was                 */
    bool         is_forgivable;    /* Can auto-clear after condition ends*/
    bool         is_acknowledged;  /* Operator pressed OK                */
    uint16_t     checksum;         /* CRC-16 of preceding fields         */
} fault_entry_t;

/** @brief Initialise fault handler. */
error_code_t fault_handler_init(void);

/**
 * @brief  Inject a fault into the log.
 *
 * Used by protection module when threshold exceeded, and by
 * test button for demo purposes.
 *
 * @param  code        Which fault type.
 * @param  severity    How critical.
 * @param  measured    The value that triggered the fault.
 * @param  threshold   The limit that was exceeded.
 * @param  forgivable  Can this fault auto-clear?
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 */
error_code_t fault_handler_inject(fault_code_t code,
                                  severity_t severity,
                                  float measured,
                                  float threshold,
                                  bool forgivable);

/** @brief Get number of active faults in the log. */
error_code_t fault_handler_get_count(uint32_t *count_out);

/** @brief Get fault entry by index (0 = oldest). */
error_code_t fault_handler_get_entry(uint32_t index,
                                     fault_entry_t *entry_out);

/** @brief Get most recent fault entry. */
error_code_t fault_handler_get_latest(fault_entry_t *entry_out);

/** @brief Acknowledge all unacknowledged faults. */
error_code_t fault_handler_acknowledge_all(void);

/** @brief Clear faults marked as forgivable. */
error_code_t fault_handler_clear_forgivable(void);

/** @brief Check if any faults are unacknowledged. */
bool fault_handler_has_unacknowledged(void);

/** @brief Get total historical fault count (including cleared). */
uint32_t fault_handler_get_total_count(void);

/*
 * NOTE: fault_to_str() and severity_to_str() are defined in
 * hmi/screens/screens.h — do NOT duplicate them here.
 * Include screens.h in any .c file that needs them.
 */

#endif /* FAULT_HANDLER_H */