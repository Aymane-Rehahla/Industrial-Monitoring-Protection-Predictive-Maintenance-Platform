/**
 * @file    espnow_link.c
 * @brief   ESP-NOW transport — APSTA mode for simultaneous ESP-NOW + WiFi AP.
 * @version 1.0.0
 *
 * KEY DIFFERENCE from S3 version:
 *   WiFi mode = WIFI_MODE_APSTA (not just STA)
 *   STA interface handles ESP-NOW
 *   AP interface serves iPad TCP connections
 */

#include "comm/espnow_link.h"
#include "utils/crc_utils.h"

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

#define MAX_MSG_TYPES  32
#define XTEA_DELTA     0x9E3779B9u

static bool s_initialized = false;
static QueueHandle_t s_rx_queue = NULL;
static TaskHandle_t s_rx_task_handle = NULL;
static espnow_rx_handler_t s_handlers[MAX_MSG_TYPES];
static uint16_t s_sequence = 0;
static espnow_stats_t s_stats;

/* ── XTEA ── */

void espnow_xtea_encrypt(uint32_t v[2], const uint32_t key[4])
{
    uint32_t v0 = v[0], v1 = v[1], sum = 0;
    for (uint32_t i = 0; i < COMM_AUTH_ROUNDS; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += XTEA_DELTA;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }
    v[0] = v0; v[1] = v1;
}

void espnow_xtea_decrypt(uint32_t v[2], const uint32_t key[4])
{
    uint32_t v0 = v[0], v1 = v[1], sum = XTEA_DELTA * COMM_AUTH_ROUNDS;
    for (uint32_t i = 0; i < COMM_AUTH_ROUNDS; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        sum -= XTEA_DELTA;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }
    v[0] = v0; v[1] = v1;
}

/* ── Callbacks ── */

static void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    UNUSED(mac_addr);
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_stats.tx_count++;
    } else {
        s_stats.tx_fail_count++;
    }
}

static void recv_cb(const esp_now_recv_info_t *info,
                    const uint8_t *data, int data_len)
{
    if (!info || !data || data_len <= 0 || data_len > 250) return;

    espnow_rx_item_t item;
    memcpy(item.src_mac, info->src_addr, 6);
    memcpy(item.data, data, (size_t)data_len);
    item.data_len = (uint8_t)data_len;

    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(s_rx_queue, &item, &woken) != pdTRUE) {
        s_stats.rx_dropped++;
    }
    if (woken) portYIELD_FROM_ISR();
}

/* ── RX dispatch ── */

static void rx_dispatch_task(void *arg)
{
    UNUSED(arg);
    espnow_rx_item_t item;
    const size_t min_frame = sizeof(comm_header_t) + sizeof(comm_footer_t);

    ESP_LOGI(TAG, "RX dispatch on core %d", xPortGetCoreID());

    while (1) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) != pdTRUE) continue;

        if (item.data_len < min_frame) { s_stats.rx_crc_errors++; continue; }

        const comm_header_t *hdr = (const comm_header_t *)item.data;
        if (hdr->magic != COMM_PACKET_MAGIC)    { s_stats.rx_crc_errors++; continue; }
        if (hdr->version != COMM_PROTOCOL_VERSION) { s_stats.rx_crc_errors++; continue; }

        size_t expected = sizeof(comm_header_t) + hdr->payload_len + sizeof(comm_footer_t);
        if (expected != item.data_len) { s_stats.rx_crc_errors++; continue; }

        size_t crc_len = sizeof(comm_header_t) + hdr->payload_len;
        uint16_t crc = crc16_calc(item.data, crc_len);
        const comm_footer_t *ftr = (const comm_footer_t *)(item.data + crc_len);
        if (ftr->crc16 != crc) { s_stats.rx_crc_errors++; continue; }

        s_stats.rx_count++;
        s_stats.last_rx_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        uint8_t mt = hdr->msg_type;
        if (mt < MAX_MSG_TYPES && s_handlers[mt]) {
            s_handlers[mt](item.src_mac, item.data + sizeof(comm_header_t),
                           hdr->payload_len);
        }
    }
}

/* ── Init ── */

error_code_t espnow_link_init(void)
{
    if (s_initialized) return ERR_OK;
    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "netif: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop: %s", esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    /* Create both STA and AP netifs. */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "wifi_init: %s", esp_err_to_name(err)); return ERR_HW_INIT_FAILED; }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) { ESP_LOGE(TAG, "set_storage: %s", esp_err_to_name(err)); return ERR_HW_INIT_FAILED; }

    /* APSTA mode: STA for ESP-NOW, AP for iPad. */
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) { ESP_LOGE(TAG, "set_mode: %s", esp_err_to_name(err)); return ERR_HW_INIT_FAILED; }

    /* Configure SoftAP. */
    wifi_config_t ap_cfg = {
        .ap = {
            .channel         = WIFI_AP_CHANNEL,
            .max_connection  = WIFI_AP_MAX_CONN,
            .authmode        = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg         = { .required = false },
        }
    };
    /* Copy SSID and password safely. */
    strncpy((char *)ap_cfg.ap.ssid, WIFI_AP_SSID, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = (uint8_t)strlen(WIFI_AP_SSID);
    strncpy((char *)ap_cfg.ap.password, WIFI_AP_PASSWORD, sizeof(ap_cfg.ap.password) - 1);

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err != ESP_OK) { ESP_LOGW(TAG, "ap_config: %s", esp_err_to_name(err)); }

    err = esp_wifi_start();
    if (err != ESP_OK) { ESP_LOGE(TAG, "wifi_start: %s", esp_err_to_name(err)); return ERR_HW_INIT_FAILED; }

    err = esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) { ESP_LOGW(TAG, "set_channel: %s", esp_err_to_name(err)); }

    /* ESP-NOW init. */
    err = esp_now_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_now_init: %s", esp_err_to_name(err)); return ERR_HW_INIT_FAILED; }

    esp_now_register_send_cb(send_cb);
    esp_now_register_recv_cb(recv_cb);

    memset(s_handlers, 0, sizeof(s_handlers));
    memset(&s_stats, 0, sizeof(s_stats));
    s_sequence = 0;

    s_rx_queue = xQueueCreate(ESPNOW_RX_QUEUE_SIZE, sizeof(espnow_rx_item_t));
    if (!s_rx_queue) { ESP_LOGE(TAG, "queue create fail"); esp_now_deinit(); return ERR_HW_INIT_FAILED; }

    BaseType_t ret = xTaskCreatePinnedToCore(rx_dispatch_task, "espnow_rx",
        TASK_STACK_ESPNOW_RX, NULL, TASK_PRIO_ESPNOW_RX, &s_rx_task_handle,
        TASK_CORE_PROTOCOL);
    if (ret != pdPASS) { vQueueDelete(s_rx_queue); esp_now_deinit(); return ERR_HW_INIT_FAILED; }

    s_initialized = true;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "ESP-NOW APSTA ready — STA MAC: %02X:%02X:%02X:%02X:%02X:%02X  AP SSID: %s  CH:%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             WIFI_AP_SSID, ESPNOW_WIFI_CHANNEL);

    return ERR_OK;
}

error_code_t espnow_link_add_peer(const uint8_t mac[6], uint8_t channel)
{
    if (!mac) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    if (esp_now_is_peer_exist(mac)) return ERR_OK;

    esp_now_peer_info_t pi;
    memset(&pi, 0, sizeof(pi));
    memcpy(pi.peer_addr, mac, 6);
    pi.channel = channel;
    pi.ifidx = ESP_IF_WIFI_STA;
    pi.encrypt = false;

    esp_err_t e = esp_now_add_peer(&pi);
    if (e != ESP_OK) { ESP_LOGE(TAG, "add_peer: %s", esp_err_to_name(e)); return ERR_HW_INIT_FAILED; }

    ESP_LOGI(TAG, "Peer: %02X:%02X:%02X:%02X:%02X:%02X ch=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], channel);
    return ERR_OK;
}

error_code_t espnow_link_remove_peer(const uint8_t mac[6])
{
    if (!mac) return ERR_NULL_POINTER;
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    if (!esp_now_is_peer_exist(mac)) return ERR_OK;
    esp_now_del_peer(mac);
    return ERR_OK;
}

error_code_t espnow_link_send(const uint8_t dest_mac[6], uint8_t msg_type,
                              const uint8_t *payload, uint8_t payload_len)
{
    if (!s_initialized) return ERR_NOT_INITIALIZED;
    if (payload_len > COMM_MAX_PAYLOAD_SIZE) return ERR_INVALID_ARG;
    if (payload_len > 0 && !payload) return ERR_NULL_POINTER;

    static uint8_t s_tx_buf[250];

    comm_header_t hdr;
    hdr.magic       = COMM_PACKET_MAGIC;
    hdr.version     = COMM_PROTOCOL_VERSION;
    hdr.msg_type    = msg_type;
    hdr.src_role    = 0;  /* WROOM role — not S3 */
    hdr.sequence    = s_sequence++;
    hdr.payload_len = payload_len;
    hdr.node_id     = WROOM_NODE_ID;

    memcpy(s_tx_buf, &hdr, sizeof(hdr));
    if (payload_len > 0) memcpy(s_tx_buf + sizeof(hdr), payload, payload_len);

    size_t crc_len = sizeof(hdr) + payload_len;
    comm_footer_t ftr;
    ftr.crc16 = crc16_calc(s_tx_buf, crc_len);
    memcpy(s_tx_buf + crc_len, &ftr, sizeof(ftr));

    esp_err_t e = esp_now_send(dest_mac, s_tx_buf, crc_len + sizeof(ftr));
    if (e != ESP_OK) { s_stats.tx_fail_count++; return ERR_HW_INIT_FAILED; }
    return ERR_OK;
}

error_code_t espnow_link_register_handler(uint8_t msg_type, espnow_rx_handler_t handler)
{
    if (msg_type >= MAX_MSG_TYPES) return ERR_INVALID_ARG;
    s_handlers[msg_type] = handler;
    return ERR_OK;
}

error_code_t espnow_link_get_stats(espnow_stats_t *out)
{
    if (!out) return ERR_NULL_POINTER;
    *out = s_stats;
    return ERR_OK;
}

bool espnow_link_is_ready(void) { return s_initialized; }