// ═══ FILE: main/drivers/actuators/drv_buzzer.h ═══
/**
 * @file    drv_buzzer.h
 * @brief   Non-blocking passive buzzer driver using LEDC PWM.
 *          Plays predefined tone patterns for UI feedback and alarms.
 *          Never blocks — uses tick-based state machine for multi-step patterns.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — informational only, not safety-critical.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef DRV_BUZZER_H
#define DRV_BUZZER_H

#include "system_types.h"
#include <stdbool.h>

/**
 * @brief  Initialise the LEDC PWM for the passive buzzer.
 *
 * @pre    hal_gpio_init() has been called.
 * @post   LEDC timer and channel configured, buzzer silent (duty=0).
 * @return ERR_OK, ERR_HW_INIT_FAILED.
 * @wcet   < 2 ms
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buzzer_init(void);

/**
 * @brief  Start playing a predefined tone pattern.
 *
 * If another pattern is already playing, the new one overrides it.
 * BUZZER_SILENT calls drv_buzzer_stop().
 *
 * @pre    drv_buzzer_init() called.
 * @post   Pattern state machine started.  Sound begins on next tick().
 * @param  action  Which pattern to play (BUZZER_CLICK, BUZZER_ALARM, etc.).
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG.
 * @wcet   < 100 µs
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buzzer_play(buzzer_action_t action);

/**
 * @brief  Immediately silence the buzzer.
 *
 * @pre    drv_buzzer_init() called.
 * @post   Buzzer silent, pattern state machine reset.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 * @wcet   < 100 µs
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buzzer_stop(void);

/**
 * @brief  Advance the buzzer pattern state machine.
 *
 * Must be called periodically (every 10–20 ms) by HMI task.
 * Manages step timing, tone changes, and pattern looping.
 *
 * @pre    drv_buzzer_init() called.
 * @post   LEDC frequency/duty updated if step changed.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 * @wcet   < 100 µs
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buzzer_tick(void);

/**
 * @brief  Check if a tone pattern is currently playing.
 *
 * @return true if a pattern is active (not silent).
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe (reads static bool).
 * @isr_safety     ISR-safe.
 */
bool drv_buzzer_is_playing(void);

#endif /* DRV_BUZZER_H */