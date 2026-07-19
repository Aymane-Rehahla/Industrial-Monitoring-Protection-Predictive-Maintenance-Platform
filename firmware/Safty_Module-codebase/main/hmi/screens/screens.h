/**
 * @file    screens.h
 * @brief   Declares function signatures for ALL screen lifecycle operations.
 *          Each screen has 4 functions: enter, update, handle_event, exit.
 *          hmi_manager dispatches to the active screen via function pointers.
 *          Also provides static inline helper functions used by all screens.
 * @version 2.0.0
 * @date    2025-01-01
 * @safety  LOW — HMI screens are informational only.
 */

#ifndef SCREENS_H
#define SCREENS_H

#include "system_types.h"
#include "app_config.h"
#include "drivers/interface/drv_lcd2004.h"

#include <stdbool.h>
#include <stdio.h>

/* ── Function pointer types for screen lifecycle ─────────────────────── */
typedef void (*screen_enter_fn)(void);
typedef void (*screen_update_fn)(void);
typedef bool (*screen_event_fn)(const button_event_t *event);
typedef void (*screen_exit_fn)(void);

/**
 * @brief  Bundle of lifecycle functions for one screen.
 */
typedef struct {
    screen_enter_fn  enter;
    screen_update_fn update;
    screen_event_fn  handle_event;
    screen_exit_fn   exit;
} screen_handler_t;

/* ── Macro to declare all 4 lifecycle functions for a screen ─────────── */
#define DECLARE_SCREEN(name)                                        \
    void screen_##name##_enter(void);                               \
    void screen_##name##_update(void);                              \
    bool screen_##name##_handle_event(const button_event_t *e);     \
    void screen_##name##_exit(void);

/* ── Declare all 21 screens ──────────────────────────────────────────── */
DECLARE_SCREEN(boot)
DECLARE_SCREEN(fault)
DECLARE_SCREEN(home)
DECLARE_SCREEN(voltage)
DECLARE_SCREEN(current)
DECLARE_SCREEN(temp)
DECLARE_SCREEN(gas)
DECLARE_SCREEN(vibration)
DECLARE_SCREEN(rpm)
DECLARE_SCREEN(system)
DECLARE_SCREEN(settings)
DECLARE_SCREEN(threshold)
DECLARE_SCREEN(sensor_view)
DECLARE_SCREEN(sensor_add)
DECLARE_SCREEN(sensor_remove)
DECLARE_SCREEN(sensor_test)
DECLARE_SCREEN(cal_select)
DECLARE_SCREEN(cal_manual)
DECLARE_SCREEN(cal_auto)
DECLARE_SCREEN(pairing)
DECLARE_SCREEN(mac_entry)
DECLARE_SCREEN(diagnostics)     /* NEW */
DECLARE_SCREEN(inject_fault)    /* NEW */

/**
 * @brief  Check if boot animation has completed.
 * @return true if boot animation is done (or was skipped by user).
 */
bool screen_boot_is_done(void);

/* ═══════════════════════════════════════════════════════════════════════
 *  STATIC INLINE HELPER FUNCTIONS
 *  These MUST be visible to any .c file that includes screens.h
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Convert sensor_alarm_t to a 4-char display string.
 */
static inline const char *alarm_to_str(sensor_alarm_t a)
{
    switch (a) {
        case SENSOR_ALARM_OK:   return " OK ";
        case SENSOR_ALARM_WARN: return "WARN";
        case SENSOR_ALARM_TRIP: return "TRIP";
        default:                return " -- ";
    }
}

/**
 * @brief  Convert system_state_t to a short display string.
 */
static inline const char *state_to_str(system_state_t s)
{
    switch (s) {
        case SYS_STATE_BOOT:     return "BOOT";
        case SYS_STATE_CONFIG:   return "CONFIG";
        case SYS_STATE_VALIDATE: return "CHECK";
        case SYS_STATE_READY:    return "READY";
        case SYS_STATE_RUN:      return "RUN";
        case SYS_STATE_FAULT:    return "FAULT";
        default:                 return "???";
    }
}

/**
 * @brief  Convert peer_status_t (int) to a display string.
 */
static inline const char *peer_to_str(int p)
{
    switch (p) {
        case 1:  return "ONLINE";   /* PEER_ONLINE */
        case 2:  return "DEGRADED"; /* PEER_DEGRADED */
        case 3:  return "OFFLINE";  /* PEER_OFFLINE */
        default: return "UNKNOWN";
    }
}

/**
 * @brief  Convert fault_code_t to a display string (max 12 chars).
 */
static inline const char *fault_to_str(fault_code_t f)
{
    switch (f) {
        case FAULT_OVERVOLTAGE:      return "OVERVOLTAGE";
        case FAULT_UNDERVOLTAGE:     return "UNDERVOLTAGE";
        case FAULT_OVERCURRENT:      return "OVERCURRENT";
        case FAULT_OVERTEMP:         return "OVER TEMP";
        case FAULT_GAS_LEVEL:        return "GAS DETECTED";
        case FAULT_VIBRATION:        return "VIBRATION HI";
        case FAULT_RPM_HIGH:         return "RPM HIGH";
        case FAULT_RPM_LOW:          return "RPM LOW";
        case FAULT_PEER_HEARTBEAT:   return "PEER LOST";
        case FAULT_CROSS_VALIDATION: return "CROSS-VAL";
        case FAULT_ESPNOW_LINK:      return "ESPNOW LOST";
        case FAULT_SENSOR_OFFLINE:   return "SENSOR OFF";
        case FAULT_NVS_CORRUPT:      return "NVS CORRUPT";
        case FAULT_WATCHDOG:         return "WATCHDOG";
        case FAULT_RELAY_STUCK:      return "RELAY STUCK";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief  Convert severity_t to a 4-char display string.
 */
static inline const char *severity_to_str(severity_t s)
{
    switch (s) {
        case SEVERITY_INFO:         return "INFO";
        case SEVERITY_WARNING:      return "WARN";
        case SEVERITY_CRITICAL:     return "CRIT";
        case SEVERITY_CATASTROPHIC: return "!!!!";
        default:                    return "????";
    }
}

/**
 * @brief  Render row 0 with sensor title and alarm indicator.
 */
static inline void render_title_with_alarm(const char *title,
                                           sensor_alarm_t alarm,
                                           bool is_engineer)
{
    char line[LCD_COLS + 1];

    if (is_engineer) {
        snprintf(line, sizeof(line), " %-8s[E] [%4s]",
                 title, alarm_to_str(alarm));
    } else {
        snprintf(line, sizeof(line), " %-13s[%4s]",
                 title, alarm_to_str(alarm));
    }

    drv_lcd2004_write_line(0, line);
}

/**
 * @brief  Render row 3 with navigation hints.
 */
static inline void render_nav_bar(const char *label)
{
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "< %-16s >", label);
    drv_lcd2004_write_line(3, line);
}

#endif /* SCREENS_H */