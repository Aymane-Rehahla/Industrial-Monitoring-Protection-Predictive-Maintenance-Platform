/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  hmi_manager.h - Human-Machine Interface Manager                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Rule 14.1: Display shows system state clearly                               ║
 * ║  Rule 14.9: Display remains readable during failover                         ║
 * ║  Safety Level: LOW                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#ifndef HMI_MANAGER_H
#define HMI_MANAGER_H

#include "system_types.h"
#include "screens.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define HMI_UPDATE_INTERVAL_MS      150     /* ~7 Hz display refresh */
#define HMI_ALARM_BLINK_MS          250     /* Alarm blink rate */

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize HMI (LCD, buttons, LEDs, buzzer)
 * @return ERR_OK on success
 */
error_code_t hmi_manager_init(void);

/**
 * @brief Update HMI (call from main loop or task)
 * 
 * Handles:
 * - Button polling and debouncing
 * - Screen rendering at HMI_UPDATE_INTERVAL_MS
 * - LED patterns
 * - Buzzer patterns
 * - Alarm display
 */
void hmi_manager_update(void);

/**
 * @brief Navigate to a specific screen
 * @param screen Screen to navigate to
 */
void hmi_manager_set_screen(screen_id_t screen);

/**
 * @brief Get current screen
 * @return Current screen ID
 */
screen_id_t hmi_manager_get_screen(void);

/**
 * @brief Force full screen redraw on next update
 */
void hmi_manager_request_redraw(void);

/**
 * @brief Show alarm overlay (doesn't change current screen)
 * @param code Fault code to display
 * @param value Value that triggered fault
 */
void hmi_manager_show_alarm(error_code_t code, float value);

/**
 * @brief Clear alarm overlay
 */
void hmi_manager_clear_alarm(void);

/**
 * @brief Check if alarm is being displayed
 */
bool hmi_manager_has_alarm(void);

/**
 * @brief Set pointer to sensor data (for screens to read)
 * @param sensors Pointer to sensor_set_t (owned by caller)
 */
void hmi_manager_set_sensor_data(const sensor_set_t *sensors);

/**
 * @brief Set pointer to system status (for screens to read)
 * @param status Pointer to system_status_t (owned by caller)
 */
void hmi_manager_set_system_status(const system_status_t *status);

/**
 * @brief Set pointer to protection config (for settings screen)
 * @param config Pointer to protection_config_t (owned by caller)
 */
void hmi_manager_set_config(protection_config_t *config);

#endif /* HMI_MANAGER_H */