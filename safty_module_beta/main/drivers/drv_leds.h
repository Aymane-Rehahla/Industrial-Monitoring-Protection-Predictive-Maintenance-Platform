/**
 * @file  drv_leds.h
 * @brief LED driver with blink patterns.
 * @version 1.0.0
 *
 * @safety LOW
 * @hardware GPIO10=GREEN  GPIO11=RED
 *
 * Rule 14.8: Status LEDs visible from 5 metres.
 * Rule 14.2: Critical alarms have distinct visual patterns.
 */
#ifndef DRV_LEDS_H
#define DRV_LEDS_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/**
 * @brief  Initialize LED driver.
 * @return ERR_OK
 * @wcet   1 ms | thread-safe NO | isr-safe NO
 */
error_code_t leds_init(void);

/**
 * @brief  Set operating mode for one LED.
 * @param  id    LED_GREEN or LED_RED
 * @param  mode  LED_MODE_OFF, _ON, _BLINK_SLOW, _BLINK_FAST, _BLINK_VERY_FAST
 * @return ERR_OK, ERR_INVALID_PARAMETER
 */
error_code_t leds_set_mode(led_id_t id, led_mode_t mode);

/**
 * @brief  Update LED outputs (call every 50–100 ms).
 * @return ERR_OK
 * @wcet   <100 µs | thread-safe NO | isr-safe NO
 */
error_code_t leds_update(void);

/**
 * @brief  Get current mode for an LED.
 */
led_mode_t leds_get_mode(led_id_t id);

/**
 * @brief  All LEDs off (emergency).
 * @return ERR_OK
 */
error_code_t leds_all_off(void);

/**
 * @brief  Set pattern for system state (convenience).
 *         READY: green on.  RUNNING: green on.  WARNING: red slow blink.
 *         FAULT: red fast blink.  SAFE_MODE: both fast blink.
 */
error_code_t leds_set_system_pattern(system_state_t state);

#endif /* DRV_LEDS_H */