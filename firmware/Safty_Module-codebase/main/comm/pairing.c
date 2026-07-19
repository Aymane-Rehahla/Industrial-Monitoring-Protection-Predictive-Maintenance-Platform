/**
 * @file    pairing.c
 * @brief   WROOM peer management — MAC slots, NVS, liveness, XTEA auth.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — Observability path, not safety-critical.
 *
 * DESIGN:
 *   Two peer slots (index 0 and 1).  Each stores a WROOM MAC.
 *   On boot, MACs are loaded from NVS and added to ESP-NOW.
 *   When a stored peer is first heard, S3 sends an AUTH_CHALLENGE.
 *   WROOM must respond with XTEA-encrypted challenge value.
 *   If verified, peer is marked ONLINE + authenticated.
 *   Telemetry only sends to authenticated peers.
 *   If no packets received for ESPNOW_PEER_TIMEOUT_MS, peer goes OFFLINE.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "comm/pairing.h"
#include "comm/espnow_link.h"
#include "storage/nvs_config.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"

#include <string.h>

static const char *TAG = "pairing";

/* ═══════════════════════════════════════════════════════════════════════
 *  AUTH PACKET STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint32_t challenge;
    uint32_t device_id;
} auth_challenge_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t response;
    uint32_t device_id;
} auth_response_payload_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  STATIC STATE
 * ═══════════════════════════════════════════════════════════════════════ */

/* Justification: Peer slot array.  Max 2 WROOM peers.  Persists across
 * all API calls.  Written by set/remove/load, read by get/check/send.
 * File scope, single-task access (comm task on core 0). */
static pairing_peer_info_t s_peers[ESPNOW_MAX_WROOM_PEERS];

/* Justification: Pending challenge values for each peer slot.
 * Stored when challenge is sent, compared when response arrives.
 * Written by send_challenge, read by handle_auth. */
static uint32_t s_pending_challenge[ESPNOW_MAX_WROOM_PEERS];

/* Justification: XTEA key from compile-time config.  Const, flash-resident. */
static const uint32_t s_xtea_key[4] = {
    XTEA_KEY_0, XTEA_KEY_1, XTEA_KEY_2, XTEA_KEY_3
};

/* Justification: Init guard.  Set once in pairing_init(). */
static bool s_initialized = false;

/* Justification: NVS key strings per slot.  Avoids runtime snprintf.
 * Const, flash-resident. */
static const char *s_nvs_mac_keys[ESPNOW_MAX_WROOM_PEERS] = {
    NVS_KEY_WROOM_MAC_0,
    NVS_KEY_WROOM_MAC_1
};

/* ═══════════════════════════════════════════════════════════════════════
 *  STATIC HELPERS
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Find peer slot index by MAC address.
 * @return 0 or 1 if found, -1 if not found.
 */
static int find_peer_by_mac(const uint8_t mac[6])
{
    for (int i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        if (s_peers[i].mac_valid &&
            memcmp(s_peers[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief  Check if a MAC is all zeros (empty/invalid).
 */
static bool is_mac_zero(const uint8_t mac[6])
{
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) { return false; }
    }
    return true;
}

/**
 * @brief  RX handler wrapper for MSG_AUTH_RESPONSE.
 *
 * Matches espnow_rx_handler_t signature so it can be registered
 * with espnow_link_register_handler().
 */
static void auth_response_rx_handler(const uint8_t *src_mac,
                                     const uint8_t *payload,
                                     uint8_t payload_len)
{
    pairing_handle_auth(src_mac, payload, payload_len, MSG_AUTH_RESPONSE);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

error_code_t pairing_init(void)
{
    if (!espnow_link_is_ready()) {
        ESP_LOGE(TAG, "espnow_link not ready — init pairing after link");
        return ERR_NOT_INITIALIZED;
    }

    /* Clear all slots. */
    memset(s_peers, 0, sizeof(s_peers));
    memset(s_pending_challenge, 0, sizeof(s_pending_challenge));

    /* Load from NVS.  Not finding peers is normal on first boot. */
    error_code_t load_err = pairing_load_from_nvs();
    if (load_err == ERR_OK) {
        ESP_LOGI(TAG, "Loaded %u peer(s) from NVS",
                 pairing_get_peer_count());
    } else {
        ESP_LOGI(TAG, "No saved peers — starting fresh");
    }

    /* Register handler for auth responses from WROOMs. */
    espnow_link_register_handler(MSG_AUTH_RESPONSE,
                                 auth_response_rx_handler);

    s_initialized = true;
    ESP_LOGI(TAG, "Pairing initialised — %u peer(s)",
             pairing_get_peer_count());
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

uint8_t pairing_get_peer_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        if (s_peers[i].mac_valid) { count++; }
    }
    return count;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t pairing_get_peer(uint8_t index, pairing_peer_info_t *out)
{
    if (out == NULL)                      { return ERR_NULL_POINTER; }
    if (index >= ESPNOW_MAX_WROOM_PEERS) { return ERR_INVALID_ARG; }

    *out = s_peers[index];
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t pairing_set_peer_mac(uint8_t index, const uint8_t mac[6])
{
    if (mac == NULL)                      { return ERR_NULL_POINTER; }
    if (index >= ESPNOW_MAX_WROOM_PEERS) { return ERR_INVALID_ARG; }
    if (is_mac_zero(mac))                { return ERR_INVALID_ARG; }

    /* Remove old peer from ESP-NOW if slot was occupied. */
    if (s_peers[index].mac_valid) {
        espnow_link_remove_peer(s_peers[index].mac);
    }

    /* Populate slot. */
    memcpy(s_peers[index].mac, mac, 6);
    s_peers[index].mac_valid     = true;
    s_peers[index].status        = WROOM_OFFLINE;
    s_peers[index].authenticated = false;
    s_peers[index].last_seen_ms  = 0;
    s_peers[index].packets_rx    = 0;
    s_peers[index].packets_tx    = 0;

    /* Add to ESP-NOW. */
    error_code_t err = espnow_link_add_peer(mac, ESPNOW_WIFI_CHANNEL);
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "Failed to add peer %u to ESP-NOW", index);
    }

    ESP_LOGI(TAG, "Peer %u set: %02X:%02X:%02X:%02X:%02X:%02X",
             index, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t pairing_remove_peer(uint8_t index)
{
    if (index >= ESPNOW_MAX_WROOM_PEERS) { return ERR_INVALID_ARG; }

    if (s_peers[index].mac_valid) {
        espnow_link_remove_peer(s_peers[index].mac);
    }

    memset(&s_peers[index], 0, sizeof(pairing_peer_info_t));

    ESP_LOGI(TAG, "Peer %u removed", index);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t pairing_save_to_nvs(void)
{
    error_code_t err;
    uint8_t count = pairing_get_peer_count();

    err = nvs_config_write_u8(NVS_KEY_WROOM_COUNT, count);
    if (err != ERR_OK) { return ERR_NVS_WRITE_FAILED; }

    for (int i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        if (s_peers[i].mac_valid) {
            err = nvs_config_write_blob(s_nvs_mac_keys[i],
                                        s_peers[i].mac, 6);
            if (err != ERR_OK) {
                ESP_LOGE(TAG, "Failed to save peer %d MAC", i);
                return ERR_NVS_WRITE_FAILED;
            }
        } else {
            nvs_config_erase_key(s_nvs_mac_keys[i]);
        }
    }

    ESP_LOGI(TAG, "Saved %u peer(s) to NVS", count);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t pairing_load_from_nvs(void)
{
    uint8_t count = 0;
    error_code_t err = nvs_config_read_u8(NVS_KEY_WROOM_COUNT, &count);
    if (err != ERR_OK) { return ERR_NVS_NOT_FOUND; }

    for (int i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        uint8_t mac[6];
        size_t mac_size = 6;
        err = nvs_config_read_blob(s_nvs_mac_keys[i], mac, &mac_size);

        if (err == ERR_OK && mac_size == 6 && !is_mac_zero(mac)) {
            memcpy(s_peers[i].mac, mac, 6);
            s_peers[i].mac_valid     = true;
            s_peers[i].status        = WROOM_OFFLINE;
            s_peers[i].authenticated = false;
            s_peers[i].last_seen_ms  = 0;
            s_peers[i].packets_rx    = 0;
            s_peers[i].packets_tx    = 0;

            espnow_link_add_peer(mac, ESPNOW_WIFI_CHANNEL);

            ESP_LOGI(TAG, "NVS peer %d: %02X:%02X:%02X:%02X:%02X:%02X",
                     i, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            memset(&s_peers[i], 0, sizeof(pairing_peer_info_t));
        }
    }

    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

bool pairing_is_peer_authenticated(uint8_t index)
{
    if (index >= ESPNOW_MAX_WROOM_PEERS) { return false; }
    return s_peers[index].authenticated;
}

/* ───────────────────────────────────────────────────────────────────── */

wroom_peer_status_t pairing_get_peer_status(uint8_t index)
{
    if (index >= ESPNOW_MAX_WROOM_PEERS) { return WROOM_UNKNOWN; }
    return s_peers[index].status;
}

/* ───────────────────────────────────────────────────────────────────── */

void pairing_update_peer_seen(const uint8_t mac[6])
{
    if (mac == NULL) { return; }

    int idx = find_peer_by_mac(mac);
    if (idx < 0) { return; }

    uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    s_peers[idx].last_seen_ms = now;
    s_peers[idx].packets_rx++;

    /* First packet from offline peer — initiate auth. */
    if (s_peers[idx].status == WROOM_OFFLINE) {
        s_peers[idx].status = WROOM_AUTHENTICATING;
        pairing_send_challenge((uint8_t)idx);
    }
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t pairing_send_challenge(uint8_t index)
{
    if (index >= ESPNOW_MAX_WROOM_PEERS) { return ERR_INVALID_ARG; }
    if (!s_peers[index].mac_valid)       { return ERR_INVALID_ARG; }
    if (!espnow_link_is_ready())         { return ERR_NOT_INITIALIZED; }

    uint32_t challenge = esp_random();
    s_pending_challenge[index] = challenge;

    auth_challenge_payload_t pld;
    pld.challenge = challenge;
    pld.device_id = COMM_DEVICE_TOKEN;

    error_code_t err = espnow_link_send(
        s_peers[index].mac,
        MSG_AUTH_CHALLENGE,
        (const uint8_t *)&pld,
        sizeof(pld)
    );

    if (err == ERR_OK) {
        s_peers[index].packets_tx++;
        ESP_LOGI(TAG, "Auth challenge sent to peer %u", index);
    } else {
        ESP_LOGW(TAG, "Failed to send challenge to peer %u", index);
    }

    return err;
}

/* ───────────────────────────────────────────────────────────────────── */

void pairing_handle_auth(const uint8_t *src_mac,
                         const uint8_t *payload,
                         uint8_t payload_len,
                         uint8_t msg_type)
{
    if (src_mac == NULL || payload == NULL) { return; }

    int idx = find_peer_by_mac(src_mac);
    if (idx < 0) {
        ESP_LOGW(TAG, "Auth from unknown MAC — ignored");
        return;
    }

    if (msg_type == MSG_AUTH_RESPONSE) {
        if (payload_len < sizeof(auth_response_payload_t)) {
            ESP_LOGW(TAG, "Auth response too short (%u bytes)",
                     payload_len);
            return;
        }

        const auth_response_payload_t *resp =
            (const auth_response_payload_t *)payload;

        /* Expected: XTEA_encrypt({challenge, 0}, key)[0]. */
        uint32_t block[2];
        block[0] = s_pending_challenge[idx];
        block[1] = 0;
        espnow_xtea_encrypt(block, s_xtea_key);

        if (resp->response == block[0]) {
            s_peers[idx].authenticated = true;
            s_peers[idx].status        = WROOM_ONLINE;
            ESP_LOGI(TAG, "Peer %d AUTHENTICATED", idx);
        } else {
            s_peers[idx].authenticated = false;
            s_peers[idx].status        = WROOM_OFFLINE;
            ESP_LOGW(TAG, "Peer %d auth FAILED — bad response", idx);
        }
    }
}

/* ───────────────────────────────────────────────────────────────────── */

void pairing_check_timeouts(void)
{
    uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    for (int i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        if (!s_peers[i].mac_valid)                { continue; }
        if (s_peers[i].status == WROOM_OFFLINE)   { continue; }
        if (s_peers[i].last_seen_ms == 0)         { continue; }

        uint32_t age = now - s_peers[i].last_seen_ms;
        if (age > ESPNOW_PEER_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Peer %d timed out (%lu ms)",
                     i, (unsigned long)age);
            s_peers[i].status        = WROOM_OFFLINE;
            s_peers[i].authenticated = false;
        }
    }
}