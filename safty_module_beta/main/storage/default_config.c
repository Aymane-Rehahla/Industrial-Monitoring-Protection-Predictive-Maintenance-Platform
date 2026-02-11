/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  default_config.c - Factory Default Implementation                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "default_config.h"
#include "crc_utils.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              FACTORY DEFAULTS (ROM)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const protection_config_t FACTORY_DEFAULTS = {
    .magic = MAGIC_CONFIG_DATA,
    .version = 1,
    
    /* Current limits */
    .overcurrent_limit_A = DEFAULT_OVERCURRENT_LIMIT_A,
    .overcurrent_warning_A = DEFAULT_OVERCURRENT_WARNING_A,
    
    /* Voltage limits */
    .overvoltage_limit_V = DEFAULT_OVERVOLTAGE_LIMIT_V,
    .undervoltage_limit_V = DEFAULT_UNDERVOLTAGE_LIMIT_V,
    .voltage_warning_percent = DEFAULT_VOLTAGE_WARNING_PERCENT,
    
    /* Temperature limits */
    .overtemp_limit_C = DEFAULT_OVERTEMP_LIMIT_C,
    .overtemp_warning_C = DEFAULT_OVERTEMP_WARNING_C,
    
    /* Gas thresholds */
    .gas_alarm_threshold = DEFAULT_GAS_ALARM_THRESHOLD_RAW,
    .gas_warning_threshold = DEFAULT_GAS_WARNING_THRESHOLD_RAW,
    
    /* Timing */
    .trip_delay_ms = DEFAULT_CURRENT_TRIP_DELAY_MS,
    .retry_delay_ms = DEFAULT_RETRY_DELAY_MS,
    .max_retry_count = DEFAULT_MAX_RETRY_COUNT,
    
    .checksum = 0  /* Calculated at runtime if needed */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

const protection_config_t* default_config_get(void)
{
    return &FACTORY_DEFAULTS;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void default_config_copy(protection_config_t *config)
{
    if (config == NULL) {
        return;
    }
    
    memcpy(config, &FACTORY_DEFAULTS, sizeof(protection_config_t));
    
    /* Calculate fresh CRC */
    config->checksum = crc16_calculate(config, 
                                        sizeof(protection_config_t) - sizeof(uint16_t));
}

/* ─────────────────────────────────────────────────────────────────────────────── */

bool default_config_validate(const protection_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    
    /* Check magic */
    if (config->magic != MAGIC_CONFIG_DATA) {
        return false;
    }
    
    /* Current: 1A to 200A reasonable */
    if (config->overcurrent_limit_A < 1.0f || 
        config->overcurrent_limit_A > 200.0f) {
        return false;
    }
    
    /* Voltage: 100V to 500V reasonable for 3-phase */
    if (config->overvoltage_limit_V < 100.0f || 
        config->overvoltage_limit_V > 500.0f) {
        return false;
    }
    
    if (config->undervoltage_limit_V < 50.0f || 
        config->undervoltage_limit_V > 400.0f) {
        return false;
    }
    
    /* Under must be less than over */
    if (config->undervoltage_limit_V >= config->overvoltage_limit_V) {
        return false;
    }
    
    /* Temperature: -40 to 150°C reasonable */
    if (config->overtemp_limit_C < -40.0f || 
        config->overtemp_limit_C > 150.0f) {
        return false;
    }
    
    /* Gas: 0 to 4095 (12-bit ADC) */
    if (config->gas_alarm_threshold > 4095) {
        return false;
    }
    
    return true;
}