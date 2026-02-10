/**
 * @file app_config.h
 * @brief Hardware pin map and system constants. FROZEN after validation.
 * @version 1.0.0
 * @safety CRITICAL
 *
 * ESP32-S3-N16R8 has exactly 2 I2C ports (0, 1).
 * Bus 0: ADS1115 x2 (voltage + current sensors)
 * Bus 1: LCD 2004 + SHT45 (shared bus, different addresses)
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ── System identity ─────────────────────────────────────────────────── */
#define FIRMWARE_VERSION_STRING  "1.0.0"
#define SYSTEM_NAME              "SAFETY MODULE"
#define HARDWARE_TARGET          "ESP32-S3-N16R8"

/* ── I2C bus 0 – ADC sensors (fast) ──────────────────────────────────── */
#define I2C_PORT_ADC             0
#define PIN_I2C0_SDA             8
#define PIN_I2C0_SCL             9
#define I2C_FREQ_ADC_HZ          400000

/* ── I2C bus 1 – LCD + SHT45 (shared) ────────────────────────────────── */
#define I2C_PORT_SHARED          1
#define PIN_I2C1_SDA             17
#define PIN_I2C1_SCL             18
#define I2C_FREQ_SHARED_HZ       100000

/* ── I2C addresses ───────────────────────────────────────────────────── */
#define I2C_ADDR_ADS_VOLTAGE     0x48
#define I2C_ADDR_ADS_CURRENT     0x49
#define I2C_ADDR_SHT45           0x44
#define I2C_ADDR_LCD_PRIMARY     0x27
#define I2C_ADDR_LCD_ALTERNATE   0x3F

/* ── I2C safety ──────────────────────────────────────────────────────── */
#define I2C_TIMEOUT_MS           100
#define I2C_RETRY_COUNT          3

/* ── Internal ADC channels (ESP32-S3 ADC1) ───────────────────────────── */
#define PIN_ADC_LM35             1
#define PIN_ADC_MAX9814          2
#define PIN_ADC_MQ2              3
#define PIN_ADC_MQ4              4
#define PIN_ADC_MQ9              5

#define ADC_CHANNEL_LM35         0
#define ADC_CHANNEL_MAX9814      1
#define ADC_CHANNEL_MQ2          2
#define ADC_CHANNEL_MQ4          3
#define ADC_CHANNEL_MQ9          4
#define ADC_CHANNEL_COUNT        5

#define ADC_MAX_RAW              4095
#define ADC_REF_MV               3300

/* ── Digital input pins ──────────────────────────────────────────────── */
#define PIN_HALL_SENSOR          6
#define PIN_BTN_UP               35
#define PIN_BTN_DOWN             36
#define PIN_BTN_LEFT             37
#define PIN_BTN_RIGHT            38
#define PIN_BTN_OK               39
#define PIN_RELAY_ENABLE_IN      15

/* ── Digital output pins ─────────────────────────────────────────────── */
#define PIN_LED_GREEN            10
#define PIN_LED_RED              11
#define PIN_RELAY_DRIVE          12
#define PIN_BUZZER               13
#define PIN_HEARTBEAT_OUT        14
#define PIN_RGB_LED              48

/* ── UART peer communication ─────────────────────────────────────────── */
#define PIN_UART_TX              43
#define PIN_UART_RX              44
#define UART_PORT_PEER           1
#define UART_BAUD_RATE           115200
#define UART_RX_BUF_SIZE         512
#define UART_TX_BUF_SIZE         512

/* ── Button timing ───────────────────────────────────────────────────── */
#define BTN_DEBOUNCE_MS          30
#define BTN_LONG_PRESS_MS        1500
#define BTN_REPEAT_DELAY_MS      400
#define BTN_REPEAT_RATE_MS       100

/* ── Voltage divider: 10 k + 10 k  ───────────────────────────────────── */
#define VDIV_RATIO               0.5f
#define VDIV_MULTIPLY            2.0f

/* ── ACS758-50A after divider ────────────────────────────────────────── */
#define ACS_ZERO_MV_DIVIDED      1250.0f
#define ACS_SENS_MV_PER_A_DIV    20.0f
#define ACS_MAX_CURRENT_A        50.0f

/* ── LM35 ────────────────────────────────────────────────────────────── */
#define LM35_MV_PER_DEG_C        10.0f

/* ── MQ gas warm-up ──────────────────────────────────────────────────── */
#define MQ_WARMUP_MS             30000
#define MQ_DEFAULT_THRESHOLD     2000

/* ── LCD ─────────────────────────────────────────────────────────────── */
#define LCD_COLS                 20
#define LCD_ROWS                 4

/* ── Task timing ─────────────────────────────────────────────────────── */
#define SENSOR_READ_INTERVAL_MS  100
#define LCD_UPDATE_INTERVAL_MS   150
#define SERIAL_LOG_INTERVAL_MS   10000
#define PROTECTION_CHECK_MS      50
#define HEARTBEAT_INTERVAL_MS    500
#define WATCHDOG_TIMEOUT_MS      2000
#define LED_BLINK_INTERVAL_MS    500

/* ── Protection defaults ─────────────────────────────────────────────── */
#define DEF_OVERCURRENT_A        50.0f
#define DEF_OVERVOLTAGE_V        260.0f
#define DEF_UNDERVOLTAGE_V       180.0f
#define DEF_OVERTEMP_C           80.0f
#define DEF_GAS_THRESHOLD        2500
#define FAULT_CONFIRM_COUNT      3
#define FAULT_HYSTERESIS_PCT     5

/* ── Filter ──────────────────────────────────────────────────────────── */
#define SENSOR_FILTER_SAMPLES    8

/* ── Data-integrity magic numbers (BUG 1 fix: all valid hex) ─────────── */
#define MAGIC_SENSOR_DATA        0x5E5D
#define MAGIC_CONFIG_DATA        0xC0FD
#define MAGIC_FAULT_LOG          0xFA17
#define MAGIC_CALIBRATION        0xCA1B

/* ── Peer protocol ───────────────────────────────────────────────────── */
#define PEER_PACKET_MAGIC        0xA55A
#define PEER_MAX_PACKET_SIZE     256

/* ── Hall RPM ────────────────────────────────────────────────────────── */
#define HALL_PULSES_PER_REV      1
#define HALL_RPM_TIMEOUT_MS      2000

/* ── Compile-time checks ─────────────────────────────────────────────── */
#if (I2C_PORT_ADC == I2C_PORT_SHARED)
  #error "I2C_PORT_ADC and I2C_PORT_SHARED must differ"
#endif
#if (I2C_PORT_ADC > 1) || (I2C_PORT_SHARED > 1)
  #error "ESP32-S3 only has I2C ports 0 and 1"
#endif

#endif /* APP_CONFIG_H */