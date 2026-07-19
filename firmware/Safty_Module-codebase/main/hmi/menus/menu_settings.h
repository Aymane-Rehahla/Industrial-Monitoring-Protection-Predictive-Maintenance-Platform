// ═══ FILE: main/hmi/menus/menu_settings.h ═══
/**
 * @file    menu_settings.h
 * @brief   Shared data tables for settings-related screens.
 *          Sensor type info (names, units, step sizes) used by
 *          screen_threshold, screen_sensor_add, screen_sensor_test.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — data tables only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef MENU_SETTINGS_H
#define MENU_SETTINGS_H

#include "system_types.h"
#include <stdint.h>

/**
 * WHY this macro: The HD44780 degree symbol is byte 0xDF.  If we write
 * "\xDFC", the compiler tries to parse "DFC" as a 3-digit hex escape
 * (value 0xDFC = 3580, which overflows a char).  Splitting into two
 * adjacent string literals forces correct parsing: "\xDF" + "C".
 */
#define UNIT_STR_DEGC  "\xDF" "C"

/**
 * @brief  Info for one threshold-editable sensor type.
 */
typedef struct {
    sensor_type_t type;
    const char   *name;       /* Display name: "Voltage"              */
    const char   *unit_str;   /* Unit suffix: "V", "A", UNIT_STR_DEGC */
    float         step_small; /* Increment on single press             */
    float         step_large; /* Increment on held repeat              */
} threshold_type_info_t;

/** Threshold-editable sensor types with their display info. */
extern const threshold_type_info_t g_threshold_types[];

/** Number of entries in g_threshold_types[]. */
extern const uint32_t g_threshold_type_count;

/** Sensor types that users can add/remove at runtime. */
extern const sensor_type_t g_removable_sensor_types[];

/** Number of entries in g_removable_sensor_types[]. */
extern const uint32_t g_removable_type_count;

#endif /* MENU_SETTINGS_H */