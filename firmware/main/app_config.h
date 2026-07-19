/**
 * @file    app_config.h
 * @brief   All hardware constants, pin definitions, timing, I2C addresses.
 *          Single point of truth for every configurable value.
 * @version 1.1.0
 * @date    2025-01-01
 * @safety  CRITICAL — Wrong pin or timing constant = damaged equipment.
 *
 * CHANGELOG:
 *   1.1.0  2025-01-01  Fixed duplicate defines, added missing HMI constants.
 *   1.0.0  2025-01-01  Initial release — frozen pin map.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  BOARD / FIRMWARE IDENTIFICATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define FIRMWARE_VERSION_MAJOR   1
#define FIRMWARE_VERSION_MINOR   0
#define FIRMWARE_VERSION_PATCH   0
#define FIRMWARE_VERSION_STRING  "1.0.0"
#define FIRMWARE_BUILD_DATE      __DATE__
#define FIRMWARE_BUILD_TIME      __TIME__
#define BOARD_NAME               "ESP32-S3-N16R8"

/* ═══════════════════════════════════════════════════════════════════════
 *  I2C BUS 0 — SAFETY-CRITICAL SENSORS (ADS1115 x2)
 *  NEVER share this bus with user-facing devices.
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_I2C_BUS0_SDA         8
#define PIN_I2C_BUS0_SCL         9
#define I2C_BUS0_FREQ_HZ        400000
#define I2C_BUS0_TIMEOUT_MS      100

/* ═══════════════════════════════════════════════════════════════════════
 *  I2C BUS 1 — SHARED (LCD + SHT45)
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_I2C_BUS1_SDA         17
#define PIN_I2C_BUS1_SCL         18
#define I2C_BUS1_FREQ_HZ        100000
#define I2C_BUS1_TIMEOUT_MS      100

/* ═══════════════════════════════════════════════════════════════════════
 *  I2C DEVICE ADDRESSES
 * ═══════════════════════════════════════════════════════════════════════ */
#define I2C_ADDR_ADS1115_VOLTAGE 0x48
#define I2C_ADDR_ADS1115_CURRENT 0x49
#define I2C_ADDR_LCD2004         0x27
#define I2C_ADDR_LCD2004_ALT     0x3F
#define I2C_ADDR_SHT45           0x44

/* ═══════════════════════════════════════════════════════════════════════
 *  I2C SAFETY PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════ */
#define I2C_RETRY_COUNT                  3
#define I2C_BUS_RECOVERY_CLOCK_PULSES   16
#define I2C_ERROR_THRESHOLD             10

/* ═══════════════════════════════════════════════════════════════════════
 *  ANALOG SENSOR PINS (ADC1 ONLY — ADC2 conflicts with WiFi/ESP-NOW)
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_ADC_MQ2              1
#define PIN_ADC_MQ4              2
#define PIN_ADC_MQ9              4
#define PIN_ADC_LIS344_X         5
#define PIN_ADC_LIS344_Y         6
#define PIN_ADC_MAX9814          7

/* ═══════════════════════════════════════════════════════════════════════
 *  CD4053BE ANALOG MUX
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_MUX_SELECT           3

/* ═══════════════════════════════════════════════════════════════════════
 *  DIGITAL SENSOR PINS
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_HALL_RPM            41

/* ═══════════════════════════════════════════════════════════════════════
 *  BUTTON PINS — all active LOW with internal pull-up
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_BTN_LEFT            10
#define PIN_BTN_RIGHT           11
#define PIN_BTN_UP              12
#define PIN_BTN_DOWN            21
#define PIN_BTN_OK              42

#define BTN_ACTIVE_LEVEL         0
#define BTN_DEBOUNCE_MS         50
#define BTN_LONG_PRESS_MS     2000
#define BTN_REPEAT_DELAY_MS    500
#define BTN_REPEAT_RATE_MS     150
#define BTN_SCAN_INTERVAL_MS    20      /* How often buttons are polled */

#define APP_BUTTON_COUNT         5

/* ═══════════════════════════════════════════════════════════════════════
 *  BUZZER — PWM via LEDC peripheral
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_BUZZER              13

#define BUZZER_LEDC_TIMER        0
#define BUZZER_LEDC_CHANNEL      0
#define BUZZER_LEDC_SPEED_MODE   0
#define BUZZER_LEDC_RESOLUTION  10
#define BUZZER_DUTY_50_PERCENT 512
#define BUZZER_DEFAULT_DUTY    512      /* 50% duty for 10-bit resolution */

/* Tone frequencies (Hz) */
#define TONE_CLICK_HZ         4000
#define TONE_NAV_HZ           2500
#define TONE_CONFIRM_HZ       3000
#define TONE_WARNING_HZ       2000
#define TONE_ERROR_HZ         1000
#define TONE_ALARM_HZ          800
#define TONE_STARTUP_HZ       1500

/* Tone durations (ms) */
#define TONE_CLICK_MS           20
#define TONE_NAV_MS             50
#define TONE_CONFIRM_MS        100
#define TONE_WARNING_MS        200
#define TONE_ERROR_MS          400
#define TONE_ALARM_MS          500
#define TONE_STARTUP_MS        150

/* ═══════════════════════════════════════════════════════════════════════
 *  LEDs
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_LED_RED             40
#define PIN_RGB_LED             48
#define RGB_BRIGHTNESS          20

/* LED blink intervals (ms) */
#define LED_BLINK_SLOW_MS      500
#define LED_BLINK_FAST_MS      150

/* SOS pattern timing (ms) */
#define LED_SOS_DOT_MS         200
#define LED_SOS_DASH_MS        600
#define LED_SOS_GAP_MS         200
#define LED_SOS_WORD_GAP_MS   1400

/* ═══════════════════════════════════════════════════════════════════════
 *  RELAY & SAFETY CHAIN
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_RELAY_DRIVE         15
#define PIN_RELAY_READBACK      16
#define PIN_WATCHDOG_KEY_OUT    14
#define PIN_WATCHDOG_WARN_IN    47

#define WATCHDOG_KEY_INTERVAL_MS 100
#define WATCHDOG_LCG_SEED       0x42
#define WATCHDOG_LCG_MUL         173
#define WATCHDOG_LCG_INC          79

/* ═══════════════════════════════════════════════════════════════════════
 *  UART — PEER ESP COMMUNICATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_UART_PEER_TX        43
#define PIN_UART_PEER_RX        44
#define UART_PORT_PEER           1
#define UART_BAUD_RATE      115200
#define UART_RX_BUF_SIZE       512
#define UART_TX_BUF_SIZE       512

/* ═══════════════════════════════════════════════════════════════════════
 *  SPARE PINS
 * ═══════════════════════════════════════════════════════════════════════ */
#define PIN_SPARE_DIGITAL       38
#define PIN_SPARE_JTAG          39

/* ═══════════════════════════════════════════════════════════════════════
 *  LCD (I2C Bus 1)
 * ═══════════════════════════════════════════════════════════════════════ */
#define LCD_COLS                20
#define LCD_ROWS                 4
#define LCD_UPDATE_INTERVAL_MS 250
#define LCD_ERROR_THRESHOLD     10

/* ═══════════════════════════════════════════════════════════════════════
 *  ADC CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define ADC_ATTEN_INDEX          3
#define ADC_WIDTH_INDEX          4
#define ADC_MAX_RAW           4095
#define ADC_REF_MV            3300

/* ═══════════════════════════════════════════════════════════════════════
 *  TASK CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define TASK_CORE_REALTIME       1
#define TASK_CORE_COMMS          0

#define TASK_STACK_PROTECTION  4096
#define TASK_STACK_SENSOR_POLL 4096
#define TASK_STACK_HEARTBEAT   3072
#define TASK_STACK_HMI         4096
#define TASK_STACK_TELEMETRY   4096

#define TASK_PRIO_PROTECTION    24
#define TASK_PRIO_SENSOR_POLL   20
#define TASK_PRIO_HEARTBEAT     18
#define TASK_PRIO_HMI           10
#define TASK_PRIO_TELEMETRY      8

/* ═══════════════════════════════════════════════════════════════════════
 *  TIMING INTERVALS (ms)
 * ═══════════════════════════════════════════════════════════════════════ */
#define PROTECTION_CHECK_MS     10
#define SENSOR_POLL_MS         100
#define HEARTBEAT_MS            50
#define LCD_UPDATE_MS          250
#define TELEMETRY_MS          1000
#define LED_UPDATE_MS          100
#define BTN_SCAN_MS             20
#define LED_UPDATE_INTERVAL_MS  LED_UPDATE_MS   /* Alias used by HMI code */

/* ═══════════════════════════════════════════════════════════════════════
 *  MAGIC NUMBERS
 * ═══════════════════════════════════════════════════════════════════════ */
#define MAGIC_SENSOR_DATA    0x5E5D
#define MAGIC_CONFIG_DATA    0xC0FD
#define MAGIC_FAULT_LOG      0xFA17
#define MAGIC_CALIBRATION    0xCA1B
#define STATUS_MAGIC         0xDEAD5AFE

/* ═══════════════════════════════════════════════════════════════════════
 *  NVS (Non-Volatile Storage) KEYS
 * ═══════════════════════════════════════════════════════════════════════ */
#define NVS_NAMESPACE            "safety_cfg"
#define NVS_KEY_BOOT_COUNT       "boot_cnt"
#define NVS_KEY_FAULT_COUNT      "fault_cnt"
#define NVS_KEY_LAST_FAULT       "last_fault"
#define NVS_KEY_DEVICE_ROLE      "dev_role"
#define NVS_KEY_PEER_MAC         "peer_mac"
#define NVS_KEY_THRESHOLDS       "thresholds"
#define NVS_KEY_CALIBRATION      "cal_data"
#define NVS_KEY_MUTE_STATE       "mute_state"

/* ═══════════════════════════════════════════════════════════════════════
 *  SENSOR-SPECIFIC CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */
#define ACS758_SENSITIVITY_MV_PER_A   40.0f
#define ACS758_ZERO_CURRENT_MV      1650.0f

#define ZMPT101B_RATIO               1.0f
#define ZMPT101B_OFFSET_MV        1650.0f

#define MQ_WARMUP_MS              30000

#define HALL_PULSES_PER_REV          1
#define HALL_RPM_TIMEOUT_MS       2000

#define LIS344_SENSITIVITY_HIGH_MV_PER_G  660.0f

#define SHT45_TEMP_MIN_C          -40.0f
#define SHT45_TEMP_MAX_C          125.0f

/* ═══════════════════════════════════════════════════════════════════════
 *  FILTER CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define SENSOR_FILTER_SAMPLES     8

/* ═══════════════════════════════════════════════════════════════════════
 *  HMI CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define HMI_MUTE_COMBO_MS       3000
#define HMI_ENGINEER_COMBO_MS   5000
#define HMI_IDLE_TIMEOUT_MS    60000

/* Aliases used by HMI code */
#define ENGINEER_MODE_HOLD_MS   HMI_ENGINEER_COMBO_MS
#define MUTE_COMBO_HOLD_MS      HMI_MUTE_COMBO_MS

#define THRESHOLD_STEP_VOLTAGE_V      1.0f
#define THRESHOLD_STEP_CURRENT_A      0.5f
#define THRESHOLD_STEP_TEMP_C         1.0f
#define THRESHOLD_STEP_GAS_PPM       50.0f
#define THRESHOLD_STEP_VIBRATION_G    0.1f
#define THRESHOLD_STEP_RPM          100.0f

#define THRESHOLD_COARSE_MULTIPLIER  10
#define THRESHOLD_FINE_DIVISOR       10

/* ═══════════════════════════════════════════════════════════════════════
 *  MENU CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define MENU_LABEL_MAX_LEN       20



/* ═══════════════════════════════════════════════════════════════════════
 *  ESP-NOW CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════ */
#define ESPNOW_WIFI_CHANNEL          1
#define ESPNOW_MAX_WROOM_PEERS       2
#define ESPNOW_PEER_TIMEOUT_MS    5000
#define ESPNOW_RX_QUEUE_SIZE        16
#define ESPNOW_MAX_PACKET_SIZE     250

/* ═══════════════════════════════════════════════════════════════════════
 *  COMM PROTOCOL
 * ═══════════════════════════════════════════════════════════════════════ */
#define COMM_PACKET_MAGIC          0x5A
#define COMM_PROTOCOL_VERSION         1
#define COMM_DEVICE_TOKEN      0xC0FEBEEF
#define COMM_AUTH_ROUNDS            32

/* ═══════════════════════════════════════════════════════════════════════
 *  XTEA SHARED KEY (128-bit)
 *  MUST match on WROOM firmware.  Change before deployment.
 * ═══════════════════════════════════════════════════════════════════════ */
#define XTEA_KEY_0             0x1A2B3C4D
#define XTEA_KEY_1             0x5E6F7A8B
#define XTEA_KEY_2             0x9C0D1E2F
#define XTEA_KEY_3             0x3A4B5C6D

/* ═══════════════════════════════════════════════════════════════════════
 *  TELEMETRY TIMING
 * ═══════════════════════════════════════════════════════════════════════ */
#define TELEM_FAST_INTERVAL_MS     200
#define TELEM_SLOW_INTERVAL_MS    1000
#define TELEM_HEARTBEAT_MS         400

/* ═══════════════════════════════════════════════════════════════════════
 *  TASK — ESPNOW RX DISPATCH
 * ═══════════════════════════════════════════════════════════════════════ */
#define TASK_STACK_ESPNOW_RX      3072
#define TASK_PRIO_ESPNOW_RX         12

/* ═══════════════════════════════════════════════════════════════════════
 *  NVS KEYS — WROOM PEERS
 * ═══════════════════════════════════════════════════════════════════════ */
#define NVS_KEY_WROOM_MAC_0      "wroom_mac0"
#define NVS_KEY_WROOM_MAC_1      "wroom_mac1"
#define NVS_KEY_WROOM_COUNT      "wroom_cnt"



/* ═══════════════════════════════════════════════════════════════════════
 *  COMPILE-TIME SAFETY CHECKS
 * ═══════════════════════════════════════════════════════════════════════ */

#if (PIN_I2C_BUS0_SDA == PIN_I2C_BUS1_SDA) || \
    (PIN_I2C_BUS0_SDA == PIN_I2C_BUS1_SCL) || \
    (PIN_I2C_BUS0_SCL == PIN_I2C_BUS1_SDA) || \
    (PIN_I2C_BUS0_SCL == PIN_I2C_BUS1_SCL)
    #error "I2C Bus 0 and Bus 1 pins overlap — check wiring!"
#endif

_Static_assert(APP_BUTTON_COUNT == 5,
               "APP_BUTTON_COUNT must be 5 (LEFT, RIGHT, UP, DOWN, OK)");

_Static_assert(PIN_RELAY_DRIVE == 15,
               "PIN_RELAY_DRIVE must be 15 — register write is hardcoded");

#endif /* APP_CONFIG_H */