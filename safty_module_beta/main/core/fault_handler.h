/**
 * @file  fault_handler.h
 * @brief Fault logging, severity escalation, acknowledgement.
 * @version 1.0.0
 *
 * @safety CRITICAL
 *
 * Rule 4.1: Fault entries have magic numbers.
 * Rule 4.2: Fault entries have CRC-16 checksums.
 * Rule 4.9: Ring buffer — never loses critical events.
 * Rule 5.1: Four severity levels.
 * Rule 5.6: Repetitive warnings escalate to critical.
 */
#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "system_types.h"

/**
 * @brief  Initialize fault handler and clear log.
 * @return ERR_OK
 * @wcet   1 ms | thread-safe NO | isr-safe NO
 */
error_code_t fault_init(void);

/**
 * @brief  Log a new fault event.
 * @param  code       Error code (e.g. ERR_OVERCURRENT)
 * @param  severity   SEVERITY_INFO .. CATASTROPHIC
 * @param  value      Sensor value that caused the fault
 * @param  threshold  Threshold that was exceeded
 * @return ERR_OK
 * @wcet   <1 ms | thread-safe YES (mutex) | isr-safe NO
 *
 * Rule 5.5: Increments per-code counters.
 * Rule 5.6: Auto-escalates repeated warnings.
 */
error_code_t fault_log(error_code_t code, severity_t severity,
                        float value, float threshold);

/**
 * @brief  Get total number of faults ever logged.
 */
uint32_t fault_get_total_count(void);

/**
 * @brief  Get number of unacknowledged faults.
 */
uint8_t fault_get_active_count(void);

/**
 * @brief  Get a specific entry from the ring buffer.
 * @param  index  0 = oldest still in buffer
 * @param  out    Destination (must not be NULL)
 * @return ERR_OK, ERR_NULL_POINTER, ERR_INVALID_PARAMETER
 */
error_code_t fault_get_entry(uint8_t index, fault_entry_t *out);

/**
 * @brief  Get the most recent fault.
 * @param  out  Destination (NULL = just check if any exist)
 * @return ERR_OK or ERR_INVALID_PARAMETER (no faults)
 */
error_code_t fault_get_latest(fault_entry_t *out);

/**
 * @brief  Acknowledge (mark as cleared) the most recent fault.
 * @return ERR_OK or ERR_INVALID_PARAMETER
 */
error_code_t fault_acknowledge_latest(void);

/**
 * @brief  Acknowledge all faults.
 */
error_code_t fault_acknowledge_all(void);

/**
 * @brief  Clear the entire fault log.
 */
error_code_t fault_clear_all(void);

/**
 * @brief  Map an error code to its default severity.
 * @param  code  Error code
 * @return Severity level
 */
severity_t fault_default_severity(error_code_t code);

/**
 * @brief  Get the number of times a specific error has occurred.
 */
uint32_t fault_get_code_count(error_code_t code);

/**
 * @brief  Get the fault log handle (for NVS save/load).
 */
const fault_log_t *fault_get_log(void);

#endif /* FAULT_HANDLER_H */