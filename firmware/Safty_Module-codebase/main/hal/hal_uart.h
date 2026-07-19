// ═══ FILE: main/hal/hal_uart.h ═══
/**
 * @file    hal_uart.h
 * @brief   UART abstraction for peer ESP-to-ESP communication.
 *          Uses UART1 on GPIO 43 (TX) / GPIO 44 (RX), 115200 baud.
 *          Carries cross-validation and heartbeat data between the two ESPs.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Carries cross-validation data for safety chain.
 *
 * NOTE: This is currently a STUB.  Real implementation will be provided
 *       in Prompt #2 (communication drivers).
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Stub release — compiles, discards data.
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Initialise UART peripheral for peer communication.
 *
 * Configures UART1 with:
 *   - 115200 baud, 8N1
 *   - TX on GPIO 43, RX on GPIO 44
 *   - RX/TX ring buffers per app_config.h
 *
 * @pre    None.
 * @post   UART ready for send/receive (or stub pretends it is).
 * @return ERR_OK, ERR_HW_INIT_FAILED.
 * @wcet   < 5 ms
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_uart_init(void);

/**
 * @brief  Send data to the peer ESP via UART.
 *
 * @pre    hal_uart_init() called.  data not NULL.
 * @post   Data queued for transmission (or discarded in stub).
 * @param  data        Pointer to bytes to send.
 * @param  len         Number of bytes.
 * @param  timeout_ms  Maximum time to wait if TX buffer is full.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_TIMEOUT.
 * @wcet   timeout_ms + 1 ms
 * @thread_safety  Thread-safe (UART driver is internally protected).
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_uart_send(const uint8_t *data, size_t len,
                           uint32_t timeout_ms);

/**
 * @brief  Receive data from the peer ESP via UART.
 *
 * @pre    hal_uart_init() called.  buf not NULL.  received_len not NULL.
 * @post   *received_len = number of bytes actually read into buf.
 * @param  buf           Buffer to receive bytes.
 * @param  buf_size      Size of buf in bytes.
 * @param  received_len  Pointer to size_t — receives actual byte count.
 * @param  timeout_ms    Maximum time to wait for data.
 * @return ERR_OK if data received, ERR_TIMEOUT if no data within timeout,
 *         ERR_NULL_POINTER, ERR_NOT_INITIALIZED.
 * @wcet   timeout_ms + 1 ms
 * @thread_safety  Thread-safe.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_uart_receive(uint8_t *buf, size_t buf_size,
                              size_t *received_len, uint32_t timeout_ms);

/**
 * @brief  Flush the UART TX buffer (wait until all bytes sent).
 *
 * @pre    hal_uart_init() called.
 * @post   TX buffer empty.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 * @wcet   Depends on buffered data — typically < 50 ms.
 * @thread_safety  Thread-safe.
 * @isr_safety     Not ISR-safe.
 */
error_code_t hal_uart_flush(void);

/**
 * @brief  Check if UART has been initialised.
 *
 * @return true if initialised.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe.
 * @isr_safety     ISR-safe.
 */
bool hal_uart_is_initialized(void);

#endif /* HAL_UART_H */