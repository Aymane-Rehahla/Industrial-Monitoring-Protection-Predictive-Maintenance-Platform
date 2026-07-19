/**
 * @file    command_link.h
 * @brief   Send commands to S3 boards via ESP-NOW.
 * @version 1.0.0
 */

#ifndef COMMAND_LINK_H
#define COMMAND_LINK_H

#include "app_config.h"
#include "comm/espnow_link.h"
#include <stdint.h>

error_code_t command_link_init(void);
error_code_t command_link_send_to_all(uint8_t cmd_id, uint8_t param);
error_code_t command_link_send_to_peer(uint8_t peer_idx, uint8_t cmd_id, uint8_t param);

#endif /* COMMAND_LINK_H */