/**
 * @file hal_uart.c
 * @brief UART HAL — all bugs fixed.
 * @version 1.0.1
 * @safety CRITICAL
 *
 * BUG 12: no unaligned struct cast — use memcpy.
 * BUG 13: explicit endianness parsing on both sides.
 * BUG 14: size_t for uart_get_buffered_data_len.
 * BUG 15: try_parse_packet split into helpers.
 * BUG 26: uses time_utils.h.
 * BUG 27: uses crc_utils.h.
 */

#include "hal_uart.h"
#include "app_config.h"
#include "time_utils.h"
#include "crc_utils.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "HAL_UART";

#define HEADER_SIZE  6   /* magic(2) + type(1) + seq(1) + length(2) */
#define CRC_SIZE     2

/* ── State ───────────────────────────────────────────────────────────── */

static bool              s_init   = false;
static SemaphoreHandle_t s_tx_mtx = NULL;
static uint8_t           s_seq    = 0;
static peer_stats_t      s_stats;

static uint8_t s_rxbuf[PEER_MAX_PACKET_SIZE * 2];
static size_t  s_rxidx = 0;

/* ── Serialize header big-endian (BUG 13) ────────────────────────────── */

static void serialize_header(uint8_t *buf, uint8_t type,
                             uint8_t seq, uint16_t len)
{
    buf[0] = (PEER_PACKET_MAGIC >> 8) & 0xFF;
    buf[1] =  PEER_PACKET_MAGIC       & 0xFF;
    buf[2] = type;
    buf[3] = seq;
    buf[4] = (len >> 8) & 0xFF;
    buf[5] =  len       & 0xFF;
}

/* ── Parse header big-endian (BUG 12 + 13 fix) ──────────────────────── */

static bool parse_header(const uint8_t *buf, packet_header_t *out)
{
    out->magic    = ((uint16_t)buf[0] << 8) | buf[1];
    out->type     = buf[2];
    out->sequence = buf[3];
    out->length   = ((uint16_t)buf[4] << 8) | buf[5];
    return (out->magic == PEER_PACKET_MAGIC);
}

/* ── Peer status helper ──────────────────────────────────────────────── */

static void update_peer(void)
{
    uint32_t now = get_time_ms();
    uint32_t dt  = now - s_stats.last_hb_ms;

    if (s_stats.last_hb_ms == 0)         { s_stats.peer = PEER_UNKNOWN; }
    else if (dt < HEARTBEAT_INTERVAL_MS*2){ s_stats.peer = PEER_ONLINE; }
    else if (dt < WATCHDOG_TIMEOUT_MS)    { s_stats.peer = PEER_DEGRADED; }
    else                                  { s_stats.peer = PEER_OFFLINE; }
}

/* ── BUG 15 sub-function: find magic, discard garbage ────────────────── */

static bool find_and_align_magic(void)
{
    for (size_t i = 0; i + 1 < s_rxidx; i++) {
        uint16_t m = ((uint16_t)s_rxbuf[i] << 8) | s_rxbuf[i + 1];
        if (m == PEER_PACKET_MAGIC) {
            if (i > 0) {
                memmove(s_rxbuf, s_rxbuf + i, s_rxidx - i);
                s_rxidx -= i;
                s_stats.pkts_invalid++;
            }
            return true;
        }
    }
    return false;
}

/* ── BUG 15 sub-function: validate CRC ───────────────────────────────── */

static bool validate_packet_crc(size_t total_len)
{
    uint16_t rx_crc = ((uint16_t)s_rxbuf[total_len - 2] << 8)
                    |           s_rxbuf[total_len - 1];
    uint16_t calc   = crc16_calculate(s_rxbuf, total_len - CRC_SIZE);
    return (rx_crc == calc);
}

/* ── BUG 15 sub-function: dispatch ───────────────────────────────────── */

static void dispatch_packet(const packet_header_t *hdr,
                            packet_callback_t cb)
{
    s_stats.pkts_recv++;
    s_stats.pkts_valid++;

    /* Handle heartbeat internally */
    if (hdr->type == PACKET_HEARTBEAT &&
        hdr->length == sizeof(heartbeat_payload_t))
    {
        heartbeat_payload_t hb;
        memcpy(&hb, s_rxbuf + HEADER_SIZE, sizeof(hb));
        s_stats.hb_recv++;
        s_stats.last_hb_ms    = get_time_ms();
        s_stats.peer_uptime_ms = hb.uptime_ms;
        update_peer();
    }

    if (cb) {
        const void *payload = (hdr->length > 0)
                              ? (s_rxbuf + HEADER_SIZE) : NULL;
        cb((packet_type_t)hdr->type, payload, hdr->length);
    }
}

/* ── Process one packet from buffer ──────────────────────────────────── */

static size_t try_parse(packet_callback_t cb)
{
    if (s_rxidx < HEADER_SIZE + CRC_SIZE) { return 0; }
    if (!find_and_align_magic())          { return 0; }
    if (s_rxidx < HEADER_SIZE + CRC_SIZE) { return 0; }

    packet_header_t hdr;
    if (!parse_header(s_rxbuf, &hdr))     { return 0; }

    /* sanity-check length */
    if (hdr.length > PEER_MAX_PACKET_SIZE - HEADER_SIZE - CRC_SIZE) {
        memmove(s_rxbuf, s_rxbuf + 2, s_rxidx - 2);
        s_rxidx -= 2;
        s_stats.pkts_invalid++;
        return 0;
    }

    size_t total = HEADER_SIZE + hdr.length + CRC_SIZE;
    if (s_rxidx < total) { return 0; }

    if (!validate_packet_crc(total)) {
        s_stats.pkts_invalid++;
        memmove(s_rxbuf, s_rxbuf + 2, s_rxidx - 2);
        s_rxidx -= 2;
        return 0;
    }

    dispatch_packet(&hdr, cb);
    return total;
}

/* ── Public ──────────────────────────────────────────────────────────── */

error_code_t hal_uart_init(void)
{
    ESP_LOGI(TAG, "Initializing UART peer link...");

    s_tx_mtx = xSemaphoreCreateMutex();
    if (!s_tx_mtx) { return ERR_UART_INIT_FAILED; }

    uart_config_t uc = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t e;
    e = uart_driver_install(UART_PORT_PEER, UART_RX_BUF_SIZE,
                            UART_TX_BUF_SIZE, 0, NULL, 0);
    if (e != ESP_OK) { return ERR_UART_INIT_FAILED; }
    e = uart_param_config(UART_PORT_PEER, &uc);
    if (e != ESP_OK) { return ERR_UART_INIT_FAILED; }
    e = uart_set_pin(UART_PORT_PEER, PIN_UART_TX, PIN_UART_RX,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (e != ESP_OK) { return ERR_UART_INIT_FAILED; }

    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.peer = PEER_UNKNOWN;
    s_init = true;

    ESP_LOGI(TAG, "UART OK (TX=%d RX=%d %d baud)",
             PIN_UART_TX, PIN_UART_RX, UART_BAUD_RATE);
    return ERR_OK;
}

error_code_t hal_uart_send_packet(packet_type_t type,
                                   const void *payload, size_t len)
{
    if (len > 0 && !payload)                { return ERR_NULL_POINTER; }
    if (len > PEER_MAX_PACKET_SIZE - HEADER_SIZE - CRC_SIZE) {
        return ERR_INVALID_PARAMETER;
    }
    if (!s_init)                            { return ERR_UART_INIT_FAILED; }
    if (xSemaphoreTake(s_tx_mtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ERR_UART_TIMEOUT;
    }

    uint8_t pkt[PEER_MAX_PACKET_SIZE];
    serialize_header(pkt, (uint8_t)type, s_seq++, (uint16_t)len);
    size_t off = HEADER_SIZE;
    if (len > 0) { memcpy(pkt + off, payload, len); off += len; }

    uint16_t crc = crc16_calculate(pkt, off);
    pkt[off++] = (crc >> 8) & 0xFF;
    pkt[off++] =  crc       & 0xFF;

    int sent = uart_write_bytes(UART_PORT_PEER, pkt, off);
    xSemaphoreGive(s_tx_mtx);

    if (sent != (int)off) { return ERR_UART_SEND_FAILED; }
    s_stats.pkts_sent++;
    return ERR_OK;
}

error_code_t hal_uart_send_heartbeat(system_state_t st, uint8_t faults,
                                      bool relay_cmd, bool relay_en)
{
    heartbeat_payload_t hb = {
        .uptime_ms       = get_time_ms(),
        .state           = (uint8_t)st,
        .fault_count     = faults,
        .relay_commanded = relay_cmd ? 1 : 0,
        .relay_enabled   = relay_en  ? 1 : 0,
    };
    error_code_t e = hal_uart_send_packet(PACKET_HEARTBEAT, &hb, sizeof(hb));
    if (e == ERR_OK) { s_stats.hb_sent++; }
    return e;
}

error_code_t hal_uart_send_sensor_data(const sensor_sync_payload_t *d)
{
    if (!d) { return ERR_NULL_POINTER; }
    return hal_uart_send_packet(PACKET_SENSOR_DATA, d, sizeof(*d));
}

int hal_uart_process_rx(packet_callback_t cb)
{
    if (!s_init) { return 0; }

    /* BUG 14 fix: proper size_t */
    size_t avail = 0;
    esp_err_t e = uart_get_buffered_data_len(UART_PORT_PEER, &avail);
    if (e != ESP_OK) { return 0; }

    if (avail > 0) {
        size_t space = sizeof(s_rxbuf) - s_rxidx;
        if (avail > space) { avail = space; }
        int rd = uart_read_bytes(UART_PORT_PEER, s_rxbuf + s_rxidx,
                                 avail, pdMS_TO_TICKS(10));
        if (rd > 0) { s_rxidx += (size_t)rd; }
    }

    int packets = 0;
    int max_iter = 10;  /* Rule 2.3: max iterations */
    size_t consumed;
    while ((consumed = try_parse(cb)) > 0 && max_iter-- > 0) {
        if (consumed < s_rxidx) {
            memmove(s_rxbuf, s_rxbuf + consumed, s_rxidx - consumed);
        }
        s_rxidx -= consumed;
        packets++;
    }

    update_peer();
    return packets;
}

peer_status_t hal_uart_get_peer_status(void)
{
    update_peer();
    return s_stats.peer;
}

bool hal_uart_is_peer_online(void)
{
    update_peer();
    return (s_stats.peer == PEER_ONLINE || s_stats.peer == PEER_DEGRADED);
}

error_code_t hal_uart_get_stats(peer_stats_t *out)
{
    if (!out) { return ERR_NULL_POINTER; }
    update_peer();
    memcpy(out, &s_stats, sizeof(*out));
    return ERR_OK;
}

error_code_t hal_uart_self_test(void)
{
    ESP_LOGI(TAG, "UART self-test...");
    if (!s_init) { return ERR_UART_INIT_FAILED; }
    ESP_LOGI(TAG, "  PASS: initialized");
    ESP_LOGI(TAG, "  Peer: %s",
             s_stats.peer == PEER_ONLINE   ? "ONLINE" :
             s_stats.peer == PEER_DEGRADED ? "DEGRADED" :
             s_stats.peer == PEER_OFFLINE  ? "OFFLINE" : "UNKNOWN");
    return ERR_OK;
}