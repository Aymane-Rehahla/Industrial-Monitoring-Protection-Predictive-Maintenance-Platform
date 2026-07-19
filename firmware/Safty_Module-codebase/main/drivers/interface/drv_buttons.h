// ═══ FILE: main/drivers/interface/drv_buttons.h ═══
/**
 * @file    drv_buttons.h
 * @brief   Polled button driver for 5 navigation buttons with debounce,
 *          hold detection, and auto-repeat.
 *          Events are pushed to a FreeRTOS queue for HMI consumption.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — buttons are informational, not safety-critical.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef DRV_BUTTONS_H
#define DRV_BUTTONS_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Initialise button GPIOs and event queue.
 *
 * Configures all 5 button GPIOs as inputs with internal pull-ups.
 * Creates a FreeRTOS queue (depth 16) for button events.
 *
 * @pre    hal_gpio_init() has been called (JTAG pins reclaimed).
 * @post   All buttons readable, event queue created.
 * @return ERR_OK, ERR_HW_INIT_FAILED.
 * @wcet   < 2 ms
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buttons_init(void);

/**
 * @brief  Scan all buttons and generate events.
 *
 * Must be called periodically (every BTN_SCAN_MS = 20 ms) by HMI task.
 * Reads GPIOs, runs debounce, generates PRESSED/RELEASED/HELD/REPEAT.
 * Events pushed to internal queue (non-blocking, dropped if full).
 *
 * @pre    drv_buttons_init() called.
 * @post   Internal debounce states updated, events queued.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 * @wcet   < 500 µs (5 GPIO reads + debounce logic)
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buttons_scan(void);

/**
 * @brief  Retrieve next button event from queue.
 *
 * Blocks up to timeout_ms waiting for an event.
 * If no event within timeout, event_out->event = BTN_EVENT_NONE.
 *
 * @pre    drv_buttons_init() called.  event_out not NULL.
 * @post   event_out filled with next event, or BTN_EVENT_NONE on timeout.
 * @param  event_out   Pointer to receive the event.
 * @param  timeout_ms  Maximum wait time in milliseconds.
 * @return ERR_OK if event retrieved, ERR_TIMEOUT if no event,
 *         ERR_NULL_POINTER, ERR_NOT_INITIALIZED.
 * @wcet   timeout_ms + 1 ms
 * @thread_safety  Thread-safe (FreeRTOS queue is internally protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_buttons_get_event(button_event_t *event_out,
                                   uint32_t timeout_ms);

/**
 * @brief  Check if a specific button is currently pressed (debounced).
 *
 * Does NOT consume events from the queue.
 *
 * @pre    drv_buttons_init() called.
 * @param  btn  Button to check (BTN_LEFT..BTN_OK).
 * @return true if button is currently pressed, false otherwise.
 *         Returns false for invalid btn values.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe (reads volatile state).
 * @isr_safety     ISR-safe.
 */
bool drv_buttons_is_pressed(button_id_t btn);

/**
 * @brief  Check if any button is currently pressed.
 *
 * Used for wake-from-idle detection.
 *
 * @pre    drv_buttons_init() called.
 * @return true if at least one button is pressed.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
bool drv_buttons_any_pressed(void);

#endif /* DRV_BUTTONS_H */