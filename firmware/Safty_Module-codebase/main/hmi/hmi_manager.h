// ═══ FILE: main/hmi/hmi_manager.h ═══
/**
 * @file    hmi_manager.h
 * @brief   Central HMI coordinator.  Creates the HMI FreeRTOS task.
 *          Manages screen lifecycle, button dispatch, idle timeout,
 *          buzzer feedback, and LED status updates.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — HMI is not safety-critical.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef HMI_MANAGER_H
#define HMI_MANAGER_H

#include "system_types.h"
#include <stdbool.h>

/**
 * @brief  Initialise the HMI subsystem and create the HMI task.
 *
 * @pre    All HAL and driver init functions have been called.
 * @post   HMI task running on Core 0.  Boot screen displayed.
 * @return ERR_OK, ERR_HW_INIT_FAILED.
 * @wcet   < 50 ms (task creation + initial screen draw)
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t hmi_manager_init(void);

/**
 * @brief  Request transition to a specific screen.
 *
 * Can be called from ANY task (e.g., protection triggers SCREEN_FAULT).
 * Stored atomically; processed on next HMI tick.
 *
 * @pre    hmi_manager_init() called.
 * @post   Screen change queued.
 * @param  screen  Target screen ID.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG.
 * @wcet   < 1 µs (atomic write)
 * @thread_safety  Thread-safe (atomic variable).
 * @isr_safety     ISR-safe.
 */
error_code_t hmi_manager_request_screen(screen_id_t screen);

/** @brief Get the currently displayed screen. */
screen_id_t hmi_manager_get_current_screen(void);

/** @brief Check if HMI subsystem is initialised. */
bool hmi_manager_is_initialized(void);

/** @brief Get current HMI mode (OPERATOR or ENGINEER). */
hmi_mode_t hmi_manager_get_mode(void);

/** @brief Check if alarm buzzer is muted. */
bool hmi_manager_is_alarm_muted(void);

#endif /* HMI_MANAGER_H */