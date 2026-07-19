// ═══ FILE: main/core/system_status.h ═══
/**
 * @file    system_status.h
 * @brief   Central repository for system-wide state information.
 *          Single source of truth for state, role, uptime, relay, errors.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Protection engine reads state to decide relay action.
 */

#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h> 

/**
 * @brief  Complete snapshot of system state at a point in time.
 */
typedef struct {
    uint32_t       magic;               /* STATUS_MAGIC (0xDEAD5AFE)       */
    system_state_t state;               /* Current state machine state     */
    device_role_t  role;                /* INFORMER or SILENT              */
    uint32_t       uptime_seconds;      /* Seconds since boot              */
    uint32_t       boot_count;          /* Total boots from NVS            */
    bool           relay_commanded;     /* Software wants relay energised  */
    bool           relay_confirmed;     /* Hardware feedback: relay is on  */
    uint32_t       error_count;         /* Cumulative errors               */
    uint32_t       warning_count;       /* Cumulative warnings             */
    uint32_t       free_heap_bytes;     /* Current free heap               */
    uint32_t       min_free_heap_bytes; /* Minimum ever observed           */
} system_snapshot_t;

/** @brief Initialise system status module. */
error_code_t system_status_init(void);

/** @brief Get atomic snapshot of all system state. */
error_code_t system_status_get_snapshot(system_snapshot_t *out);

/** @brief Get current system state. */
system_state_t system_status_get_state(void);

/** @brief Get device role. */
device_role_t system_status_get_role(void);

/** @brief Get uptime in seconds. */
uint32_t system_status_get_uptime_seconds(void);

/** @brief Check if relay is commanded on. */
bool system_status_is_relay_on(void);

/** @brief Get current security mode. */
security_mode_t system_status_get_security_mode(void);

/** @brief Set system state. */
error_code_t system_status_set_state(system_state_t new_state);

/** @brief Set device role. */
error_code_t system_status_set_role(device_role_t role);

/** @brief Set relay commanded state. */
error_code_t system_status_set_relay(bool commanded);

/** @brief Set security mode. */
error_code_t system_status_set_security_mode(security_mode_t mode);

/** @brief Increment error counter. */
error_code_t system_status_increment_errors(void);

/** @brief Increment warning counter. */
error_code_t system_status_increment_warnings(void);

#endif /* SYSTEM_STATUS_H */