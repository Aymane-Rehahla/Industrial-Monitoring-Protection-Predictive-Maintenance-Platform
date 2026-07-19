/**
 * @file    telemetry.c
 * @brief   Telemetry scheduler — periodic sensor data + immediate faults.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — Observability only.
 *
 * ARCHITECTURE:
 *   Single FreeRTOS task on Core 0, tick period = 200 ms (5 Hz base).
 *   Every tick: send fast telemetry.
 *   Every 5th tick: send slow telemetry + check peer timeouts.
 *   Every 2nd tick: send heartbeat.
 *   Fault events are sent immediately from any calling context via
 *   a static buffer protected by a FreeRTOS mutex.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "comm/telemetry.h"
#include "comm/espnow_link.h"
#include "comm/pairing.h"
#include "core/measurement/measurement.h"
#include "core/system_status.h"
#include "core/protection/fault_handler.h"
#include "core/redundancy/redundancy.h"
#include "core/config_manager.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"

#include <string.h>

static const char *TAG = "telemetry";

/* ═══════════════════════════════════════════════════════════════════════
 *  STATIC STATE
 * ═══════════════════════════════════════════════════════════════════════ */

/* Justification: Task handle for stop/cleanup.  File scope. */
static TaskHandle_t s_task_handle = NULL;

/* Justification: Running flag — controls whether task sends or idles.
 * Written by start/stop, read by task loop. */
static volatile bool s_running = false;

/* Justification: Init guard. */
static bool s_initialized = false;

/* Justification: Statistics counters.  Written by task + fault sender.
 * Individual uint32 increments are atomic on Xtensa. */
static telemetry_stats_t s_stats;

/* Justification: Mutex for fault event send — called from protection
 * engine on core 1, must not collide with telemetry task on core 0
 * which also uses espnow_link_send(). */
static SemaphoreHandle_t s_fault_mutex = NULL;

/* Justification: Heartbeat sequence counter.  Only used by task. */
static uint16_t s_hb_sequence = 0;

/* ═══════════════════════════════════════════════════════════════════════
 *  QUALITY FLAG BITS
 * ═══════════════════════════════════════════════════════════════════════ */
#define QFLAG_V_L1    (1u << 0)
#define QFLAG_V_L2    (1u << 1)
#define QFLAG_V_L3    (1u << 2)
#define QFLAG_I_L1    (1u << 3)
#define QFLAG_I_L2    (1u << 4)
#define QFLAG_I_L3    (1u << 5)
#define QFLAG_RPM     (1u << 6)
#define QFLAG_VIB_X   (1u << 7)
#define QFLAG_VIB_Y   (1u << 8)

/* ═══════════════════════════════════════════════════════════════════════
 *  FLOAT → INT SCALING
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Scale float to int16 with given multiplier, clamped.
 *
 * WHY clamp: Prevents overflow on corrupted sensor readings.
 * int16 range: -32768 to +32767.  With ×100 scale that covers
 * -327.68 to +327.67 which is adequate for voltage/current.
 */
static inline int16_t float_to_i16(float val, float scale)
{
    float scaled = val * scale;
    if (scaled > 32767.0f)  { return 32767; }
    if (scaled < -32768.0f) { return -32768; }
    return (int16_t)scaled;
}

static inline uint16_t float_to_u16(float val)
{
    if (val < 0.0f)      { return 0; }
    if (val > 65535.0f)   { return 65535; }
    return (uint16_t)val;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  SEND TO ALL AUTHENTICATED PEERS
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Send a packet to every authenticated WROOM peer.
 *
 * @param  msg_type     Message type enum.
 * @param  payload      Payload bytes.
 * @param  payload_len  Payload length.
 * @return Number of peers successfully sent to.
 */
static uint8_t send_to_all_peers(uint8_t msg_type,
                                 const uint8_t *payload,
                                 uint8_t payload_len)
{
    uint8_t sent = 0;

    for (uint8_t i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        if (!pairing_is_peer_authenticated(i)) { continue; }

        pairing_peer_info_t info;
        if (pairing_get_peer(i, &info) != ERR_OK) { continue; }
        if (!info.mac_valid) { continue; }

        error_code_t err = espnow_link_send(info.mac, msg_type,
                                            payload, payload_len);
        if (err == ERR_OK) {
            sent++;
        } else {
            s_stats.send_errors++;
        }
    }

    return sent;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  BUILD + SEND: FAST TELEMETRY (5 Hz)
 * ═══════════════════════════════════════════════════════════════════════ */

static void send_fast_telemetry(void)
{
    telem_fast_payload_t pld;
    memset(&pld, 0, sizeof(pld));

    uint16_t flags = 0;

    /* ── Voltage (3-phase) ── */
    three_phase_reading_t voltage;
    if (measurement_get_voltage(&voltage) == ERR_OK) {
        pld.voltage_L1_x100 = float_to_i16(voltage.L1.scaled_value, 100.0f);
        pld.voltage_L2_x100 = float_to_i16(voltage.L2.scaled_value, 100.0f);
        pld.voltage_L3_x100 = float_to_i16(voltage.L3.scaled_value, 100.0f);
        if (voltage.L1.is_valid) { flags |= QFLAG_V_L1; }
        if (voltage.L2.is_valid) { flags |= QFLAG_V_L2; }
        if (voltage.L3.is_valid) { flags |= QFLAG_V_L3; }
    }

    /* ── Current (3-phase) ── */
    three_phase_reading_t current;
    if (measurement_get_current(&current) == ERR_OK) {
        pld.current_L1_x100 = float_to_i16(current.L1.scaled_value, 100.0f);
        pld.current_L2_x100 = float_to_i16(current.L2.scaled_value, 100.0f);
        pld.current_L3_x100 = float_to_i16(current.L3.scaled_value, 100.0f);
        if (current.L1.is_valid) { flags |= QFLAG_I_L1; }
        if (current.L2.is_valid) { flags |= QFLAG_I_L2; }
        if (current.L3.is_valid) { flags |= QFLAG_I_L3; }
    }

    /* ── RPM ── */
    sensor_reading_t rpm;
    if (measurement_get_rpm(&rpm) == ERR_OK) {
        pld.rpm = float_to_u16(rpm.scaled_value);
        if (rpm.is_valid) { flags |= QFLAG_RPM; }
    }

    /* ── Vibration ── */
    sensor_reading_t vib_x, vib_y;
    if (measurement_get_vibration(&vib_x, &vib_y) == ERR_OK) {
        pld.vibration_x_x1000 = float_to_i16(vib_x.scaled_value, 1000.0f);
        pld.vibration_y_x1000 = float_to_i16(vib_y.scaled_value, 1000.0f);
        if (vib_x.is_valid) { flags |= QFLAG_VIB_X; }
        if (vib_y.is_valid) { flags |= QFLAG_VIB_Y; }
    }

    pld.quality_flags = flags;

    send_to_all_peers(MSG_TELEM_FAST, (const uint8_t *)&pld, sizeof(pld));
    s_stats.fast_packets_sent++;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  BUILD + SEND: SLOW TELEMETRY (1 Hz)
 * ═══════════════════════════════════════════════════════════════════════ */

static void send_slow_telemetry(void)
{
    telem_slow_payload_t pld;
    memset(&pld, 0, sizeof(pld));

    /* ── Temperature & Humidity ── */
    sensor_reading_t temp, hum;
    if (measurement_get_temperature(&temp) == ERR_OK) {
        pld.temp_x100 = float_to_i16(temp.scaled_value, 100.0f);
    }
    if (measurement_get_humidity(&hum) == ERR_OK) {
        pld.humidity_x100 = float_to_i16(hum.scaled_value, 100.0f);
    }

    /* ── Gas sensors ── */
    sensor_reading_t gas;
    if (measurement_get_gas(SENSOR_GAS_SMOKE, &gas) == ERR_OK) {
        pld.gas_smoke_ppm = float_to_u16(gas.scaled_value);
    }
    if (measurement_get_gas(SENSOR_GAS_METHANE, &gas) == ERR_OK) {
        pld.gas_methane_ppm = float_to_u16(gas.scaled_value);
    }
    if (measurement_get_gas(SENSOR_GAS_CO, &gas) == ERR_OK) {
        pld.gas_co_ppm = float_to_u16(gas.scaled_value);
    }

    /* ── System status ── */
    system_snapshot_t snap;
    if (system_status_get_snapshot(&snap) == ERR_OK) {
        pld.system_state    = (uint8_t)snap.state;
        pld.relay_commanded = snap.relay_commanded ? 1 : 0;
        pld.relay_confirmed = snap.relay_confirmed ? 1 : 0;
        pld.uptime_seconds  = snap.uptime_seconds;
    }

    pld.security_mode = (uint8_t)system_status_get_security_mode();

    /* ── Active faults ── */
    uint32_t fault_count = 0;
    fault_handler_get_count(&fault_count);
    pld.active_fault_count = (fault_count > 8) ? 8 : (uint8_t)fault_count;

    for (uint8_t i = 0; i < pld.active_fault_count; i++) {
        fault_entry_t entry;
        if (fault_handler_get_entry(i, &entry) == ERR_OK) {
            pld.active_faults[i] = (uint8_t)entry.code;
        }
    }

    /* ── Peer S3 status ── */
    pld.peer_s3_status = (uint8_t)redundancy_get_peer_status();

    /* ── Gas warmup ── */
    measurement_snapshot_t msnap;
    if (measurement_get_snapshot(&msnap) == ERR_OK) {
        pld.gas_warmed_up = msnap.gas_warmed_up ? 1 : 0;
    }

    send_to_all_peers(MSG_TELEM_SLOW, (const uint8_t *)&pld, sizeof(pld));
    s_stats.slow_packets_sent++;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  BUILD + SEND: HEARTBEAT (2 Hz)
 * ═══════════════════════════════════════════════════════════════════════ */

static void send_heartbeat(void)
{
    telem_heartbeat_payload_t pld;
    pld.sequence = s_hb_sequence++;
    pld.padding  = 0;
    pld.uptime_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    send_to_all_peers(MSG_HEARTBEAT, (const uint8_t *)&pld, sizeof(pld));
    s_stats.heartbeat_packets_sent++;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  COMMAND HANDLER (WROOM → S3)
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Handle incoming command from WROOM.
 *
 * Registered with espnow_link as handler for MSG_CMD.
 * Validates sender is authenticated, executes command, sends ACK.
 */
static void cmd_rx_handler(const uint8_t *src_mac,
                           const uint8_t *payload,
                           uint8_t payload_len)
{
    if (src_mac == NULL || payload == NULL) { return; }
    if (payload_len < sizeof(comm_cmd_payload_t)) { return; }

    /* Update peer liveness. */
    pairing_update_peer_seen(src_mac);

    /* Verify sender is authenticated. */
    pairing_peer_info_t info;
    bool authenticated = false;
    for (uint8_t i = 0; i < ESPNOW_MAX_WROOM_PEERS; i++) {
        if (pairing_get_peer(i, &info) == ERR_OK &&
            info.mac_valid &&
            memcmp(info.mac, src_mac, 6) == 0 &&
            info.authenticated) {
            authenticated = true;
            break;
        }
    }

    if (!authenticated) {
        ESP_LOGW(TAG, "CMD from unauthenticated peer — rejected");
        return;
    }

    const comm_cmd_payload_t *cmd = (const comm_cmd_payload_t *)payload;
    uint8_t result = 0;

    s_stats.cmd_packets_received++;

    ESP_LOGI(TAG, "CMD received: id=0x%02X param=0x%02X",
             cmd->cmd_id, cmd->param);

    switch (cmd->cmd_id) {
        case CMD_ACK_ALARM:
            fault_handler_acknowledge_all();
            ESP_LOGI(TAG, "CMD: alarms acknowledged");
            break;

        case CMD_RESET_FAULTS:
            fault_handler_clear_forgivable();
            ESP_LOGI(TAG, "CMD: forgivable faults cleared");
            break;

        case CMD_REBOOT:
            ESP_LOGW(TAG, "CMD: reboot requested — rebooting in 1s");
            /* Send ACK before reboot. */
            {
                comm_cmd_ack_payload_t ack;
                ack.cmd_id   = cmd->cmd_id;
                ack.result   = 0;
                ack.reserved = 0;
                espnow_link_send(src_mac, MSG_CMD_ACK,
                                 (const uint8_t *)&ack, sizeof(ack));
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
            return;  /* Never reached. */

        case CMD_REQUEST_CONFIG: {
            /* Send current thresholds back as MSG_CONFIG_DATA. */
            protection_config_t cfg;
            if (config_manager_get_config(&cfg) == ERR_OK) {
                espnow_link_send(src_mac, MSG_CONFIG_DATA,
                                 (const uint8_t *)&cfg, sizeof(cfg));
                ESP_LOGI(TAG, "CMD: config sent");
            } else {
                result = (uint8_t)(-ERR_NOT_INITIALIZED);
            }
            break;
        }

        case CMD_EDIT_THRESHOLD: {
            /* param = sensor_type_t.  Threshold data follows cmd. */
            if (payload_len < sizeof(comm_cmd_payload_t)
                              + sizeof(sensor_threshold_t)) {
                result = (uint8_t)(-ERR_INVALID_ARG);
                ESP_LOGW(TAG, "CMD: edit threshold — payload too short");
                break;
            }

            const sensor_threshold_t *new_thresh =
                (const sensor_threshold_t *)(payload
                                             + sizeof(comm_cmd_payload_t));
            error_code_t err = config_manager_set_threshold(
                (sensor_type_t)cmd->param, new_thresh
            );

            if (err == ERR_OK) {
                config_manager_save();
                ESP_LOGI(TAG, "CMD: threshold updated for sensor %u",
                         cmd->param);
            } else {
                result = (uint8_t)(-err);
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "CMD: unknown cmd_id 0x%02X", cmd->cmd_id);
            result = 0xFF;
            break;
    }

    /* Send ACK. */
    comm_cmd_ack_payload_t ack;
    ack.cmd_id   = cmd->cmd_id;
    ack.result   = result;
    ack.reserved = 0;
    espnow_link_send(src_mac, MSG_CMD_ACK,
                     (const uint8_t *)&ack, sizeof(ack));
}

/**
 * @brief  Handle any received packet from a known peer — update liveness.
 *
 * Registered as handler for MSG_HEARTBEAT from WROOM side.
 * Also updates peer_seen for liveness tracking.
 */
static void heartbeat_rx_handler(const uint8_t *src_mac,
                                 const uint8_t *payload,
                                 uint8_t payload_len)
{
    UNUSED(payload);
    UNUSED(payload_len);

    if (src_mac != NULL) {
        pairing_update_peer_seen(src_mac);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  TELEMETRY TASK
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Main telemetry task.
 *
 * Base tick = 200 ms.  Sub-counters determine which packets to send.
 *
 * Tick schedule:
 *   tick % 1 == 0  →  fast telemetry  (every 200 ms = 5 Hz)
 *   tick % 2 == 0  →  heartbeat       (every 400 ms ≈ 2.5 Hz)
 *   tick % 5 == 0  →  slow telemetry  (every 1000 ms = 1 Hz)
 *                     + peer timeout check
 */
static void telemetry_task(void *arg)
{
    UNUSED(arg);

    uint32_t tick = 0;
    const TickType_t period = pdMS_TO_TICKS(TELEM_FAST_INTERVAL_MS);

    ESP_LOGI(TAG, "Telemetry task running on core %d", xPortGetCoreID());

    while (1) {
        vTaskDelay(period);

        if (!s_running) { continue; }

        /* Fast telemetry — every tick (5 Hz). */
        send_fast_telemetry();

        /* Heartbeat — every 2nd tick (~2.5 Hz). */
        if ((tick % 2) == 0) {
            send_heartbeat();
        }

        /* Slow telemetry + housekeeping — every 5th tick (1 Hz). */
        if ((tick % 5) == 0) {
            send_slow_telemetry();
            pairing_check_timeouts();
        }

        tick++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

error_code_t telemetry_init(void)
{
    if (s_initialized) { return ERR_ALREADY_INITIALIZED; }

    if (!espnow_link_is_ready()) {
        ESP_LOGE(TAG, "espnow_link not ready");
        return ERR_NOT_INITIALIZED;
    }

    /* Create fault event mutex. */
    s_fault_mutex = xSemaphoreCreateMutex();
    if (s_fault_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create fault mutex");
        return ERR_HW_INIT_FAILED;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    s_running = false;
    s_hb_sequence = 0;

    /* Register RX handlers. */
    espnow_link_register_handler(MSG_CMD, cmd_rx_handler);
    espnow_link_register_handler(MSG_HEARTBEAT, heartbeat_rx_handler);

    /* Create telemetry task on core 0 (comm core). */
    BaseType_t ret = xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry",
        TASK_STACK_TELEMETRY,
        NULL,
        TASK_PRIO_TELEMETRY,
        &s_task_handle,
        TASK_CORE_COMMS
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry task");
        vSemaphoreDelete(s_fault_mutex);
        s_fault_mutex = NULL;
        return ERR_HW_INIT_FAILED;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Telemetry initialised — task created, awaiting start");
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t telemetry_start(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    s_running = true;
    ESP_LOGI(TAG, "Telemetry STARTED — fast=%dHz slow=%dHz",
             1000 / TELEM_FAST_INTERVAL_MS,
             1000 / TELEM_SLOW_INTERVAL_MS);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t telemetry_stop(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    s_running = false;
    ESP_LOGI(TAG, "Telemetry STOPPED");
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t telemetry_send_fault_event(fault_code_t code,
                                        severity_t severity,
                                        float value,
                                        float threshold)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    telem_fault_payload_t pld;
    pld.fault_code         = (uint8_t)code;
    pld.severity           = (uint8_t)severity;
    pld.trigger_value_x100 = float_to_i16(value, 100.0f);
    pld.threshold_x100     = float_to_i16(threshold, 100.0f);
    pld.timestamp_ms       = (uint32_t)(xTaskGetTickCount()
                                        * portTICK_PERIOD_MS);

    /* Mutex protects against concurrent send from telemetry task. */
    if (xSemaphoreTake(s_fault_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        s_stats.send_errors++;
        return ERR_TIMEOUT;
    }

    send_to_all_peers(MSG_FAULT_EVENT, (const uint8_t *)&pld, sizeof(pld));
    s_stats.fault_packets_sent++;

    xSemaphoreGive(s_fault_mutex);

    ESP_LOGW(TAG, "FAULT EVENT sent: code=%u sev=%u val=%.2f thresh=%.2f",
             (unsigned)code, (unsigned)severity, value, threshold);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t telemetry_get_stats(telemetry_stats_t *stats_out)
{
    if (stats_out == NULL) { return ERR_NULL_POINTER; }

    *stats_out = s_stats;
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

bool telemetry_is_running(void)
{
    return s_running;
}