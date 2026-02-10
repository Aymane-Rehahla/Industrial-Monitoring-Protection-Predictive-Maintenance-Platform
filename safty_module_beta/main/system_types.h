/**
 * @file system_types.h
 * @brief All shared type definitions. FROZEN after validation.
 * @version 1.0.0
 * @safety CRITICAL
 */
#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ── Error codes (BUG 3 fix: proper domains) ─────────────────────────── */
typedef enum {
    ERR_OK = 0,

    /* GPIO 0x08 */
    ERR_GPIO_INIT_FAILED     = 0x08,
    ERR_GPIO_INVALID_PIN     = 0x09,
    ERR_GPIO_NULL_POINTER    = 0x0A,
    ERR_GPIO_NOT_INIT        = 0x0B,
    ERR_GPIO_SELF_TEST_FAIL  = 0x0C,

    /* I2C 0x10 */
    ERR_I2C_INIT_FAILED      = 0x10,
    ERR_I2C_TIMEOUT          = 0x11,
    ERR_I2C_NACK             = 0x12,
    ERR_I2C_BUS_ERROR        = 0x13,

    /* ADC 0x20 */
    ERR_ADC_INIT_FAILED      = 0x20,
    ERR_ADC_READ_FAILED      = 0x21,
    ERR_ADC_OUT_OF_RANGE     = 0x22,
    ERR_ADC_RATE_EXCEEDED    = 0x23,

    /* Sensor 0x30 */
    ERR_SENSOR_OFFLINE       = 0x30,
    ERR_SENSOR_INVALID       = 0x31,
    ERR_SENSOR_TIMEOUT       = 0x32,
    ERR_SENSOR_MISMATCH      = 0x33,

    /* Protection 0x40 */
    ERR_OVERCURRENT          = 0x40,
    ERR_OVERVOLTAGE          = 0x41,
    ERR_UNDERVOLTAGE         = 0x42,
    ERR_OVERTEMP             = 0x43,
    ERR_GAS_DETECTED         = 0x44,

    /* System 0x50 */
    ERR_WATCHDOG_TIMEOUT     = 0x50,
    ERR_MEMORY_CORRUPT       = 0x51,
    ERR_CONFIG_INVALID       = 0x52,
    ERR_NVS_FAILED           = 0x53,

    /* Communication 0x60 */
    ERR_ESPNOW_FAILED        = 0x60,
    ERR_PEER_TIMEOUT         = 0x61,
    ERR_PEER_MISMATCH        = 0x62,

    /* UART 0x70 */
    ERR_UART_INIT_FAILED     = 0x70,
    ERR_UART_SEND_FAILED     = 0x71,
    ERR_UART_TIMEOUT         = 0x72,
    ERR_UART_INVALID_PACKET  = 0x73,

    /* LCD 0x80 */
    ERR_LCD_OFFLINE          = 0x80,
    ERR_LCD_WRITE_FAILED     = 0x81,

    /* Generic 0x90 */
    ERR_INVALID_PARAMETER    = 0x90,
    ERR_NULL_POINTER         = 0x91,
    ERR_NOT_INITIALIZED      = 0x92,
} error_code_t;

/* ── Severity ────────────────────────────────────────────────────────── */
typedef enum {
    SEVERITY_INFO         = 0,
    SEVERITY_WARNING      = 1,
    SEVERITY_CRITICAL     = 2,
    SEVERITY_CATASTROPHIC = 3,
} severity_t;

/* ── System state machine ────────────────────────────────────────────── */
typedef enum {
    STATE_BOOT = 0,
    STATE_INIT,
    STATE_SELF_TEST,
    STATE_READY,
    STATE_RUNNING,
    STATE_WARNING,
    STATE_FAULT,
    STATE_SAFE_MODE,
} system_state_t;

/* ── Data quality ────────────────────────────────────────────────────── */
typedef enum {
    QUALITY_INVALID  = 0,
    QUALITY_DEGRADED = 50,
    QUALITY_GOOD     = 100,
} data_quality_t;

/* ── Measurement units (BUG 4 fix) ───────────────────────────────────── */
typedef enum {
    UNIT_RAW = 0,
    UNIT_MILLIVOLTS,
    UNIT_VOLTS,
    UNIT_MILLIAMPS,
    UNIT_AMPS,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_RPM,
    UNIT_ADC_COUNT,
} measurement_unit_t;

/* ── Sensor reading (BUG 4 fix: checksum + unit) ─────────────────────── */
typedef struct {
    uint16_t           magic;
    uint32_t           timestamp_ms;
    int32_t            raw_value;
    float              scaled_value;
    measurement_unit_t unit;
    data_quality_t     quality;
    uint8_t            error_count;
    bool               is_valid;
    uint16_t           checksum;
} sensor_reading_t;

/* ── Button state ────────────────────────────────────────────────────── */
typedef struct {
    bool     is_pressed;
    bool     just_pressed;
    bool     just_released;
    bool     is_held;
    uint32_t press_start_ms;
    uint32_t hold_duration_ms;
} button_state_t;

typedef struct {
    button_state_t up;
    button_state_t down;
    button_state_t left;
    button_state_t right;
    button_state_t ok;
} buttons_t;

/* ── 3-phase structures ──────────────────────────────────────────────── */
typedef struct {
    sensor_reading_t L1;
    sensor_reading_t L2;
    sensor_reading_t L3;
    bool all_valid;
} phase3_reading_t;

/* ── Complete sensor set ─────────────────────────────────────────────── */
typedef struct {
    phase3_reading_t  voltage;
    phase3_reading_t  current;
    sensor_reading_t  temp_machine;
    sensor_reading_t  temp_ambient;
    sensor_reading_t  humidity;
    sensor_reading_t  gas_mq2;
    sensor_reading_t  gas_mq4;
    sensor_reading_t  gas_mq9;
    sensor_reading_t  audio_level;
    int32_t           audio_peak;
    float             rpm;
    volatile uint32_t hall_pulse_count;
    bool ads_voltage_online;
    bool ads_current_online;
    bool sht45_online;
    bool lcd_online;
    bool gas_warmed_up;
} sensor_set_t;

/* ── Fault log entry (BUG 4 fix: checksum) ───────────────────────────── */
typedef struct {
    uint16_t       magic;
    uint32_t       timestamp_ms;
    error_code_t   error_code;
    severity_t     severity;
    float          value_at_fault;
    float          threshold;
    system_state_t state_before;
    bool           was_cleared;
    uint16_t       checksum;
} fault_entry_t;

#define FAULT_LOG_SIZE 32

typedef struct {
    fault_entry_t entries[FAULT_LOG_SIZE];
    uint8_t       head;
    uint8_t       count;
    uint32_t      total_faults;
} fault_log_t;

/* ── Protection config ───────────────────────────────────────────────── */
typedef struct {
    uint16_t magic;
    uint16_t version;
    float    overcurrent_limit_A;
    float    overcurrent_warning_A;
    float    overvoltage_limit_V;
    float    undervoltage_limit_V;
    float    overtemp_limit_C;
    float    overtemp_warning_C;
    uint16_t gas_alarm_threshold;
    uint16_t gas_warning_threshold;
    uint16_t trip_delay_ms;
    uint16_t retry_delay_ms;
    uint8_t  max_retry_count;
    uint16_t checksum;
} protection_config_t;

/* ── Screen IDs ──────────────────────────────────────────────────────── */
typedef enum {
    SCREEN_HOME = 0,
    SCREEN_VOLTAGE,
    SCREEN_CURRENT,
    SCREEN_TEMPERATURE,
    SCREEN_GAS,
    SCREEN_SETTINGS,
    SCREEN_FAULT_LOG,
    SCREEN_COUNT,
} screen_id_t;

/* ── System status ───────────────────────────────────────────────────── */
typedef struct {
    system_state_t state;
    screen_id_t    current_screen;
    bool           screen_needs_redraw;
    uint32_t       uptime_seconds;
    bool           relay_enabled;
    bool           relay_commanded;
    uint32_t       error_count_total;
    uint32_t       warning_count_total;
} system_status_t;

/* ── UART peer packets ───────────────────────────────────────────────── */
typedef enum {
    PACKET_HEARTBEAT   = 0x01,
    PACKET_SENSOR_DATA = 0x02,
    PACKET_FAULT_ALERT = 0x03,
    PACKET_TRIP_CMD    = 0x04,
    PACKET_ACK         = 0x10,
} packet_type_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  type;
    uint8_t  sequence;
    uint16_t length;
} packet_header_t;

typedef struct __attribute__((packed)) {
    uint32_t uptime_ms;
    uint8_t  state;
    uint8_t  fault_count;
    uint8_t  relay_commanded;
    uint8_t  relay_enabled;
} heartbeat_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    int32_t  voltage_L1_mv;
    int32_t  voltage_L2_mv;
    int32_t  voltage_L3_mv;
    int32_t  current_L1_ma;
    int32_t  current_L2_ma;
    int32_t  current_L3_ma;
    int16_t  temp_machine_x10;
    int16_t  temp_ambient_x10;
    uint16_t gas_mq2;
    uint16_t gas_mq4;
    uint16_t gas_mq9;
    uint8_t  quality_flags;
} sensor_sync_payload_t;

typedef enum {
    PEER_UNKNOWN  = 0,
    PEER_ONLINE,
    PEER_DEGRADED,
    PEER_OFFLINE,
} peer_status_t;

/* ── Button IDs ──────────────────────────────────────────────────────── */
typedef enum {
    BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_OK, BTN_COUNT
} button_id_t;

/* ── LED IDs ─────────────────────────────────────────────────────────── */
typedef enum {
    LED_GREEN = 0, LED_RED, LED_COUNT
} led_id_t;

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_BLINK_SLOW,
    LED_MODE_BLINK_FAST,
    LED_MODE_BLINK_VERY_FAST,
} led_mode_t;

/* ── Gas sensor IDs ──────────────────────────────────────────────────── */
typedef enum {
    GAS_SENSOR_MQ2 = 0, GAS_SENSOR_MQ4, GAS_SENSOR_MQ9, GAS_SENSOR_COUNT
} gas_sensor_id_t;

/* ── Beep patterns ───────────────────────────────────────────────────── */
typedef enum {
    BEEP_CLICK = 0, BEEP_CONFIRM, BEEP_ERROR,
    BEEP_WARNING, BEEP_ALARM, BEEP_STARTUP, BEEP_PATTERN_COUNT
} beep_pattern_t;

/* ── Callbacks ───────────────────────────────────────────────────────── */
typedef void (*packet_callback_t)(packet_type_t type, const void *payload, size_t len);

#endif /* SYSTEM_TYPES_H */