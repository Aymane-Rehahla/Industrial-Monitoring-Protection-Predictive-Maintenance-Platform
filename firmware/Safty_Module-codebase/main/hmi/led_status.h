// ═══ FILE: main/hmi/led_status.h ═══
/**
 * @file    led_status.h
 * @brief   Maps system state to LED visual patterns.
 *          Called periodically by HMI manager.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — informational only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "system_types.h"

/**
 * @brief  Initialise LED status mapping.
 *
 * @pre    drv_leds_init() called.
 * @post   LEDs set to boot pattern.
 * @return ERR_OK.
 */
error_code_t led_status_init(void);

/**
 * @brief  Update LEDs based on current system state.
 *
 * Called every LED_UPDATE_INTERVAL_MS by hmi_manager.
 *
 * @pre    led_status_init() called.
 * @post   LED modes and colours updated, drv_leds_tick() called.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 */
error_code_t led_status_tick(void);

/**
 * @brief  Override automatic LED behaviour for one LED.
 *
 * Override is cleared automatically when system state changes.
 *
 * @pre    led_status_init() called.
 * @post   Specified LED uses manual mode until state change.
 * @param  led   Which LED.
 * @param  mode  Desired mode.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG.
 */
error_code_t led_status_force(led_id_t led, led_mode_t mode);

#endif /* LED_STATUS_H */