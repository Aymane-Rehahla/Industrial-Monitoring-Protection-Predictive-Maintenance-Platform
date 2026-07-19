/**
 * @file    pairing.h
 * @brief   WROOM peer management — MAC storage, NVS persistence,
 *          liveness tracking, XTEA authentication handshake.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — WROOM peers are observability, not safety path.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef PAIRING_H
#define PAIRING_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  WROOM peer status.
 */
typedef enum {
    WROOM_UNKNOWN          = 0,
    WROOM_OFFLINE          = 1,
    WROOM_AUTHENTICATING   = 2,
    WROOM_ONLINE           = 3
} wroom_peer_status_t;

/**
 * @brief  Information about one WROOM peer slot.
 */
typedef struct {
    uint8_t             mac[6];          /**< Stored MAC address           */
    bool                mac_valid;       /**< Slot has a valid MAC         */
    wroom_peer_status_t status;          /**< Current link status          */
    bool                authenticated;   /**< XTEA handshake passed        */
    uint32_t            last_seen_ms;    /**< Last valid packet timestamp  */
    uint32_t            packets_rx;      /**< Packets received from peer   */
    uint32_t            packets_tx;      /**< Packets sent to peer         */
} pairing_peer_info_t;

/**
 * @brief  Initialise pairing module.
 *
 * Loads stored WROOM MACs from NVS and adds them as ESP-NOW peers.
 *
 * @pre    nvs_config_init() and espnow_link_init() called.
 * @post   Peers loaded.  Auth will happen when WROOMs come online.
 * @return ERR_OK, ERR_NOT_INITIALIZED (if espnow_link not ready).
 */
error_code_t pairing_init(void);

/**
 * @brief  Get number of configured WROOM peer slots (0–2).
 * @return Number of slots with valid MACs.
 */
uint8_t pairing_get_peer_count(void);

/**
 * @brief  Get info about a peer slot.
 *
 * @param  index   0 or 1.
 * @param  out     Receives peer info.  Must not be NULL.
 * @return ERR_OK, ERR_INVALID_ARG, ERR_NULL_POINTER.
 */
error_code_t pairing_get_peer(uint8_t index, pairing_peer_info_t *out);

/**
 * @brief  Set MAC address for a peer slot.
 *
 * Adds peer to ESP-NOW and marks slot as valid.
 * Does NOT save to NVS — call pairing_save_to_nvs() separately.
 *
 * @param  index  0 or 1.
 * @param  mac    6-byte MAC.  Must not be NULL.
 * @return ERR_OK, ERR_INVALID_ARG, ERR_NULL_POINTER.
 */
error_code_t pairing_set_peer_mac(uint8_t index, const uint8_t mac[6]);

/**
 * @brief  Remove a peer from a slot.
 *
 * Removes from ESP-NOW and clears slot.
 * Does NOT save to NVS — call pairing_save_to_nvs() separately.
 *
 * @param  index  0 or 1.
 * @return ERR_OK, ERR_INVALID_ARG.
 */
error_code_t pairing_remove_peer(uint8_t index);

/**
 * @brief  Save current peer list to NVS.
 * @return ERR_OK, ERR_NVS_WRITE_FAILED.
 */
error_code_t pairing_save_to_nvs(void);

/**
 * @brief  Load peer list from NVS.
 * @return ERR_OK, ERR_NVS_NOT_FOUND (no saved peers — not an error).
 */
error_code_t pairing_load_from_nvs(void);

/**
 * @brief  Check if a peer slot is authenticated (XTEA handshake passed).
 *
 * @param  index  0 or 1.
 * @return true if authenticated.
 */
bool pairing_is_peer_authenticated(uint8_t index);

/**
 * @brief  Get peer status.
 *
 * @param  index  0 or 1.
 * @return Status enum.  Returns WROOM_UNKNOWN for invalid index.
 */
wroom_peer_status_t pairing_get_peer_status(uint8_t index);

/**
 * @brief  Update last-seen timestamp for a peer identified by MAC.
 *
 * Called by telemetry/espnow handlers when any valid packet arrives
 * from a known WROOM.
 *
 * @param  mac  6-byte sender MAC.
 */
void pairing_update_peer_seen(const uint8_t mac[6]);

/**
 * @brief  Handle incoming auth message (challenge or response).
 *
 * Called from espnow_link RX handler for MSG_AUTH_CHALLENGE and
 * MSG_AUTH_RESPONSE.  S3 only sends challenges; WROOM sends responses.
 *
 * @param  src_mac      6-byte sender MAC.
 * @param  payload      Auth payload bytes.
 * @param  payload_len  Length.
 * @param  msg_type     MSG_AUTH_CHALLENGE or MSG_AUTH_RESPONSE.
 */
void pairing_handle_auth(const uint8_t *src_mac,
                         const uint8_t *payload,
                         uint8_t payload_len,
                         uint8_t msg_type);

/**
 * @brief  Send auth challenge to a peer (called when peer first heard).
 *
 * @param  index  0 or 1.
 * @return ERR_OK, ERR_INVALID_ARG, ERR_NOT_INITIALIZED.
 */
error_code_t pairing_send_challenge(uint8_t index);

/**
 * @brief  Periodic check — update peer status based on timeouts.
 *
 * Call from telemetry task at 1 Hz.  Marks peers OFFLINE if
 * last_seen exceeds ESPNOW_PEER_TIMEOUT_MS.
 */
void pairing_check_timeouts(void);

#endif /* PAIRING_H */