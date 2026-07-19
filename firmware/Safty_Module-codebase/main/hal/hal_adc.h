// ═══ FILE: main/hal/hal_adc.h ═══
/**
 * @file    hal_adc.h
 * @brief   ADC hardware abstraction layer.
 *          Reads safety sensors: vibration (LIS344), gas (MQ-x), sound (MAX9814).
 *          ADC1 only — ADC2 conflicts with ESP-NOW / WiFi.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — ADC reads feed into trip decisions.
 *
 * NOTE: This is currently a STUB.  Real implementation will be provided
 *       in Prompt #2 (sensor drivers).
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Stub release — compiles, returns fake data.
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Initialise the ADC subsystem (ADC1, 12-bit, 12 dB attenuation).
 *
 * @pre    None.
 * @post   ADC ready for readings (or stub pretends it is).
 * @return ERR_OK on success, ERR_HW_INIT_FAILED on driver error.
 * @wcet   < 5 ms
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_adc_init(void);

/**
 * @brief  Read raw ADC value from a GPIO pin (ADC1 channel).
 *
 * @pre    hal_adc_init() called.  gpio_num is an ADC1 pin.
 * @post   *raw_out contains 12-bit raw ADC value (0–4095).
 * @param  gpio_num  GPIO number (must map to ADC1 channel).
 * @param  raw_out   Pointer to receive raw value (must not be NULL).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_INVALID_ARG.
 * @wcet   < 200 µs
 * @thread_safety  Thread-safe (ESP-IDF ADC1 is mutex-protected internally).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_adc_read_raw(uint8_t gpio_num, int32_t *raw_out);

/**
 * @brief  Read ADC value converted to millivolts.
 *
 * @pre    hal_adc_init() called.  gpio_num is an ADC1 pin.
 * @post   *mv_out contains voltage in millivolts (0–3300 typical).
 * @param  gpio_num  GPIO number (must map to ADC1 channel).
 * @param  mv_out    Pointer to receive millivolt value (must not be NULL).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_INVALID_ARG.
 * @wcet   < 200 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_adc_read_millivolts(uint8_t gpio_num, int32_t *mv_out);

/**
 * @brief  Check if the ADC subsystem has been initialised.
 *
 * @return true if initialised.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
bool hal_adc_is_initialized(void);

#endif /* HAL_ADC_H */