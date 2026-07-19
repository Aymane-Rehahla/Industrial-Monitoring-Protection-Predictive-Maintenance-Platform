/**
 * @file    command_link.c
 * @brief   Builds CMD packets, sends to S3 boards.
 * @version 1.0.0
 */

#include "comm/command_link.h"
#include "comm/peer_registry.h"
#include "esp_log.h"

static const char *TAG = "cmd_link";

error_code_t command_link_init(void)
{
    ESP_LOGI(TAG, "Command link ready");
    return ERR_OK;
}

error_code_t command_link_send_to_peer(uint8_t peer_idx, uint8_t cmd_id, uint8_t param)
{
    s3_peer_info_t info;
    if (peer_registry_get_info((s3_peer_id_t)peer_idx, &info) != ERR_OK) return ERR_INVALID_ARG;
    if (!info.mac_valid) return ERR_INVALID_ARG;

    comm_cmd_payload_t pld;
    pld.cmd_id = cmd_id;
    pld.param  = param;

    error_code_t err = espnow_link_send(info.mac, MSG_CMD,
                                        (const uint8_t *)&pld, sizeof(pld));
    ESP_LOGI(TAG, "CMD 0x%02X → S3-%c: %s", cmd_id,
             (peer_idx == 0) ? 'A' : 'B',
             (err == ERR_OK) ? "OK" : "FAIL");
    return err;
}

error_code_t command_link_send_to_all(uint8_t cmd_id, uint8_t param)
{
    error_code_t r1 = command_link_send_to_peer(0, cmd_id, param);
    error_code_t r2 = command_link_send_to_peer(1, cmd_id, param);
    return (r1 == ERR_OK || r2 == ERR_OK) ? ERR_OK : r1;
}