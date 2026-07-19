// ═══ FILE: main/hal/hal_uart.c ═══
/**
 * @file    hal_uart.c
 * @brief   UART HAL — STUB IMPLEMENTATION.
 *
 *          ╔══════════════════════════════════════════════════════════╗
 *          ║  WARNING: THIS IS A STUB.  NO REAL UART COMMUNICATION. ║
 *          ║  Send: data is silently discarded, returns ERR_OK.     ║
 *          ║  Receive: always returns ERR_TIMEOUT with 0 bytes.     ║
 *          ║  This exists ONLY so upper layers compile and run       ║
 *          ║  without a peer ESP connected.                          ║
 *          ║  Replace with real UART driver before field deployment. ║
 *          ╚══════════════════════════════════════════════════════════╝
 *
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Must be replaced before production use.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Stub release — send discards, receive times out.
 */

#include "hal/hal_uart.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "hal_uart";

/* Justification: Tracks initialisation state. Must persist across calls. */
static bool s_uart_initialized = false;

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_uart_init  (STUB)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_uart_init(void)
{
    ESP_LOGW(TAG, "STUB: UART init — no real hardware configured");
    s_uart_initialized = true;
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_uart_send  (STUB — data silently discarded)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_uart_send(const uint8_t *data, size_t len,
                           uint32_t timeout_ms)
{
    if (data == NULL) {
        return ERR_NULL_POINTER;
    }

    /* Suppress unused-parameter warnings. */
    UNUSED(len);
    UNUSED(timeout_ms);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_uart_receive  (STUB — always times out with 0 bytes)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_uart_receive(uint8_t *buf, size_t buf_size,
                              size_t *received_len, uint32_t timeout_ms)
{
    if (buf == NULL || received_len == NULL) {
        return ERR_NULL_POINTER;
    }

    UNUSED(buf_size);
    UNUSED(timeout_ms);

    *received_len = 0;
    return ERR_TIMEOUT;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_uart_flush  (STUB — nothing to flush)
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_uart_flush(void)
{
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_uart_is_initialized  (STUB)
 * ═══════════════════════════════════════════════════════════════════════ */
bool hal_uart_is_initialized(void)
{
    return s_uart_initialized;
}