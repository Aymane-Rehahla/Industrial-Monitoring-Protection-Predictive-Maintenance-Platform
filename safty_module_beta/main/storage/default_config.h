/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  default_config.h - Factory Default Configuration                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Rule 13.1: Factory defaults always available (cannot be corrupted)          ║
 * ║  Safety Level: CRITICAL                                                       ║
 * ║                                                                               ║
 * ║  These values are compiled into ROM. They survive everything.                ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#ifndef DEFAULT_CONFIG_H
#define DEFAULT_CONFIG_H

#include "system_types.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              PROTECTION DEFAULTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Current Protection */
#define DEFAULT_OVERCURRENT_LIMIT_A         50.0f
#define DEFAULT_OVERCURRENT_WARNING_A       45.0f
#define DEFAULT_CURRENT_TRIP_DELAY_MS       0       /* Instant */

/* Voltage Protection */
#define DEFAULT_OVERVOLTAGE_LIMIT_V         260.0f
#define DEFAULT_UNDERVOLTAGE_LIMIT_V        180.0f
#define DEFAULT_VOLTAGE_WARNING_PERCENT     5.0f

/* Temperature Protection */
#define DEFAULT_OVERTEMP_LIMIT_C            80.0f
#define DEFAULT_OVERTEMP_WARNING_C          70.0f

/* Gas Protection */
#define DEFAULT_GAS_ALARM_THRESHOLD_RAW     2500
#define DEFAULT_GAS_WARNING_THRESHOLD_RAW   2000

/* Timing */
#define DEFAULT_FAULT_CONFIRM_COUNT         3       /* 3 consecutive readings */
#define DEFAULT_HYSTERESIS_PERCENT          5       /* 5% to prevent chatter */
#define DEFAULT_RETRY_DELAY_MS              30000   /* 30s before auto-reset */
#define DEFAULT_MAX_RETRY_COUNT             3

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              FACTORY CONFIG STRUCT
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get pointer to factory default configuration (in ROM)
 * @return Const pointer to default protection_config_t
 */
const protection_config_t* default_config_get(void);

/**
 * @brief Copy factory defaults to a config struct
 * @param[out] config Destination config
 */
void default_config_copy(protection_config_t *config);

/**
 * @brief Validate a config against sane limits
 * @param[in] config Config to validate
 * @return true if all values within acceptable ranges
 */
bool default_config_validate(const protection_config_t *config);

#endif /* DEFAULT_CONFIG_H */