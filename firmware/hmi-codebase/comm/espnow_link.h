/**
 * @file    espnow_link.h
 * @brief   ESP-NOW transport for WROOM — APSTA mode, receives from S3 boards.
 * @version 1.0.0
 */

#ifndef ESPNOW_LINK_H
#define ESPNOW_LINK_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Packet structures — MUST match S3 firmware ── */

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  version;
    uint8_t  msg_type;
    uint8_t  src_role;
    uint16_t sequence;
    uint8_t  payload_len;
    uint8_t  node_id;
} comm_header_t;

typedef struct __attribute__((packed)) {
    uint16_t crc16;
} comm_footer_t;

#define COMM_MAX_PAYLOAD_SIZE  (250 - sizeof(comm_header_t) - sizeof(comm_footer_t))

/* ── Message types — MUST match S3 firmware ── */

typedef enum {
    MSG_TELEM_FAST     = 0x01,
    MSG_TELEM_SLOW     = 0x02,
    MSG_FAULT_EVENT    = 0x03,
    MSG_HEARTBEAT      = 0x04,
    MSG_CMD            = 0x05,
    MSG_CMD_ACK        = 0x06,
    MSG_AUTH_CHALLENGE  = 0x10,
    MSG_AUTH_RESPONSE  = 0x11,
    MSG_CONFIG_DATA    = 0x12
} comm_msg_type_t;

/* ── Command IDs — MUST match S3 firmware ── */

typedef enum {
    CMD_ACK_ALARM      = 0x01,
    CMD_RESET_FAULTS   = 0x02,
    CMD_REBOOT         = 0x03,
    CMD_REQUEST_CONFIG = 0x04,
    CMD_EDIT_THRESHOLD = 0x05
} comm_cmd_id_t;

/* ── Telemetry payloads — MUST match S3 firmware ── */

typedef struct __attribute__((packed)) {
    int16_t  voltage_L1_x100;
    int16_t  voltage_L2_x100;
    int16_t  voltage_L3_x100;
    int16_t  current_L1_x100;
    int16_t  current_L2_x100;
    int16_t  current_L3_x100;
    uint16_t rpm;
    int16_t  vibration_x_x1000;
    int16_t  vibration_y_x1000;
    uint16_t quality_flags;
} telem_fast_payload_t;

typedef struct __attribute__((packed)) {
    int16_t  temp_x100;
    int16_t  humidity_x100;
    uint16_t gas_smoke_ppm;
    uint16_t gas_methane_ppm;
    uint16_t gas_co_ppm;
    uint8_t  system_state;
    uint8_t  security_mode;
    uint8_t  relay_commanded;
    uint8_t  relay_confirmed;
    uint8_t  active_fault_count;
    uint8_t  active_faults[8];
    uint32_t uptime_seconds;
    uint8_t  peer_s3_status;
    uint8_t  gas_warmed_up;
} telem_slow_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  fault_code;
    uint8_t  severity;
    int16_t  trigger_value_x100;
    int16_t  threshold_x100;
    uint32_t timestamp_ms;
} telem_fault_payload_t;

typedef struct __attribute__((packed)) {
    uint16_t sequence;
    uint16_t padding;
    uint32_t uptime_ms;
} telem_heartbeat_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;
    uint8_t  param;
} comm_cmd_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;
    uint8_t  result;
    uint8_t  reserved;
} comm_cmd_ack_payload_t;

/* ── RX queue item ── */

typedef struct {
    uint8_t  src_mac[6];
    uint8_t  data[250];
    uint8_t  data_len;
} espnow_rx_item_t;

/* ── Link stats ── */

typedef struct {
    uint32_t tx_count;
    uint32_t tx_fail_count;
    uint32_t rx_count;
    uint32_t rx_crc_errors;
    uint32_t rx_dropped;
    uint32_t last_rx_ms;
} espnow_stats_t;

/* ── Handler callback ── */

typedef void (*espnow_rx_handler_t)(const uint8_t *src_mac,
                                    const uint8_t *payload,
                                    uint8_t payload_len);

/* ── XTEA ── */

void espnow_xtea_encrypt(uint32_t v[2], const uint32_t key[4]);
void espnow_xtea_decrypt(uint32_t v[2], const uint32_t key[4]);

/* ── API ── */

error_code_t espnow_link_init(void);
error_code_t espnow_link_add_peer(const uint8_t mac[6], uint8_t channel);
error_code_t espnow_link_remove_peer(const uint8_t mac[6]);
error_code_t espnow_link_send(const uint8_t dest_mac[6], uint8_t msg_type,
                              const uint8_t *payload, uint8_t payload_len);
error_code_t espnow_link_register_handler(uint8_t msg_type,
                                          espnow_rx_handler_t handler);
error_code_t espnow_link_get_stats(espnow_stats_t *out);
bool espnow_link_is_ready(void);

#endif /* ESPNOW_LINK_H */