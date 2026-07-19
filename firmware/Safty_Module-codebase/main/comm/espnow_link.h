/**
 * @file    espnow_link.h
 * @brief   ESP-NOW transport layer for WROOM peer communication.
 *          Handles WiFi/ESP-NOW init, packet framing, CRC, XTEA auth,
 *          send/receive with ISR-safe queue dispatch.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — Communication link, not direct safety path.
 *          WROOM loss degrades observability but does not trip contactor.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  PROTOCOL STRUCTURES — all packed, no floats, CRC protected
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Packet header — prepended to every ESP-NOW frame.
 *
 * WHY packed: Byte-exact layout required for cross-platform
 * compatibility between ESP32-S3 and ESP32 WROOM.
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic;          /**< COMM_PACKET_MAGIC (0x5A)              */
    uint8_t  version;        /**< COMM_PROTOCOL_VERSION (1)             */
    uint8_t  msg_type;       /**< comm_msg_type_t                       */
    uint8_t  src_role;       /**< device_role_t of sender               */
    uint16_t sequence;       /**< Rolling sequence number               */
    uint8_t  payload_len;    /**< Bytes of payload after this header    */
    uint8_t  node_id;        /**< Sender node ID (1=S3-A, 2=S3-B)      */
} comm_header_t;

/**
 * @brief  Packet footer — appended after payload.
 */
typedef struct __attribute__((packed)) {
    uint16_t crc16;          /**< CRC-16 over header + payload          */
} comm_footer_t;

/**
 * @brief  Maximum payload size.
 *         ESP-NOW limit = 250.  Header = 8, Footer = 2.  Payload ≤ 240.
 */
#define COMM_MAX_PAYLOAD_SIZE  (250 - sizeof(comm_header_t) - sizeof(comm_footer_t))

/**
 * @brief  Received packet passed from ISR to dispatch task via queue.
 */
typedef struct {
    uint8_t  src_mac[6];     /**< Sender MAC address                    */
    uint8_t  data[250];      /**< Raw frame (header + payload + footer) */
    uint8_t  data_len;       /**< Total frame length                    */
} espnow_rx_item_t;

/**
 * @brief  Link statistics.
 */
typedef struct {
    uint32_t tx_count;       /**< Total packets sent                    */
    uint32_t tx_fail_count;  /**< Send failures                         */
    uint32_t rx_count;       /**< Total valid packets received          */
    uint32_t rx_crc_errors;  /**< Packets with bad CRC                  */
    uint32_t rx_dropped;     /**< Packets dropped (queue full)          */
    uint32_t rx_unknown_src; /**< Packets from unknown MAC              */
    uint32_t last_tx_ms;     /**< Tick count of last successful send    */
    uint32_t last_rx_ms;     /**< Tick count of last valid receive      */
} espnow_stats_t;

/**
 * @brief  Receive handler callback type.
 *
 * Called from the RX dispatch task (not ISR) when a valid packet
 * of the registered msg_type arrives.
 *
 * @param  src_mac      6-byte sender MAC.
 * @param  payload      Pointer to payload bytes (after header).
 * @param  payload_len  Number of payload bytes.
 */
typedef void (*espnow_rx_handler_t)(const uint8_t *src_mac,
                                    const uint8_t *payload,
                                    uint8_t payload_len);

/* ═══════════════════════════════════════════════════════════════════════
 *  XTEA UTILITY — exposed for pairing auth module
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  XTEA encrypt a single 64-bit block in-place.
 *
 * @param  v    Two uint32_t values to encrypt.  Modified in-place.
 * @param  key  Four uint32_t key words (128-bit key).
 */
void espnow_xtea_encrypt(uint32_t v[2], const uint32_t key[4]);

/**
 * @brief  XTEA decrypt a single 64-bit block in-place.
 *
 * @param  v    Two uint32_t values to decrypt.  Modified in-place.
 * @param  key  Four uint32_t key words (128-bit key).
 */
void espnow_xtea_decrypt(uint32_t v[2], const uint32_t key[4]);

/* ═══════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Initialise WiFi (STA mode) and ESP-NOW.
 *
 * Creates RX dispatch task on Core 0.
 *
 * @pre    nvs_config_init() called (WiFi uses NVS internally).
 * @post   ESP-NOW ready.  No peers added yet.
 * @return ERR_OK, ERR_HW_INIT_FAILED, ERR_ALREADY_INITIALIZED.
 * @wcet   < 500 ms
 */
error_code_t espnow_link_init(void);

/**
 * @brief  Tear down ESP-NOW and WiFi.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 */
error_code_t espnow_link_deinit(void);

/**
 * @brief  Add a peer MAC address for ESP-NOW communication.
 *
 * @param  mac      6-byte MAC address.
 * @param  channel  WiFi channel (use ESPNOW_WIFI_CHANNEL).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_HW_INIT_FAILED.
 */
error_code_t espnow_link_add_peer(const uint8_t mac[6], uint8_t channel);

/**
 * @brief  Remove a peer MAC address.
 *
 * @param  mac  6-byte MAC address.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NOT_INITIALIZED, ERR_NOT_FOUND.
 */
error_code_t espnow_link_remove_peer(const uint8_t mac[6]);

/**
 * @brief  Send a framed packet to a specific peer.
 *
 * Builds header (magic, version, type, role, sequence, len, node_id),
 * computes CRC-16 over header+payload, sends via esp_now_send().
 *
 * @param  dest_mac     6-byte destination MAC.  NULL = broadcast.
 * @param  msg_type     comm_msg_type_t value.
 * @param  payload      Payload bytes.  NULL if payload_len == 0.
 * @param  payload_len  Length of payload.  Must be ≤ COMM_MAX_PAYLOAD_SIZE.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_HW_WRITE_FAILED.
 * @wcet   < 5 ms (non-blocking, queued internally by ESP-IDF).
 */
error_code_t espnow_link_send(const uint8_t dest_mac[6],
                              uint8_t msg_type,
                              const uint8_t *payload,
                              uint8_t payload_len);

/**
 * @brief  Register a handler for a specific message type.
 *
 * Only one handler per msg_type.  Replaces previous if set again.
 *
 * @param  msg_type  comm_msg_type_t value.
 * @param  handler   Callback function.  NULL to unregister.
 * @return ERR_OK, ERR_INVALID_ARG.
 */
error_code_t espnow_link_register_handler(uint8_t msg_type,
                                          espnow_rx_handler_t handler);

/**
 * @brief  Get link statistics.
 *
 * @param  stats_out  Pointer to receive stats.  Must not be NULL.
 * @return ERR_OK, ERR_NULL_POINTER.
 */
error_code_t espnow_link_get_stats(espnow_stats_t *stats_out);

/**
 * @brief  Check if ESP-NOW link is initialised and ready.
 * @return true if ready.
 */
bool espnow_link_is_ready(void);

#endif /* ESPNOW_LINK_H */