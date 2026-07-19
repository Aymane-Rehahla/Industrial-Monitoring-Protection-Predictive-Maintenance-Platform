/**
 * @file    app_config.h
 * @brief   HMI Gateway configuration — pins, timing, protocol, WiFi.
 *          Single source of truth for all WROOM-32 configurable values.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — HMI node, not direct safety path.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  BOARD IDENTIFICATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define FIRMWARE_VERSION_STRING  "1.0.0"
#define FIRMWARE_BUILD_DATE      __DATE__
#define FIRMWARE_BUILD_TIME      __TIME__
#define BOARD_NAME               "ESP32-WROOM-32"

/* ═══════════════════════════════════════════════════════════════════════
 *  DEVICE ROLE — selected via Kconfig menuconfig
 * ═══════════════════════════════════════════════════════════════════════ */
#ifdef CONFIG_WROOM_IS_A
    #define WROOM_ROLE_A         1
    #define WROOM_ROLE_B         0
    #define WROOM_NODE_ID        1
    #define WROOM_NODE_NAME      "HMI-A"
#elif defined(CONFIG_WROOM_IS_B)
    #define WROOM_ROLE_A         0
    #define WROOM_ROLE_B         1
    #define WROOM_NODE_ID        2
    #define WROOM_NODE_NAME      "HMI-B"
#else
    /* Fallback: default to A if Kconfig not used */
    #define WROOM_ROLE_A         1
    #define WROOM_ROLE_B         0
    #define WROOM_NODE_ID        1
    #define WROOM_NODE_NAME      "HMI-A"
#endif

/* ═══════════════════════════════════════════════════════════════════════
 *  LED INDICATOR PINS
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_LED_GREEN            4
#define PIN_LED_YELLOW           5
#define PIN_LED_ORANGE          18
#define PIN_LED_RED             19
#define LED_COUNT                4

#define LED_BLINK_SLOW_MS      500
#define LED_BLINK_FAST_MS      150
#define LED_BLINK_HEARTBEAT_MS 1000
#define LED_UPDATE_INTERVAL_MS  50

/* ═══════════════════════════════════════════════════════════════════════
 *  UART2 — WROOM-to-WROOM PEER LINK
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_UART_PEER_TX        17
#define PIN_UART_PEER_RX        16
#define UART_PEER_PORT           2
#define UART_PEER_BAUD      115200
#define UART_PEER_RX_BUF       512
#define UART_PEER_TX_BUF       512
#define UART_PEER_SYNC_MS     1000
#define UART_PEER_TIMEOUT_MS  3000

#define UART_PEER_MAGIC        0xA5
#define UART_PEER_MAX_PAYLOAD   200

/* ═══════════════════════════════════════════════════════════════════════
 *  WiFi — APSTA MODE
 * ═══════════════════════════════════════════════════════════════════════ */
#define ESPNOW_WIFI_CHANNEL      1

#if WROOM_ROLE_A
    #define WIFI_AP_SSID         "SAFETY_HMI_A"
#else
    #define WIFI_AP_SSID         "SAFETY_HMI_B"
#endif

#define WIFI_AP_PASSWORD         "safe2025!"
#define WIFI_AP_MAX_CONN         2
#define WIFI_AP_CHANNEL          ESPNOW_WIFI_CHANNEL

/* ═══════════════════════════════════════════════════════════════════════
 *  TCP SERVER
 * ═══════════════════════════════════════════════════════════════════════ */
#define TCP_SERVER_PORT         8080
#define TCP_MAX_CLIENTS            2
#define TCP_RX_BUF_SIZE          256
#define TCP_TX_BUF_SIZE         1024

/* ═══════════════════════════════════════════════════════════════════════
 *  ESP-NOW
 * ═══════════════════════════════════════════════════════════════════════ */
#define ESPNOW_MAX_S3_PEERS      2
#define ESPNOW_RX_QUEUE_SIZE    16
#define ESPNOW_PEER_TIMEOUT_MS 5000

/* ═══════════════════════════════════════════════════════════════════════
 *  COMM PROTOCOL — MUST MATCH S3 FIRMWARE
 * ═══════════════════════════════════════════════════════════════════════ */
#define COMM_PACKET_MAGIC      0x5A
#define COMM_PROTOCOL_VERSION     1
#define COMM_DEVICE_TOKEN  0xC0FEBEEF
#define COMM_AUTH_ROUNDS        32

#define XTEA_KEY_0         0x1A2B3C4D
#define XTEA_KEY_1         0x5E6F7A8B
#define XTEA_KEY_2         0x9C0D1E2F
#define XTEA_KEY_3         0x3A4B5C6D

/* ═══════════════════════════════════════════════════════════════════════
 *  HARDCODED PEER MAC ADDRESSES
 *  Fill in after reading S3 boot logs.
 * ═══════════════════════════════════════════════════════════════════════ */
#define S3_A_MAC    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define S3_B_MAC    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#if WROOM_ROLE_A
    #define WROOM_PEER_MAC  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#else
    #define WROOM_PEER_MAC  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#endif

/* ═══════════════════════════════════════════════════════════════════════
 *  DATA VALIDATION THRESHOLDS
 * ═══════════════════════════════════════════════════════════════════════ */
#define MATCH_THRESH_VOLTAGE_V     5.0f
#define MATCH_THRESH_CURRENT_A     1.0f
#define MATCH_THRESH_TEMP_C        2.0f
#define MATCH_THRESH_GAS_PPM     200.0f
#define MATCH_THRESH_VIB_G         0.5f
#define MATCH_THRESH_RPM         100.0f

/* ═══════════════════════════════════════════════════════════════════════
 *  JSON / OUTPUT
 * ═══════════════════════════════════════════════════════════════════════ */
#define JSON_BUF_SIZE              512
#define JSON_FAST_INTERVAL_MS      200
#define JSON_SLOW_INTERVAL_MS     1000
#define JSON_STATUS_INTERVAL_MS   2000

/* ═══════════════════════════════════════════════════════════════════════
 *  STORES
 * ═══════════════════════════════════════════════════════════════════════ */
#define FAULT_STORE_MAX_ENTRIES   32
#define FAULT_DEDUP_WINDOW_MS   2000
#define TELEM_HISTORY_DEPTH       60

/* ═══════════════════════════════════════════════════════════════════════
 *  TASKS
 * ═══════════════════════════════════════════════════════════════════════ */
#define TASK_CORE_PROTOCOL        0
#define TASK_CORE_APPLICATION     1

#define TASK_STACK_ESPNOW_RX   3072
#define TASK_STACK_UART_PEER   3072
#define TASK_STACK_TCP_SERVER   4096
#define TASK_STACK_VALIDATION  3072
#define TASK_STACK_LED_UPDATE  2048
#define TASK_STACK_BRIDGE      4096

#define TASK_PRIO_ESPNOW_RX      12
#define TASK_PRIO_UART_PEER      10
#define TASK_PRIO_TCP_SERVER      8
#define TASK_PRIO_VALIDATION      6
#define TASK_PRIO_LED_UPDATE      4
#define TASK_PRIO_BRIDGE          7

/* ═══════════════════════════════════════════════════════════════════════
 *  NVS
 * ═══════════════════════════════════════════════════════════════════════ */
#define NVS_NAMESPACE            "hmi_cfg"

/* ═══════════════════════════════════════════════════════════════════════
 *  UTILITY MACROS
 * ═══════════════════════════════════════════════════════════════════════ */
#define UNUSED(x)         ((void)(x))
#define ARRAY_SIZE(arr)   (sizeof(arr) / sizeof((arr)[0]))
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/* ═══════════════════════════════════════════════════════════════════════
 *  COMPILE-TIME 