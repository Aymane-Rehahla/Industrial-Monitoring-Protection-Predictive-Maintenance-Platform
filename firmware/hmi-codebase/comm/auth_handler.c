/**
 * @file    auth_handler.c
 * @brief   Responds to S3 AUTH_CHALLENGE with XTEA-encrypted response.
 * @version 1.0.0
 */

#include "comm/auth_handler.h"
#include "comm/espnow_link.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "auth_handler";

typedef struct __attribute__((packed)) {
    uint32_t challenge;
    uint32_t device_id;
} auth_challenge_t;

typedef struct __attribute__((packed)) {
    uint32_t response;
    uint32_t device_id;
} auth_response_t;

static const uint32_t s_key[4] = { XTEA_KEY_0, XTEA_KEY_1, XTEA_KEY_2, XTEA_KEY_3 };

/**
 * @brief  Handle AUTH_CHALLENGE from S3 — compute response and send back.
 */
static void challenge_rx_handler(const uint8_t *src_mac,
                                 const uint8_t *payload,
                                 uint8_t payload_len)
{
    if (!src_mac || !payload || payload_len < sizeof(auth_challenge_t)) return;

    const auth_challenge_t *ch = (const auth_challenge_t *)payload;

    ESP_LOGI(TAG, "Auth challenge from %02X:%02X:%02X:%02X:%02X:%02X",
             src_mac[0], src_mac[1], src_mac[2],
             src_mac[3], src_mac[4], src_mac[5]);

    /* Encrypt {challenge, 0} with shared key. */
    uint32_t block[2] = { ch->challenge, 0 };
    espnow_xtea_encrypt(block, s_key);

    auth_response_t resp;
    resp.response  = block[0];
    resp.device_id = COMM_DEVICE_TOKEN;

    error_code_t err = espnow_link_send(src_mac, MSG_AUTH_RESPONSE,
                                        (const uint8_t *)&resp, sizeof(resp));
    if (err == ERR_OK) {
        ESP_LOGI(TAG, "Auth response sent");
    } else {
        ESP_LOGW(TAG, "Auth response failed: %d", err);
    }
}

error_code_t auth_handler_init(void)
{
    espnow_link_register_handler(MSG_AUTH_CHALLENGE, challenge_rx_handler);
    ESP_LOGI(TAG, "Auth handler registered");
    return ERR_OK;
}