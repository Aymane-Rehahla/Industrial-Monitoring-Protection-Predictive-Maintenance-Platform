/**
 * @file    peer_registry.h
 * @brief   Tracks S3-A and S3-B peer liveness and packet stats.
 * @version 1.0.0
 */

#ifndef PEER_REGISTRY_H
#define PEER_REGISTRY_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    S3_PEER_A = 0,
    S3_PEER_B = 1
} s3_peer_id_t;

typedef struct {
    uint8_t  mac[6];
    bool     mac_valid;
    bool     online;
    uint32_t last_seen_ms;
    uint32_t packets_rx;
    uint16_t last_sequence;
} s3_peer_info_t;

error_code_t peer_registry_init(void);
void peer_registry_update_seen(const uint8_t mac[6], uint16_t sequence);
bool peer_registry_is_online(s3_peer_id_t id);
error_code_t peer_registry_get_info(s3_peer_id_t id, s3_peer_info_t *out);
int peer_registry_find_by_mac(const uint8_t mac[6]);
void peer_registry_check_timeouts(void);
uint8_t peer_registry_online_count(void);

#endif /* PEER_REGISTRY_H */