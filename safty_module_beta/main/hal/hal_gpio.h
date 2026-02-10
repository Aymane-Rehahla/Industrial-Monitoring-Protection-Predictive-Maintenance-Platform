/**
 * @file hal_gpio.h
 * @brief GPIO hardware abstraction. FROZEN.
 * @version 1.0.1
 * @safety CRITICAL
 *
 * Rule 9.1: All GPIO pins have defined safe states.
 * Rule 9.2: All outputs init to safe state before main().
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/**
 * @brief Init all GPIO to safe defaults.
 * @return ERR_OK or ERR_GPIO_INIT_FAILED
 * @wcet 10 ms | thread-safe NO | isr-safe NO
 * @note Rule 8.11: first instruction opens contactor.
 */
error_code_t hal_gpio_init(void);

/**
 * @brief Set an output pin.
 * @return ERR_OK, ERR_GPIO_NOT_INIT, ERR_GPIO_INVALID_PIN
 * @wcet <1 µs | thread-safe YES | isr-safe YES
 */
error_code_t hal_gpio_set_output(uint8_t pin, bool state);

/**
 * @brief Read an input pin.
 * @return ERR_OK, ERR_GPIO_NULL_POINTER, ERR_GPIO_INVALID_PIN
 * @wcet <1 µs | thread-safe YES | isr-safe YES
 */
error_code_t hal_gpio_get_input(uint8_t pin, bool *state_out);

/**
 * @brief EMERGENCY: all outputs to safe state via direct register writes.
 * @return ERR_OK (always)
 * @wcet <10 µs | thread-safe YES | isr-safe YES
 * @note Rule 8.4: bypasses all software.
 */
error_code_t hal_gpio_emergency_safe(void);

/**
 * @brief Configure Hall sensor falling-edge interrupt.
 * @param callback NULL to disable
 * @return ERR_OK or ERR_GPIO_NOT_INIT
 * @wcet 100 µs | thread-safe NO | isr-safe NO
 */
error_code_t hal_gpio_configure_hall_isr(void (*callback)(void));

/**
 * @brief Read ATtiny85 relay-enable input.
 * @return ERR_OK or ERR_GPIO_NULL_POINTER / ERR_GPIO_NOT_INIT
 */
error_code_t hal_gpio_get_relay_enable(bool *enabled_out);

/**
 * @brief Toggle heartbeat output to ATtiny85.
 * @return ERR_OK or ERR_GPIO_NOT_INIT
 */
error_code_t hal_gpio_send_heartbeat(void);

/**
 * @brief Drive relay output (actual relay = this AND ATtiny enable).
 * @return ERR_OK or ERR_GPIO_NOT_INIT
 */
error_code_t hal_gpio_set_relay(bool close_relay);

/**
 * @brief Self-test GPIO (LED blink + relay safe check).
 * @return ERR_OK or ERR_GPIO_SELF_TEST_FAIL
 * @wcet 50 ms
 */
error_code_t hal_gpio_self_test(void);

#endif /* HAL_GPIO_H */