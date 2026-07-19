/**
 * @file    espnow_link.c
 * @brief   ESP-NOW transport — WiFi init, packet framing, CRC, dispatch.
 * @version 1.0.1
 * @date    2025-01-01
 * @safety  MEDIUM — Comm link.  Loss degrades observability, not safety.
 *
 * ARCHITECTURE:
 *   ISR recv callback → copies to FreeRTOS queue → dispatch task reads
 *   queue → validates CRC → calls registered handler.  Zero processing
 *   in ISR context.  Send is synchronous but non-blocking (ESP-IDF
 *   queues internally).
 *
 * CHANGELOG:
 *   1.0.1  2025-01-01  Fixed send_cb signature for ESP-IDF v5.5.3
 *                      (wifi_tx_info_t* instead of uint8_t*).
 *   1.0.0  2025-01-01  Initial release.
 */

#include "comm/espnow_link.h"
#include "app_config.h"
#include "crc_utils.h"
#include "system_status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include <string.h>

static const char *TAG = "espnow_link";

/* ═══════════════════════════════════════════════════════════════════════
 *  CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

/** Maximum number of distinct message types we can register handlers for. */
#define MAX_MSG_TYPES  32

/** XTEA delta constant. */
#define XTEA_DELTA     0x9E3779B9u

/* ═══════════════════════════════════════════════════════════════════════
 *  STATIC STATE
 * ═══════════════════════════════════════════════════════════════════════ */

/* Justification: Module init flag.  Set once in init, read by all API
 * functions.  File scope, single writer. */
static bool s_initialized = false;

/* Justification: RX queue — ISR callback writes, dispatch task reads.
 * Must persist across calls.  Created once in init. */
static QueueHandle_t s_rx_queue = NULL;

/* Justification: Dispatch task handle for cleanup in deinit. */
static TaskHandle_t s_rx_task_handle = NULL;

/* Justification: Handler table — one callback per message type.
 * Written by register_handler (HMI task at init), read by dispatch task.
 * No concurrent writes after init, so no mutex needed. */
static espnow_rx_handler_t s_handlers[MAX_MSG_TYPES];

/* Justification: Rolling sequence number for outgoing packets.
 * Only written by send() which is called from telemetry task (single writer).
 * If multiple tasks call send(), wrap increment in critical section. */
static uint16_t s_sequence = 0;

/* Justification: Statistics counters.  Updated from multiple contexts
 * (send from telemetry task, recv from dispatch task, ISR for drops).
 * Individual uint32 increments are atomic on Xtensa, so no mutex needed
 * for counters.  Snapshot read in get_stats copies all at once. */
static espnow_stats_t s_stats;

/* ═══════════════════════════════════════════════════════════════════════
 *  XTEA
 * ═══════════════════════════════════════════════════════════════════════ */

void espnow_xtea_encrypt(uint32_t v[2], const uint32_t key[4])
{
    uint32_t v0 = v[0];
    uint32_t v1 = v[1];
    uint32_t sum = 0;

    for (uint32_t i = 0; i < COMM_AUTH_ROUNDS; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += XTEA_DELTA;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}

void espnow_xtea_decrypt(uint32_t v[2], const uint32_t key[4])
{
    uint32_t v0 = v[0];
    uint32_t v1 = v[1];
    uint32_t sum = XTEA_DELTA * COMM_AUTH_ROUNDS;

    for (uint32_t i = 0; i < COMM_AUTH_ROUNDS; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        sum -= XTEA_DELTA;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }

    v[0] = v0;
    v[1] = v1;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ESP-NOW CALLBACKS (WiFi task context — minimal work)
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  ESP-NOW send completion callback.
 *
 * Called from WiFi task context after each send.
 * Only updates statistics counters.
 *
 * NOTE: ESP-IDF v5.5+ changed the callback signature.
 *       First argument is now `const wifi_tx_info_t *` instead of
 *       `const uint8_t *mac_addr`.
 *
 * @param  tx_info  Transmit information including destination MAC.
 * @param  status   ESP_NOW_SEND_SUCCESS or ESP_NOW_SEND_FAIL.
 */
static void send_cb(const wifi_tx_info_t *tx_info,
                    esp_now_send_status_t status)
{
    UNUSED(tx_info);

    if (status == ESP_NOW_SEND_SUCCESS) {
        s_stats.tx_count++;
        s_stats.last_tx_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    } else {
        s_stats.tx_fail_count++;
    }
}

/**
 * @brief  ESP-NOW receive callback.
 *
 * Called from WiFi task context.  Copies data into queue item and
 * posts to RX queue.  If queue full, drops packet and increments counter.
 * Zero parsing here — all validation happens in dispatch task.
 */
static void recv_cb(const esp_now_recv_info_t *info,
                    const uint8_t *data,
                    int data_len)
{
    if (info == NULL || data == NULL || data_len <= 0 || data_len > 250) {
        return;
    }

    espnow_rx_item_t item;
    memcpy(item.src_mac, info->src_addr, 6);
    memcpy(item.data, data, (size_t)data_len);
    item.data_len = (uint8_t)data_len;

    BaseType_t higher_woken = pdFALSE;
    if (xQueueSendFromISR(s_rx_queue, &item, &higher_woken) != pdTRUE) {
        s_stats.rx_dropped++;
    }

    if (higher_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  RX DISPATCH TASK
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Task that reads RX queue, validates packets, calls handlers.
 *
 * Runs on Core 0 (TASK_CORE_COMMS).  Blocks on queue — zero CPU when
 * no packets arriving.
 *
 * Validation order:
 *   1. Frame length ≥ header + footer
 *   2. Magic byte
 *   3. Protocol version
 *   4. Payload length consistency
 *   5. CRC-16 match
 *   6. Message type has registered handler
 *
 * Any failure → drop, increment appropriate counter, continue.
 */
static void rx_dispatch_task(void *arg)
{
    UNUSED(arg);
    espnow_rx_item_t item;
    const size_t min_frame = sizeof(comm_header_t) + sizeof(comm_footer_t);

    ESP_LOGI(TAG, "RX dispatch task running on core %d", xPortGetCoreID());

    while (1) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 1. Minimum frame size. */
        if (item.data_len < min_frame) {
            s_stats.rx_crc_errors++;
            continue;
        }

        /* 2–3. Parse header, check magic and version. */
        const comm_header_t *hdr = (const comm_header_t *)item.data;

        if (hdr->magic != COMM_PACKET_MAGIC) {
            s_stats.rx_crc_errors++;
            continue;
        }

        if (hdr->version != COMM_PROTOCOL_VERSION) {
            s_stats.rx_crc_errors++;
            ESP_LOGD(TAG, "Version mismatch: got %u", hdr->version);
            continue;
        }

        /* 4. Payload length consistency. */
        size_t expected_len = sizeof(comm_header_t) + hdr->payload_len
                              + sizeof(comm_footer_t);
        if (expected_len != item.data_len) {
            s_stats.rx_crc_errors++;
            continue;
        }

        /* 5. CRC-16 over header + payload. */
        size_t crc_len = sizeof(comm_header_t) + hdr->payload_len;
        uint16_t computed_crc = crc16_calc(item.data, crc_len);

        const comm_footer_t *ftr = (const comm_footer_t *)
                                   (item.data + crc_len);
        if (ftr->crc16 != computed_crc) {
            s_stats.rx_crc_errors++;
            ESP_LOGD(TAG, "CRC mismatch: got 0x%04X expected 0x%04X",
                     ftr->crc16, computed_crc);
            continue;
        }

        /* Packet is valid. */
        s_stats.rx_count++;
        s_stats.last_rx_ms = (uint32_t)(xTaskGetTickCount()
                                        * portTICK_PERIOD_MS);

        /* 6. Dispatch to handler. */
        uint8_t mtype = hdr->msg_type;
        if (mtype < MAX_MSG_TYPES && s_handlers[mtype] != NULL) {
            const uint8_t *payload = item.data + sizeof(comm_header_t);
            s_handlers[mtype](item.src_mac, payload, hdr->payload_len);
        } else {
            ESP_LOGD(TAG, "No handler for msg_type 0x%02X", mtype);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════ */

error_code_t espnow_link_init(void)
{
    if (s_initialized) {
        return ERR_ALREADY_INITIALIZED;
    }

    esp_err_t err;

    /* ── 1. Network interface init ── */
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop_create: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    /* Create default STA netif — required by WiFi driver even though
     * we never connect to an AP.  Stored pointer unused. */
    esp_netif_create_default_wifi_sta();

    /* ── 2. WiFi init — STA mode, no connection ── */
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_storage: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_mode: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_start: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    /* Set channel — must be after wifi_start. */
    err = esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_channel: %s (continuing)", esp_err_to_name(err));
    }

    /* ── 3. ESP-NOW init ── */
    err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    err = esp_now_register_send_cb(send_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_send_cb: %s", esp_err_to_name(err));
        esp_now_deinit();
        return ERR_HW_INIT_FAILED;
    }

    err = esp_now_register_recv_cb(recv_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_recv_cb: %s", esp_err_to_name(err));
        esp_now_deinit();
        return ERR_HW_INIT_FAILED;
    }

    /* ── 4. RX queue and dispatch task ── */
    memset(s_handlers, 0, sizeof(s_handlers));
    memset(&s_stats, 0, sizeof(s_stats));
    s_sequence = 0;

    s_rx_queue = xQueueCreate(ESPNOW_RX_QUEUE_SIZE, sizeof(espnow_rx_item_t));
    if (s_rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        esp_now_deinit();
        return ERR_HW_INIT_FAILED;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        rx_dispatch_task,
        "espnow_rx",
        TASK_STACK_ESPNOW_RX,
        NULL,
        TASK_PRIO_ESPNOW_RX,
        &s_rx_task_handle,
        TASK_CORE_COMMS
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX dispatch task");
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
        esp_now_deinit();
        return ERR_HW_INIT_FAILED;
    }

    s_initialized = true;

    /* Log own MAC for pairing reference. */
    uint8_t own_mac[6];
    esp_read_mac(own_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "ESP-NOW ready — MAC: %02X:%02X:%02X:%02X:%02X:%02X  CH:%d",
             own_mac[0], own_mac[1], own_mac[2],
             own_mac[3], own_mac[4], own_mac[5],
             ESPNOW_WIFI_CHANNEL);

    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t espnow_link_deinit(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    s_initialized = false;

    if (s_rx_task_handle != NULL) {
        vTaskDelete(s_rx_task_handle);
        s_rx_task_handle = NULL;
    }

    if (s_rx_queue != NULL) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }

    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    esp_wifi_stop();

    ESP_LOGI(TAG, "ESP-NOW deinitialised");
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t espnow_link_add_peer(const uint8_t mac[6], uint8_t channel)
{
    if (mac == NULL)    { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    /* Check if peer already exists — not an error, just skip. */
    if (esp_now_is_peer_exist(mac)) {
        ESP_LOGD(TAG, "Peer already exists");
        return ERR_OK;
    }

    esp_now_peer_info_t peer_info;
    memset(&peer_info, 0, sizeof(peer_info));
    memcpy(peer_info.peer_addr, mac, 6);
    peer_info.channel = channel;
    peer_info.ifidx   = ESP_IF_WIFI_STA;
    peer_info.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_peer: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    ESP_LOGI(TAG, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X ch=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], channel);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t espnow_link_remove_peer(const uint8_t mac[6])
{
    if (mac == NULL)    { return ERR_NULL_POINTER; }
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    if (!esp_now_is_peer_exist(mac)) {
        return ERR_NOT_FOUND;
    }

    esp_err_t err = esp_now_del_peer(mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "del_peer: %s", esp_err_to_name(err));
        return ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Peer removed: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t espnow_link_send(const uint8_t dest_mac[6],
                              uint8_t msg_type,
                              const uint8_t *payload,
                              uint8_t payload_len)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }
    if (payload_len > COMM_MAX_PAYLOAD_SIZE) { return ERR_INVALID_ARG; }
    if (payload_len > 0 && payload == NULL)  { return ERR_NULL_POINTER; }

    /* Build frame into static buffer.
     * Justification: Only telemetry task calls send at steady state.
     * If multiple tasks need to send, protect with mutex.  For now
     * single-writer is sufficient — telemetry task on core 0. */
    static uint8_t s_tx_buf[250];

    /* ── Header ── */
    comm_header_t hdr;
    hdr.magic       = COMM_PACKET_MAGIC;
    hdr.version     = COMM_PROTOCOL_VERSION;
    hdr.msg_type    = msg_type;
    hdr.src_role    = (uint8_t)system_status_get_role();
    hdr.sequence    = s_sequence++;
    hdr.payload_len = payload_len;
    hdr.node_id     = (hdr.src_role == ROLE_INFORMER) ? 1 : 2;

    memcpy(s_tx_buf, &hdr, sizeof(hdr));

    /* ── Payload ── */
    if (payload_len > 0) {
        memcpy(s_tx_buf + sizeof(hdr), payload, payload_len);
    }

    /* ── Footer (CRC over header + payload) ── */
    size_t crc_len = sizeof(hdr) + payload_len;
    comm_footer_t ftr;
    ftr.crc16 = crc16_calc(s_tx_buf, crc_len);
    memcpy(s_tx_buf + crc_len, &ftr, sizeof(ftr));

    size_t total_len = crc_len + sizeof(ftr);

    /* ── Send ── */
    esp_err_t err = esp_now_send(dest_mac, s_tx_buf, total_len);
    if (err != ESP_OK) {
        s_stats.tx_fail_count++;
        ESP_LOGD(TAG, "send failed: %s", esp_err_to_name(err));
        return ERR_HW_WRITE_FAILED;
    }

    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t espnow_link_register_handler(uint8_t msg_type,
                                          espnow_rx_handler_t handler)
{
    if (msg_type >= MAX_MSG_TYPES) { return ERR_INVALID_ARG; }

    s_handlers[msg_type] = handler;
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

error_code_t espnow_link_get_stats(espnow_stats_t *stats_out)
{
    if (stats_out == NULL) { return ERR_NULL_POINTER; }

    *stats_out = s_stats;
    return ERR_OK;
}

/* ───────────────────────────────────────────────────────────────────── */

bool espnow_link_is_ready(void)
{
    return s_initialized;
}