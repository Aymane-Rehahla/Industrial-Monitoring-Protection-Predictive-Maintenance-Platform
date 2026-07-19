/**
 * @file    telemetry.h
 * @brief   Telemetry scheduler — builds and sends sensor/status packets
 *          to authenticated WROOM peers via ESP-NOW.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — Observability.  Loss does not affect safety decisions.
 *
 * RATES:
 *   Fast (5 Hz):  voltage, current, RPM, vibration
 *   Slow (1 Hz):  temperature, humidity, gas, faults, system state
 *   Event:        fault trips sent immediately
 *   Heartbeat (2 Hz): alive indicator with sequence + uptime
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  TELEMETRY PAYLOAD STRUCTURES — packed, integer-scaled, no floats
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Fast telemetry — 5 Hz, 20 bytes.
 *
 * All values scaled to fixed-point integers to avoid floating-point
 * representation issues across different MCU architectures.
 */
typedef struct __attribute__((packed)) {
    int16_t  voltage_L1_x100;     /**< V × 100:  220.50 V → 22050      */
    int16_t  voltage_L2_x100;
    int16_t  voltage_L3_x100;
    int16_t  current_L1_x100;     /**< A × 100:  15.25 A → 1525        */
    int16_t  current_L2_x100;
    int16_t  current_L3_x100;
    uint16_t rpm;                 /**< Direct RPM value                 */
    int16_t  vibration_x_x1000;   /**< g × 1000: 2.345 g → 2345        */
    int16_t  vibration_y_x1000;
    uint16_t quality_flags;       /**< Bit 0=V_L1 .. Bit 8=vib_y valid  */
} telem_fast_payload_t;

/**
 * @brief  Slow telemetry — 1 Hz, 28 bytes.
 */
typedef struct __attribute__((packed)) {
    int16_t  temp_x100;           /**< °C × 100: 45.67 → 4567          */
    int16_t  humidity_x100;       /**< %RH × 100: 65.20 → 6520         */
    uint16_t gas_smoke_ppm;       /**< MQ-2 PPM                        */
    uint16_t gas_methane_ppm;     /**< MQ-4 PPM                        */
    uint16_t gas_co_ppm;          /**< MQ-9 PPM                        */
    uint8_t  system_state;        /**< system_state_t                   */
    uint8_t  security_mode;       /**< security_mode_t                  */
    uint8_t  relay_commanded;     /**< 1 = ON                           */
    uint8_t  relay_confirmed;     /**< 1 = hardware confirms ON         */
    uint8_t  active_fault_count;  /**< Number of active faults          */
    uint8_t  active_faults[8];    /**< Up to 8 fault codes              */
    uint32_t uptime_seconds;      /**< System uptime                    */
    uint8_t  peer_s3_status;      /**< Other S3 peer_status_t           */
    uint8_t  gas_warmed_up;       /**< 1 = MQ sensors ready             */
} telem_slow_payload_t;

/**
 * @brief  Fault event — sent immediately on trip, 10 bytes.
 */
typedef struct __attribute__((packed)) {
    uint8_t  fault_code;          /**< fault_code_t                     */
    uint8_t  severity;            /**< severity_t                       */
    int16_t  trigger_value_x100;  /**< Value that caused trip           */
    int16_t  threshold_x100;      /**< Threshold exceeded               */
    uint32_t timestamp_ms;        /**< Uptime when fault occurred       */
} telem_fault_payload_t;

/**
 * @brief  Heartbeat — 2 Hz, 8 bytes.
 */
typedef struct __attribute__((packed)) {
    uint16_t sequence;            /**< Heartbeat sequence number        */
    uint16_t padding;             /**< Alignment                        */
    uint32_t uptime_ms;           /**< System uptime in ms              */
} telem_heartbeat_payload_t;

/**
 * @brief  Command from WROOM, 2 bytes.
 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;              /**< comm_cmd_id_t                    */
    uint8_t  param;               /**< Command-specific parameter       */
} comm_cmd_payload_t;

/**
 * @brief  Command acknowledgement, 3 bytes.
 */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;              /**< Which command was acknowledged    */
    uint8_t  result;              /**< 0=OK, non-zero=error_code_t      */
    uint8_t  reserved;
} comm_cmd_ack_payload_t;

/**
 * @brief  Telemetry statistics.
 */
typedef struct {
    uint32_t fast_packets_sent;
    uint32_t slow_packets_sent;
    uint32_t fault_packets_sent;
    uint32_t heartbeat_packets_sent;
    uint32_t cmd_packets_received;
    uint32_t send_errors;
} telemetry_stats_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Initialise telemetry module and create task.
 *
 * Task runs on Core 0 at TASK_PRIO_TELEMETRY.  Starts in stopped state.
 *
 * @pre    espnow_link_init() and pairing_init() called.
 * @post   Task created, waiting for telemetry_start().
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_HW_INIT_FAILED.
 */
error_code_t telemetry_init(void);

/**
 * @brief  Start periodic telemetry transmission.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 */
error_code_t telemetry_start(void);

/**
 * @brief  Stop periodic telemetry (task stays alive but idle).
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 */
error_code_t telemetry_stop(void);

/**
 * @brief  Send a fault event immediately to all authenticated peers.
 *
 * Thread-safe — can be called from any task (protection engine on core 1).
 *
 * @param  code       Fault code.
 * @param  severity   Severity level.
 * @param  value      Measured value that triggered the fault.
 * @param  threshold  Threshold that was exceeded.
 * @return ERR_OK, ERR_NOT_INITIALIZED.
 */
error_code_t telemetry_send_fault_event(fault_code_t code,
                                        severity_t severity,
                                        float value,
                                        float threshold);

/**
 * @brief  Get telemetry statistics.
 *
 * @param  stats_out  Receives statistics.  Must not be NULL.
 * @return ERR_OK, ERR_NULL_POINTER.
 */
error_code_t telemetry_get_stats(telemetry_stats_t *stats_out);

/**
 * @brief  Check if telemetry is actively sending.
 * @return true if running.
 */
bool telemetry_is_running(void);

#endif /* TELEMETRY_H */