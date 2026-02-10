/**
 * @file  drv_buttons.h
 * @brief 5-button input with debounce, long-press and repeat.
 * @version 1.0.0
 *
 * @safety LOW
 * @hardware Active-low buttons with internal pull-ups.
 *           GPIO35=UP  GPIO36=DOWN  GPIO37=LEFT  GPIO38=RIGHT  GPIO39=OK
 *
 * Rule 14.5: Debounce — hardware (pull-up) + software (30 ms).
 * Rule 14.6: Long-press (1500 ms) for critical actions.
 */
#ifndef DRV_BUTTONS_H
#define DRV_BUTTONS_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/**
 * @brief  Initialize button driver.
 * @return ERR_OK
 * @wcet   1 ms | thread-safe NO | isr-safe NO
 */
error_code_t buttons_init(void);

/**
 * @brief  Poll and update all button states.  Call every 10–20 ms.
 * @return ERR_OK
 * @wcet   <500 µs | thread-safe NO | isr-safe NO
 */
error_code_t buttons_update(void);

/**
 * @brief  Get snapshot of all buttons.
 * @param  out  Destination (must not be NULL)
 * @return ERR_OK, ERR_NULL_POINTER
 * @wcet   <1 µs | thread-safe YES (atomic copy) | isr-safe NO
 */
error_code_t buttons_get_state(buttons_t *out);

/**
 * @brief  Was this button just pressed this cycle?
 */
bool buttons_just_pressed(button_id_t id);

/**
 * @brief  Was this button just released this cycle?
 */
bool buttons_just_released(button_id_t id);

/**
 * @brief  Is this button being held (≥ BTN_LONG_PRESS_MS)?
 */
bool buttons_is_held(button_id_t id);

/**
 * @brief  Should the value auto-repeat this cycle?
 *         (true once per BTN_REPEAT_RATE_MS after BTN_REPEAT_DELAY_MS)
 */
bool buttons_should_repeat(button_id_t id);

/**
 * @brief  Was any button pressed this cycle?
 */
bool buttons_any_pressed(void);

/**
 * @brief  Register a callback invoked on every new press (for buzzer click).
 *         Set to NULL to disable.
 */
void buttons_set_press_callback(void (*cb)(button_id_t id));

#endif /* DRV_BUTTONS_H */