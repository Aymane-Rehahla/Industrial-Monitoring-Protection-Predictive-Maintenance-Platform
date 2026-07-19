// ═══ FILE: main/hal/hal_i2c.h ═══
/**
 * @file    hal_i2c.h
 * @brief   Hardware abstraction for I2C with bus fault protection.
 *          Bus 0 reads safety-critical sensors — never share with user devices.
 *          Bus 1 is fault-tolerant: LCD crash → detect, isolate, recover.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Bus 0 carries safety-critical sensor data.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Initialise an I2C bus with pins and frequency from app_config.h.
 *
 * @pre    bus < I2C_BUS_COUNT.
 * @pre    hal_gpio_init() has been called (pins are available).
 * @post   I2C peripheral configured, mutex created, error counters zeroed.
 * @param  bus  I2C_BUS_SENSORS or I2C_BUS_SHARED.
 * @return ERR_OK, ERR_INVALID_ARG, ERR_ALREADY_INITIALIZED, ERR_HW_INIT_FAILED.
 * @wcet   < 5 ms
 * @thread_safety  Not thread-safe — call once per bus from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_init(i2c_bus_id_t bus);

/**
 * @brief  Release I2C peripheral and free resources.
 *
 * @pre    Bus was previously initialised.
 * @post   I2C peripheral released, mutex deleted, state cleared.
 * @param  bus  Which bus to deinitialise.
 * @return ERR_OK, ERR_INVALID_ARG, ERR_NOT_INITIALIZED.
 * @wcet   < 1 ms
 * @thread_safety  Not thread-safe — ensure no other task is using the bus.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_deinit(i2c_bus_id_t bus);

/**
 * @brief  Write data to an I2C device.
 *
 * @pre    Bus initialised.  data not NULL.
 * @post   Data sent on the wire.  Error counter updated on failure.
 * @param  bus         Which I2C bus.
 * @param  dev_addr    7-bit device address.
 * @param  data        Pointer to bytes to send.
 * @param  len         Number of bytes.
 * @param  timeout_ms  Maximum time to wait (ms).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_HW_TIMEOUT,
 *         ERR_HW_NACK, ERR_HW_WRITE_FAILED, ERR_HW_INIT_FAILED (bus failed).
 * @wcet   timeout_ms + 1 ms (mutex overhead)
 * @thread_safety  Thread-safe (mutex protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_write(i2c_bus_id_t bus, uint8_t dev_addr,
                           const uint8_t *data, size_t len,
                           uint32_t timeout_ms);

/**
 * @brief  Read data from an I2C device.
 *
 * @pre    Bus initialised.  data not NULL.
 * @post   Received data written to data[0..len-1].
 * @param  bus         Which I2C bus.
 * @param  dev_addr    7-bit device address.
 * @param  data        Buffer to receive bytes (must not be NULL).
 * @param  len         Number of bytes to read.
 * @param  timeout_ms  Maximum time to wait (ms).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_HW_TIMEOUT,
 *         ERR_HW_NACK, ERR_HW_READ_FAILED, ERR_HW_INIT_FAILED (bus failed).
 * @wcet   timeout_ms + 1 ms
 * @thread_safety  Thread-safe (mutex protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_read(i2c_bus_id_t bus, uint8_t dev_addr,
                          uint8_t *data, size_t len,
                          uint32_t timeout_ms);

/**
 * @brief  Combined write-then-read with I2C repeated start.
 *
 * Used by ADS1115 (write register pointer, read result) and SHT45
 * (send command, read measurement).
 *
 * @pre    Bus initialised.  write_data and read_data not NULL.
 * @post   write_data sent, then read_data filled from device.
 * @param  bus         Which I2C bus.
 * @param  dev_addr    7-bit device address.
 * @param  write_data  Bytes to send before restart.
 * @param  write_len   Number of bytes to write.
 * @param  read_data   Buffer to receive bytes after restart.
 * @param  read_len    Number of bytes to read.
 * @param  timeout_ms  Maximum time for entire transaction.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_HW_TIMEOUT,
 *         ERR_HW_NACK, ERR_HW_READ_FAILED, ERR_HW_INIT_FAILED.
 * @wcet   timeout_ms + 1 ms
 * @thread_safety  Thread-safe (mutex protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_write_then_read(i2c_bus_id_t bus, uint8_t dev_addr,
                                     const uint8_t *write_data, size_t write_len,
                                     uint8_t *read_data, size_t read_len,
                                     uint32_t timeout_ms);

/**
 * @brief  Probe for an I2C device (send address, check for ACK).
 *
 * @pre    Bus initialised.
 * @post   No side effects on device.
 * @param  bus       Which I2C bus.
 * @param  dev_addr  7-bit device address to probe.
 * @return ERR_OK if device ACKs, ERR_HW_NOT_FOUND if NACK,
 *         ERR_INVALID_ARG, ERR_NOT_INITIALIZED, ERR_HW_INIT_FAILED.
 * @wcet   < I2C_BUS*_TIMEOUT_MS + 1 ms
 * @thread_safety  Thread-safe (mutex protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_probe(i2c_bus_id_t bus, uint8_t dev_addr);

/**
 * @brief  Attempt I2C bus recovery (clock out a stuck slave).
 *
 * Procedure:
 *   1. Deinit I2C peripheral.
 *   2. Bit-bang 16 SCL clocks (slave releases SDA).
 *   3. Generate STOP condition.
 *   4. Reinit I2C peripheral.
 *   5. Reset error counter.
 *
 * @pre    Bus was previously initialised (mutex exists).
 * @post   Bus reinitialised.  Error counter reset.  has_failed cleared.
 * @param  bus  Which I2C bus.
 * @return ERR_OK on successful recovery, ERR_HW_INIT_FAILED on failure.
 * @wcet   < 10 ms
 * @thread_safety  Thread-safe (mutex protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_i2c_recover_bus(i2c_bus_id_t bus);

/**
 * @brief  Get cumulative error count since last reset.
 *
 * @pre    count_out not NULL.
 * @post   *count_out = cumulative error count.
 * @param  bus        Which I2C bus.
 * @param  count_out  Receives the error count.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_INVALID_ARG.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
error_code_t hal_i2c_get_error_count(i2c_bus_id_t bus, uint32_t *count_out);

/**
 * @brief  Reset the error counter for a bus.
 *
 * @pre    bus < I2C_BUS_COUNT.
 * @post   Error counter set to zero.
 * @param  bus  Which I2C bus.
 * @return ERR_OK, ERR_INVALID_ARG.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
error_code_t hal_i2c_reset_error_count(i2c_bus_id_t bus);

/**
 * @brief  Check if an I2C bus has been initialised.
 *
 * @param  bus  Which I2C bus.
 * @return true if initialised and not in failed state.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
bool hal_i2c_is_initialized(i2c_bus_id_t bus);

#endif /* HAL_I2C_H */