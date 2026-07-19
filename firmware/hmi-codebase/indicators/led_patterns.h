/**
 * @file    led_patterns.h
 * @brief   Maps system state to LED patterns.
 * @version 1.0.0
 */

#ifndef LED_PATTERNS_H
#define LED_PATTERNS_H

#include "app_config.h"

error_code_t led_patterns_init(void);

/** @brief  Call from validation task when link/data state changes. */
void led_patterns_update(bool s3a_online, bool s3b_online,
                         bool peer_online, bool ipad_connected,
                         bool data_mismatch, bool fault_active,
                         bool fault_critical);

#endif /* LED_PATTERNS_H */