// ═══ FILE: main/hal/hal_gpio.h ═══
/**
 * @file    hal_gpio.h
 * @brief   Hardware abstraction for all GPIO operations.
 *          GPIO 15 (relay) is life-safety — treated with extreme care.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  CRITICAL — GPIO 15 controls the safety relay.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Edge types for interrupt configuration ──────────────────────────── */
#define GPIO_EDGE_RISING   1
#define GPIO_EDGE_FALLING  2
#define GPIO_EDGE_ANY      3

/**
 * @brief  Initialise GPIO subsystem.
 *
 * FIRST ACTION: Forces GPIO 15 LOW (relay safe state) — Rule 8.11.
 * Then reclaims JTAG pins (39, 40, 41, 42) for GPIO use.
 *
 * @pre    None — this is the first function called after reset.
 * @post   GPIO 15 is LOW.  JTAG pins available as GPIO.
 *         Module is marked initialised.
 * @return ERR_OK on success, ERR_HW_INIT_FAILED on GPIO driver error.
 * @note   Must be called before any other hal_gpio_*() function.
 * @wcet   < 1 ms
 * @thread_safety  Not thread-safe — call once from app_main() only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_gpio_init(void);

/**
 * @brief  Configure a GPIO pin as push-pull output.
 *
 * @pre    hal_gpio_init() has been called.
 * @pre    gpio_num is in the valid pin list.
 * @post   Pin is configured as output at initial_level.
 * @param  gpio_num       GPIO number (must be a valid project pin).
 * @param  initial_level  true = HIGH, false = LOW.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_HW_INIT_FAILED.
 * @wcet   < 100 µs
 * @thread_safety  Thread-safe (ESP-IDF gpio_config is re-entrant).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_gpio_config_output(uint8_t gpio_num, bool initial_level);

/**
 * @brief  Configure a GPIO pin as input.
 *
 * @pre    hal_gpio_init() has been called.
 * @pre    gpio_num is in the valid pin list.
 * @post   Pin is configured as input, with optional internal pull-up.
 * @param  gpio_num       GPIO number (must be a valid project pin).
 * @param  enable_pullup  true = enable internal pull-up resistor.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_HW_INIT_FAILED.
 * @wcet   < 100 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_gpio_config_input(uint8_t gpio_num, bool enable_pullup);

/**
 * @brief  Configure GPIO interrupt (e.g., Hall RPM sensor on GPIO 41).
 *
 * Installs the ISR service if not already installed.
 *
 * @pre    hal_gpio_init() has been called.
 * @pre    gpio_num is in the valid pin list and configured as input.
 * @pre    isr_handler is not NULL.
 * @post   ISR fires on selected edge; handler is registered.
 * @param  gpio_num     GPIO number.
 * @param  edge_type    GPIO_EDGE_RISING (1), GPIO_EDGE_FALLING (2),
 *                      or GPIO_EDGE_ANY (3).
 * @param  isr_handler  ISR callback function.
 * @param  arg          Argument passed to isr_handler (may be NULL).
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_NULL_POINTER,
 *         ERR_HW_INIT_FAILED.
 * @wcet   < 500 µs (ISR service install is one-time cost).
 * @thread_safety  Thread-safe.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_gpio_config_interrupt(uint8_t gpio_num, int edge_type,
                                       void (*isr_handler)(void *),
                                       void *arg);

/**
 * @brief  Write a level to a GPIO output pin.
 *
 * @pre    hal_gpio_init() called; pin configured as output.
 * @post   Pin level is set to requested value.
 * @param  gpio_num  GPIO number.
 * @param  level     true = HIGH, false = LOW.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_HW_WRITE_FAILED.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe (atomic register write).
 * @isr_safety     ISR-safe.
 */
error_code_t hal_gpio_write(uint8_t gpio_num, bool level);

/**
 * @brief  Read the level of a GPIO pin.
 *
 * @pre    hal_gpio_init() called; pin configured as input or output.
 * @post   *level_out contains the pin state.
 * @param  gpio_num   GPIO number.
 * @param  level_out  Pointer to bool — receives pin state (must not be NULL).
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_NULL_POINTER.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
error_code_t hal_gpio_read(uint8_t gpio_num, bool *level_out);

/**
 * @brief  EMERGENCY: Force relay GPIO 15 LOW immediately.
 *
 * Uses direct register write — no ESP-IDF API, no validation, no logging.
 * Can be called from ISR, panic handler, fault handler, anywhere.
 * This is the "oh shit" function.
 *
 * @pre    None.
 * @post   GPIO 15 output register bit is cleared (relay de-energised).
 * @return void — best-effort, conceptually never fails.
 * @wcet   < 100 ns (single register write).
 * @thread_safety  Thread-safe (atomic register write).
 * @isr_safety     ISR-safe.
 */
void hal_gpio_force_relay_safe(void);

/**
 * @brief  Check if a GPIO number is in the valid pin list.
 *
 * @param  gpio_num  GPIO number to check.
 * @return true if pin is in the project's valid pin list.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe (const data).
 * @isr_safety     ISR-safe.
 */
bool hal_gpio_is_valid_pin(uint8_t gpio_num);

#endif /* HAL_GPIO_H */