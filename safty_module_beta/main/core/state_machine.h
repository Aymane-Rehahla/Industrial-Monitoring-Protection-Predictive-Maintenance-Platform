/**
 * @file  state_machine.h
 * @brief System state machine with validated transitions.
 * @version 1.0.0
 *
 * @safety CRITICAL
 *
 * Rule 0.2: All state changes logged.
 * Rule 5.10: Safe mode cannot be exited without physical reset.
 *
 * States:
 *   BOOT → INIT → SELF_TEST → READY
 *   READY ↔ RUNNING ↔ WARNING
 *   READY → CALIBRATION → READY
 *   READY/RUNNING/WARNING → SLEEP → READY
 *   RUNNING/WARNING → FAULT → READY (with acknowledgement)
 *   Any → FAULT → SAFE_MODE (catastrophic)
 *   SAFE_MODE → (physical reset only)
 */
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "system_types.h"

/* ── Transition record for history ───────────────────────────────────── */
#define SM_HISTORY_SIZE  16

typedef struct {
    system_state_t from;
    system_state_t to;
    uint32_t       timestamp_ms;
    const char    *reason;         /* string literal — not freed */
} sm_transition_record_t;

/**
 * @brief  Initialize state machine to STATE_BOOT.
 * @return ERR_OK
 * @wcet   1 ms | thread-safe NO | isr-safe NO
 */
error_code_t sm_init(void);

/**
 * @brief  Request a state transition.  Validates legality, logs, runs
 *         entry/exit actions.
 * @param  new_state  Target state
 * @param  reason     Human-readable reason (string literal)
 * @return ERR_OK or ERR_INVALID_PARAMETER if transition is illegal
 * @wcet   5 ms | thread-safe YES (mutex) | isr-safe NO
 */
error_code_t sm_request_transition(system_state_t new_state,
                                    const char *reason);

/**
 * @brief  Force transition to SAFE_MODE (bypasses validation).
 *         Rule 8.4: direct register writes to open relay.
 * @param  reason  Why
 * @return ERR_OK (always succeeds)
 * @wcet   <100 µs | thread-safe YES | isr-safe YES (uses emergency_safe)
 */
error_code_t sm_force_safe_mode(const char *reason);

/**
 * @brief  Get current state (atomic read).
 */
system_state_t sm_get_state(void);

/**
 * @brief  Time spent in the current state (ms).
 */
uint32_t sm_get_state_duration_ms(void);

/**
 * @brief  System is in an operational state (READY/RUNNING/WARNING/CALIBRATION).
 */
bool sm_is_operational(void);

/**
 * @brief  System is in a fault state (FAULT or SAFE_MODE).
 */
bool sm_is_faulted(void);

/**
 * @brief  Check if a transition to target is valid from current state.
 */
bool sm_can_transition_to(system_state_t target);

/**
 * @brief  State enum → human-readable string.
 */
const char *sm_state_to_string(system_state_t state);

/**
 * @brief  Copy transition history out.
 * @param  out   Destination array
 * @param  max   Size of out array
 * @param  count Actual number written
 * @return ERR_OK or ERR_NULL_POINTER
 */
error_code_t sm_get_history(sm_transition_record_t *out,
                             size_t max, size_t *count);

/**
 * @brief  Total uptime in seconds since boot.
 */
uint32_t sm_get_uptime_seconds(void);

#endif /* STATE_MACHINE_H */