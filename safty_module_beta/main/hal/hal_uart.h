/**
 * @file hal_uart.h
 * @brief UART HAL for ESP-to-ESP redundancy. FROZEN.
 * @version 1.0.1
 * @safety CRITICAL
 *
 * BUG 16: pin definitions moved to app_config.h.
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "system_types.h"

typedef struct {
    uint32_t      pkts_sent;
    uint32_t      pkts_recv;
    uint32_t      pkts_valid;
    uint32_t      pkts_invalid;
    uint32_t      hb_sent;
    uint32_t      hb_recv;
    uint32_t      timeouts;
    peer_status_t peer;
    uint32_t      last_hb_ms;
    uint32_t      peer_uptime_ms;
} peer_stats_t;

error_code_t  hal_uart_init(void);
error_code_t  hal_uart_send_packet(packet_type_t type, const void *payload, size_t len);
error_code_t  hal_uart_send_heartbeat(system_state_t st, uint8_t faults,
                                       bool relay_cmd, bool relay_en);
error_code_t  hal_uart_send_sensor_data(const sensor_sync_payload_t *d);
int           hal_uart_process_rx(packet_callback_t cb);
peer_status_t hal_uart_get_peer_status(void);
bool          hal_uart_is_peer_online(void);
error_code_t  hal_uart_get_stats(peer_stats_t *out);
error_code_t  hal_uart_self_test(void);

#endif /* HAL_UART_H */