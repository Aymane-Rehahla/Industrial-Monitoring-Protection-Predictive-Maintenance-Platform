// ═══ FILE: main/core/redundancy/redundancy.h ═══
/**
 * @file    redundancy.h
 * @brief   Peer ESP32 communication status and cross-validation.
 *          Tracks heartbeat, peer health, and agreement between ESPs.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Cross-validation is a safety layer.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release — API definition.
 */

#ifndef REDUNDANCY_H
#define REDUNDANCY_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Peer connection status.
 */
typedef enum {
    PEER_UNKNOWN  = 0,
    PEER_ONLINE   = 1,
    PEER_DEGRADED = 2,
    PEER_OFFLINE  = 3
} peer_status_t;

/**
 * @brief  Complete peer information snapshot.
 */
typedef struct {
    peer_status_t status;
    uint32_t      last_heartbeat_ms;
    uint32_t      heartbeat_age_ms;
    uint32_t      messages_received;
    uint32_t      messages_failed;
    bool          cross_validation_ok;
    uint8_t       peer_mac[6];
} peer_info_t;

/** @brief Initialise redundancy manager. */
error_code_t redundancy_init(void);

/** @brief Get complete peer information. */
error_code_t redundancy_get_peer_info(peer_info_t *out);

/** @brief Get peer connection status. */
peer_status_t redundancy_get_peer_status(void);

/** @brief Check if cross-validation with peer agrees. */
bool redundancy_is_cross_valid(void);

/** @brief Set the peer ESP's MAC address. */
error_code_t redundancy_set_peer_mac(const uint8_t mac[6]);

/** @brief Get the stored peer MAC address. */
error_code_t redundancy_get_peer_mac(uint8_t mac_out[6]);

#endif /* REDUNDANCY_H */