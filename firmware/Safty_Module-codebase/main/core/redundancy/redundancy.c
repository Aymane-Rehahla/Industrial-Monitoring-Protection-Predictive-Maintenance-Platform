// ═══ FILE: main/core/redundancy/redundancy.c ═══
/**
 * @file    redundancy.c
 * @brief   Redundancy manager — STUB implementation.
 *          Returns "peer online" for HMI development.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Stub only; replace before production.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  STUB release.
 */

#include "core/redundancy/redundancy.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "redundancy";

/* Justification: Peer MAC must persist across calls.  Set once via
 * HMI pairing screen or NVS load. File scope. */
static uint8_t s_peer_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
static bool    s_initialized = false;

/* STUB — returns fake data */
error_code_t redundancy_init(void)
{
    s_initialized = true;
    ESP_LOGW(TAG, "STUB: redundancy_init — fake peer ONLINE");
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t redundancy_get_peer_info(peer_info_t *out)
{
    if (out == NULL) { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    uint32_t up_s = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);

    out->status              = PEER_ONLINE;
    out->last_heartbeat_ms   = (uint32_t)(xTaskGetTickCount()
                                          * portTICK_PERIOD_MS);
    out->heartbeat_age_ms    = 50;
    out->messages_received   = up_s * 10;
    out->messages_failed     = 0;
    out->cross_validation_ok = true;
    memcpy(out->peer_mac, s_peer_mac, 6);

    return ERR_OK;
}

/* STUB — returns fake data */
peer_status_t redundancy_get_peer_status(void)
{
    return PEER_ONLINE;
}

/* STUB — returns fake data */
bool redundancy_is_cross_valid(void)
{
    return true;
}

/* STUB — stores value */
error_code_t redundancy_set_peer_mac(const uint8_t mac[6])
{
    if (mac == NULL) { return ERR_NULL_POINTER; }
    memcpy(s_peer_mac, mac, 6);
    return ERR_OK;
}

/* STUB — returns fake data */
error_code_t redundancy_get_peer_mac(uint8_t mac_out[6])
{
    if (mac_out == NULL) { return ERR_NULL_POINTER; }
    memcpy(mac_out, s_peer_mac, 6);
    return ERR_OK;
}