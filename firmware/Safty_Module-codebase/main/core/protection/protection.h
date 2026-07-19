// ═══ FILE: main/core/protection/protection.h ═══
/**
 * @file    protection.h
 * @brief   Protection engine — evaluates sensor readings against thresholds.
 *          Determines alarm status per sensor type. Runs on Core 1.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  CRITICAL — Decides when to trip the relay.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release — API definition.
 */

#ifndef PROTECTION_H
#define PROTECTION_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief Initialise protection engine. */
error_code_t protection_init(void);

/** @brief Get alarm status for a specific sensor type. */
error_code_t protection_get_alarm(sensor_type_t type,
                                  sensor_alarm_t *alarm_out);

/** @brief Check if any sensor has tripped (relay should open). */
bool protection_is_tripped(void);

/** @brief Get count of currently active faults. */
uint32_t protection_get_active_fault_count(void);

/** @brief Acknowledge all active faults (operator action). */
error_code_t protection_acknowledge_faults(void);

#endif /* PROTECTION_H */