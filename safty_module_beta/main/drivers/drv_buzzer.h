/**
 * @file  drv_buzzer.h
 * @brief Buzzer driver with named beep patterns.
 * @version 1.0.0
 *
 * @safety LOW
 * @hardware Active buzzer on GPIO13 (HIGH = sound).
 *
 * Rule 14.2: Critical alarms have distinct audio patterns.
 *
 * Non-blocking: call buzzer_update() periodically to drive patterns.
 */
#ifndef DRV_BUZZER_H
#define DRV_BUZZER_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/**
 * @brief  Initialize buzzer.
 * @return ERR_OK
 * @wcet   1 ms | thread-safe NO | isr-safe NO
 */
error_code_t buzzer_init(void);

/**
 * @brief  Start a beep pattern (non-blocking).
 *         Higher priority patterns interrupt lower ones.
 * @param  pattern  BEEP_CLICK, BEEP_CONFIRM, BEEP_ERROR, etc.
 * @return ERR_OK, ERR_INVALID_PARAMETER
 */
error_code_t buzzer_play(beep_pattern_t pattern);

/**
 * @brief  Update buzzer output.  Call every 10–20 ms.
 * @return ERR_OK
 * @wcet   <100 µs | thread-safe NO | isr-safe NO
 */
error_code_t buzzer_update(void);

/**
 * @brief  Immediately silence.
 * @return ERR_OK
 */
error_code_t buzzer_stop(void);

/**
 * @brief  Is a pattern currently playing?
 */
bool buzzer_is_playing(void);

/**
 * @brief  Enable / disable buzzer globally (mute).
 */
void buzzer_set_enabled(bool enabled);

/**
 * @brief  Is the buzzer globally enabled?
 */
bool buzzer_is_enabled(void);

#endif /* DRV_BUZZER_H */