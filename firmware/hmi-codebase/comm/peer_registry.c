/**
 * @file    peer_registry.c
 * @brief   S3 peer tracking — MAC matching, liveness, timeouts.
 * @version 1.0.0
 */

#include "comm/peer_registry.h"
#include "comm/espnow_link.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "peer_reg";

static s3_peer_info_t s_peers[ESPNOW_MAX_S3_PEERS];

static const uint8_t s_s3a_mac[6] = S3_A_MAC;
static const uint8_t s_s3b_mac[6] = S3_B_MAC;

error_code_t peer_registry_init(void)
{
    memset(s_peers, 0, sizeof(s_peers));

    /* S3-A */
    memcpy(s_peers[S3_PEER_A].mac, s_s3a_mac, 6);
    s_peers[S3_PEER_A].mac_valid = true;
    espnow_link_add_peer(s_s3a_mac, ESPNOW_WIFI_CHANNEL);

    /* S3-B */
    memcpy(s_peers[S3_PEER_B].mac, s_s3b_mac, 6);
    s_peers[S3_PEER_B].mac_valid = true;
    espnow_link_add_peer(s_s3b_mac, ESPNOW_WIFI_CHANNEL);

    ESP_LOGI(TAG, "S3-A: %02X:%02X:%02X:%02X:%02X:%02X",
             s_s3a_mac[0], s_s3a_mac[1], s_s3a_mac[2],
             s_s3a_mac[3], s_s3a_mac[4], s_s3a_mac[5]);
    ESP_LOGI(TAG, "S3-B: %02X:%02X:%02X:%02X:%02X:%02X",
             s_s3b_mac[0], s_s3b_mac[1], s_s3b_mac[2],
             s_s3b_mac[3], s_s3b_mac[4], s_s3b_mac[5]);

    return ERR_OK;
}

int peer_registry_find_by_mac(const uint8_t mac[6])
{
    if (!mac) return -1;
    for (int i = 0; i < ESPNOW_MAX_S3_PEERS; i++) {
        if (s_peers[i].mac_valid && memcmp(s_peers[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

void peer_registry_update_seen(const uint8_t mac[6], uint16_t sequence)
{
    int idx = peer_registry_find_by_mac(mac);
    if (idx < 0) return;

    uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    s_peers[idx].last_seen_ms  = now;
    s_peers[idx].packets_rx++;
    s_peers[idx].last_sequence = sequence;
    s_peers[idx].online        = true;
}

bool peer_registry_is_online(s3_peer_id_t id)
{
    if (id >= ESPNOW_MAX_S3_PEERS) return false;
    return s_peers[id].online;
}

error_code_t peer_registry_get_info(s3_peer_id_t id, s3_peer_info_t *out)
{
    if (!out) return ERR_NULL_POINTER;
    if (id >= ESPNOW_MAX_S3_PEERS) return ERR_INVALID_ARG;
    *out = s_peers[id];
    return ERR_OK;
}

void peer_registry_check_timeouts(void)
{
    uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    for (int i = 0; i < ESPNOW_MAX_S3_PEERS; i++) {
        if (!s_peers[i].mac_valid || !s_peers[i].online) continue;
        if (s_peers[i].last_seen_ms == 0) continue;

        uint32_t age = now - s_peers[i].last_seen_ms;
        if (age > ESPNOW_PEER_TIMEOUT_MS) {
            if (s_peers[i].online) {
                ESP_LOGW(TAG, "S3 peer %d timed out (%lu ms)", i, (unsigned long)age);
                s_peers[i].online = false;
            }
        }
    }
}

uint8_t peer_registry_online_count(void)
{
    uint8_t c = 0;
    for (int i = 0; i < ESPNOW_MAX_S3_PEERS; i++) {
        if (s_peers[i].online) c++;
    }
    return c;
}