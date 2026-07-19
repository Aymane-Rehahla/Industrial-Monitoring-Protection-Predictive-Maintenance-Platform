// ═══ FILE: main/drivers/actuators/drv_leds.h ═══
/**
 * @file    drv_leds.h
 * @brief   LED indicator driver for red GPIO LED and WS2812 RGB NeoPixel.
 *          Both support blink patterns via tick-based state machine.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — informational only, not safety-critical.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef DRV_LEDS_H
#define DRV_LEDS_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Initialise both LEDs.
 *
 * Red LED: GPIO 40 configured as output, initially OFF.
 * RGB LED: WS2812 on GPIO 48 via led_strip driver, initially OFF.
 * Partial init is OK — if RGB fails, red LED still works.
 *
 * @pre    hal_gpio_init() has been called.
 * @post   Both LEDs off and ready for set_mode() calls.
 * @return ERR_OK (even if RGB init fails — red LED is sufficient).
 * @wcet   < 5 ms
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_leds_init(void);

/**
 * @brief  Set the blink mode for an LED.
 *
 * @pre    drv_leds_init() called.
 * @post   LED mode updated; takes effect on next tick().
 * @param  led   Which LED (LED_RED or LED_RGB).
 * @param  mode  Desired mode (LED_OFF, LED_ON, LED_BLINK_SLOW, etc.).
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG.
 * @wcet   < 10 µs
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_leds_set_mode(led_id_t led, led_mode_t mode);

/**
 * @brief  Set the colour for the RGB LED.
 *
 * Values 0–255, internally scaled by RGB_BRIGHTNESS.
 * Does NOT turn LED on — call set_mode(LED_RGB, LED_ON) after.
 *
 * @pre    drv_leds_init() called.
 * @post   Colour stored; applied on next tick when LED is "on".
 * @param  red    Red channel 0–255.
 * @param  green  Green channel 0–255.
 * @param  blue   Blue channel 0–255.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 * @wcet   < 10 µs
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_leds_set_rgb_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief  Advance the LED blink state machines.
 *
 * Must be called periodically (every LED_UPDATE_MS, suggest 50 ms).
 * Handles toggling for BLINK_SLOW, BLINK_FAST, BLINK_SOS modes.
 *
 * @pre    drv_leds_init() called.
 * @post   LED hardware states updated per current modes.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 * @wcet   < 500 µs (includes WS2812 data transmission ~30 µs for 1 LED)
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_leds_tick(void);

/**
 * @brief  Check if the LED driver has been initialised.
 *
 * @return true if drv_leds_init() has been called successfully.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
bool drv_leds_is_initialized(void);

#endif /* DRV_LEDS_H */