// ═══ FILE: main/hmi/menus/menu_settings.c ═══
/**
 * @file    menu_settings.c
 * @brief   Shared data tables for settings screens.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — data tables only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/menus/menu_settings.h"
#include "system_types.h"

#include "esp_log.h"

const threshold_type_info_t g_threshold_types[] = {
    { SENSOR_VOLTAGE,   "Voltage",  "V",           1.0f,  10.0f  },
    { SENSOR_CURRENT,   "Current",  "A",           0.1f,   1.0f  },
    { SENSOR_TEMP,      "Temp",     UNIT_STR_DEGC, 1.0f,   5.0f  },
    { SENSOR_GAS_SMOKE, "Gas",      "ppm",        10.0f, 100.0f  },
    { SENSOR_VIBRATION, "Vibrate",  "g",           0.1f,   1.0f  },
    { SENSOR_RPM,       "RPM",      "RPM",        10.0f, 100.0f  },
};

const uint32_t g_threshold_type_count = ARRAY_SIZE(g_threshold_types);

const sensor_type_t g_removable_sensor_types[] = {
    SENSOR_TEMP,
    SENSOR_HUMIDITY,
    SENSOR_GAS_SMOKE,
    SENSOR_GAS_METHANE,
    SENSOR_GAS_CO,
};

const uint32_t g_removable_type_count = ARRAY_SIZE(g_removable_sensor_types);