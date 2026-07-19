// ═══ FILE: main/system_types.h ═══
/**
 * @file    system_types.h
 * @brief   Single source of truth for all shared type definitions.
 *          Must compile standalone with only standard C headers.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  CRITICAL — Every module depends on these definitions.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release — all enums, structs, macros.
 */

#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  ERROR CODES
 *  Negative = error, zero = success.
 *  Grouped by subsystem so new codes don't collide.
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    ERR_OK                   =   0,

    /* Argument / state errors (-1 … -19) */
    ERR_INVALID_ARG          =  -1,
    ERR_NULL_POINTER         =  -2,
    ERR_NOT_INITIALIZED      =  -3,
    ERR_ALREADY_INITIALIZED  =  -4,
    ERR_BUFFER_FULL          =  -5,
    ERR_NOT_FOUND            =  -6,
    ERR_TIMEOUT              =  -7,

    /* Hardware errors (-20 … -39) */
    ERR_HW_INIT_FAILED       = -20,
    ERR_HW_NOT_FOUND         = -21,
    ERR_HW_TIMEOUT           = -22,
    ERR_HW_NACK              = -23,
    ERR_HW_WRITE_FAILED      = -24,
    ERR_HW_READ_FAILED       = -25,
    ERR_HW_CRC_FAILED        = -26,

    /* Safety errors (-40 … -59) */
    ERR_THRESHOLD_EXCEEDED   = -40,
    ERR_SENSOR_OFFLINE       = -41,
    ERR_CROSS_VALID_FAILED   = -42,
    ERR_HEARTBEAT_LOST       = -43,
    ERR_WATCHDOG_KEY_MISS    = -44,

    /* Storage errors (-60 … -79) */
    ERR_NVS_READ_FAILED      = -60,
    ERR_NVS_WRITE_FAILED     = -61,
    ERR_NVS_NOT_FOUND        = -62
} error_code_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SEVERITY
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    SEVERITY_INFO         = 0,
    SEVERITY_WARNING      = 1,
    SEVERITY_CRITICAL     = 2,
    SEVERITY_CATASTROPHIC = 3
} severity_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SECURITY MODE — protection tolerance level
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    SECURITY_HIGH   = 0,   /* HI — tight tolerances, trips fast       */
    SECURITY_NORMAL = 1,   /* NR — balanced                           */
    SECURITY_LOW    = 2,   /* LO — keep running, wide tolerances      */
    SECURITY_CUSTOM = 3    /* CU — user-defined thresholds            */
} security_mode_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SYSTEM STATE MACHINE
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    SYS_STATE_BOOT     = 0,
    SYS_STATE_CONFIG   = 1,
    SYS_STATE_VALIDATE = 2,
    SYS_STATE_READY    = 3,
    SYS_STATE_RUN      = 4,
    SYS_STATE_FAULT    = 5
} system_state_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  DEVICE ROLE  — auto-detected at boot (LCD present → INFORMER)
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    ROLE_UNKNOWN  = 0,
    ROLE_INFORMER = 1,   /* LCD present on Bus 1   */
    ROLE_SILENT   = 2    /* LCD absent on Bus 1     */
} device_role_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SENSOR TYPE IDENTIFIERS
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    SENSOR_NONE        =  0,
    SENSOR_VOLTAGE     =  1,
    SENSOR_CURRENT     =  2,
    SENSOR_TEMP        =  3,
    SENSOR_HUMIDITY    =  4,
    SENSOR_GAS_SMOKE   =  5,   /* MQ-2  */
    SENSOR_GAS_METHANE =  6,   /* MQ-4  */
    SENSOR_GAS_CO      =  7,   /* MQ-9  */
    SENSOR_VIBRATION   =  8,
    SENSOR_RPM         =  9,
    SENSOR_AUDIO       = 10,
    SENSOR_TYPE_COUNT         /* = 11, used as array dimension */
} sensor_type_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  MEASUREMENT UNITS
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    UNIT_RAW         = 0,
    UNIT_MILLIVOLTS  = 1,
    UNIT_VOLTS       = 2,
    UNIT_MILLIAMPS   = 3,
    UNIT_AMPS        = 4,
    UNIT_CELSIUS     = 5,
    UNIT_PERCENT_RH  = 6,
    UNIT_RPM         = 7,
    UNIT_G_FORCE     = 8,
    UNIT_PPM         = 9,
    UNIT_DECIBELS    = 10,
    UNIT_ADC_COUNT   = 11
} measurement_unit_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  DATA QUALITY — non-sequential values intentional (used as scores)
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    QUALITY_INVALID  =   0,
    QUALITY_DEGRADED =  50,
    QUALITY_GOOD     = 100
} data_quality_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  FAULT CODES — sparse enum, grouped by category
 *  Gaps are intentional to leave room for future codes per category.
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    FAULT_NONE              =  0,

    /* Sensor faults (1–15) */
    FAULT_OVERVOLTAGE       =  1,
    FAULT_UNDERVOLTAGE      =  2,
    FAULT_OVERCURRENT       =  3,
    FAULT_OVERTEMP          =  4,
    FAULT_GAS_LEVEL         =  5,
    FAULT_VIBRATION         =  6,
    FAULT_RPM_HIGH          =  7,
    FAULT_RPM_LOW           =  8,

    /* Communication faults (16–31) */
    FAULT_PEER_HEARTBEAT    = 16,
    FAULT_CROSS_VALIDATION  = 17,
    FAULT_ESPNOW_LINK       = 18,

    /* System faults (32–47) */
    FAULT_SENSOR_OFFLINE    = 32,
    FAULT_NVS_CORRUPT       = 33,
    FAULT_WATCHDOG          = 34,
    FAULT_RELAY_STUCK       = 35,

    /**
     * NOTE: FAULT_CODE_COUNT is NOT the number of defined codes.
     * It is the upper bound for arrays indexed by fault_code_t.
     * Some indices are unused (sparse enum).  Always range-check.
     */
    FAULT_CODE_COUNT        = 48
} fault_code_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  I2C BUS IDENTIFIERS
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    I2C_BUS_SENSORS = 0,   /* Bus 0 — ADS1115 x2 (safety-critical)  */
    I2C_BUS_SHARED  = 1,   /* Bus 1 — LCD + SHT45 (user-accessible) */
    I2C_BUS_COUNT          /* = 2 */
} i2c_bus_id_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SENSOR READING — with integrity fields for safety
 *
 *  Every reading carries its own validity proof:
 *    magic     — detects uninitialised memory
 *    checksum  — detects bit-rot / DMA corruption
 *    timestamp — detects stale data
 * ═══════════════════════════════════════════════════════════════════════ */
typedef struct {
    uint16_t         magic;          /* Must be MAGIC_SENSOR_DATA (0x5E5D) */
    uint32_t         timestamp_ms;   /* xTaskGetTickCount at read time     */
    int32_t          raw_value;      /* Raw ADC count or sensor register   */
    float            scaled_value;   /* Value in physical units            */
    measurement_unit_t unit;         /* What scaled_value represents       */
    data_quality_t   quality;        /* Validity classification            */
    uint8_t          error_count;    /* Consecutive read errors            */
    bool             is_valid;       /* Overall validity flag              */
    uint16_t         checksum;       /* CRC-16 of all preceding fields    */
} sensor_reading_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SENSOR THRESHOLD
 * ═══════════════════════════════════════════════════════════════════════ */
typedef struct {
    float high_limit;
    float low_limit;
    float hysteresis;
    bool  high_enabled;
    bool  low_enabled;
} sensor_threshold_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  BUTTON TYPES
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    BTN_LEFT  = 0,
    BTN_RIGHT = 1,
    BTN_UP    = 2,
    BTN_DOWN  = 3,
    BTN_OK    = 4,
    BTN_COUNT = 5,
    BTN_NONE  = 255
} button_id_t;

typedef enum {
    BTN_EVENT_NONE     = 0,
    BTN_EVENT_PRESSED  = 1,
    BTN_EVENT_RELEASED = 2,
    BTN_EVENT_HELD     = 3,
    BTN_EVENT_REPEAT   = 4
} button_event_type_t;

typedef struct {
    button_id_t        button;
    button_event_type_t event;
    uint32_t           hold_time_ms;
    uint32_t           timestamp_ms;
} button_event_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  LED TYPES
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    LED_RED   = 0,
    LED_RGB   = 1,
    LED_COUNT
} led_id_t;

typedef enum {
    LED_OFF        = 0,
    LED_ON         = 1,
    LED_BLINK_SLOW = 2,
    LED_BLINK_FAST = 3,
    LED_BLINK_SOS  = 4
} led_mode_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  BUZZER ACTIONS
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    BUZZER_SILENT  = 0,
    BUZZER_CLICK   = 1,
    BUZZER_NAV     = 2,
    BUZZER_CONFIRM = 3,
    BUZZER_WARNING = 4,
    BUZZER_ERROR   = 5,
    BUZZER_ALARM   = 6,
    BUZZER_STARTUP = 7,
    BUZZER_ACTION_COUNT  /* = 8 */
} buzzer_action_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SCREEN IDENTIFIERS
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    /* Fixed screens */
    SCREEN_BOOT          =  0,
    SCREEN_FAULT         =  1,

    /* Navigation-cycle screens (LEFT / RIGHT scrolls through these) */
    SCREEN_HOME          =  2,
    SCREEN_VOLTAGE       =  3,
    SCREEN_CURRENT       =  4,
    SCREEN_TEMP          =  5,
    SCREEN_GAS           =  6,
    SCREEN_VIBRATION     =  7,
    SCREEN_RPM           =  8,
    SCREEN_SYSTEM        =  9,

    /* Menu / settings screens */
    SCREEN_SETTINGS      = 10,
    SCREEN_THRESHOLD     = 11,
    SCREEN_SENSOR_VIEW   = 12,
    SCREEN_SENSOR_ADD    = 13,
    SCREEN_SENSOR_REMOVE = 14,
    SCREEN_SENSOR_TEST   = 15,
    SCREEN_CAL_SELECT    = 16,
    SCREEN_CAL_MANUAL    = 17,
    SCREEN_CAL_AUTO      = 18,
    SCREEN_PAIRING       = 19,
    SCREEN_MAC_ENTRY     = 20,
    SCREEN_DIAGNOSTICS   = 21,   /* NEW — diagnostics sub-menu       */
    SCREEN_INJECT_FAULT  = 22,   /* NEW — inject test fault selector */

    SCREEN_COUNT         = 23,   /* UPDATED from 21 to 23            */

    /* Navigation boundaries — used by LEFT/RIGHT button handler */
    SCREEN_NAV_FIRST     = SCREEN_HOME,
    SCREEN_NAV_LAST      = SCREEN_SYSTEM
} screen_id_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  HMI MODE
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    HMI_MODE_OPERATOR = 0,
    HMI_MODE_ENGINEER = 1
} hmi_mode_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  SENSOR ALARM STATE
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    SENSOR_ALARM_UNKNOWN = 0,
    SENSOR_ALARM_OK      = 1,
    SENSOR_ALARM_WARN    = 2,
    SENSOR_ALARM_TRIP    = 3
} sensor_alarm_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  TEMPERATURE TREND
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    TEMP_TREND_UNKNOWN = 0,
    TEMP_TREND_STABLE  = 1,
    TEMP_TREND_RISING  = 2,
    TEMP_TREND_FALLING = 3
} temp_trend_t;



/* ═══════════════════════════════════════════════════════════════════════
 *  COMM MESSAGE TYPES — used in packet header msg_type field
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    MSG_TELEM_FAST     = 0x01,   /* S3 → WROOM, 5 Hz                  */
    MSG_TELEM_SLOW     = 0x02,   /* S3 → WROOM, 1 Hz                  */
    MSG_FAULT_EVENT    = 0x03,   /* S3 → WROOM, immediate             */
    MSG_HEARTBEAT      = 0x04,   /* S3 ↔ WROOM, 2 Hz                  */
    MSG_CMD            = 0x05,   /* WROOM → S3, on demand             */
    MSG_CMD_ACK        = 0x06,   /* S3 → WROOM, response              */
    MSG_AUTH_CHALLENGE = 0x10,   /* S3 → WROOM, on connect            */
    MSG_AUTH_RESPONSE  = 0x11,   /* WROOM → S3, on connect            */
    MSG_CONFIG_DATA    = 0x12    /* S3 → WROOM, on request            */
} comm_msg_type_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  REMOTE COMMANDS — WROOM → S3
 * ═══════════════════════════════════════════════════════════════════════ */
typedef enum {
    CMD_ACK_ALARM      = 0x01,   /* Acknowledge active alarm           */
    CMD_RESET_FAULTS   = 0x02,   /* Clear forgivable faults            */
    CMD_REBOOT         = 0x03,   /* Reboot S3 node                     */
    CMD_REQUEST_CONFIG = 0x04,   /* Request threshold config           */
    CMD_EDIT_THRESHOLD = 0x05    /* Modify a threshold remotely        */
} comm_cmd_id_t;



/* ═══════════════════════════════════════════════════════════════════════
 *  UTILITY MACROS
 * ═══════════════════════════════════════════════════════════════════════ */

/** Number of elements in a statically-allocated array. */
#define ARRAY_SIZE(arr)   (sizeof(arr) / sizeof((arr)[0]))

/** Clamp x to [lo, hi].  Evaluates arguments only once. */
#define CLAMP(x, lo, hi) \
    ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

/** Suppress unused-variable warnings without hiding the name. */
#define UNUSED(x)         ((void)(x))

#endif /* SYSTEM_TYPES_H */