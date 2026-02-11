/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  nvs_config.h - NVS Configuration Storage                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Rule 4.4:  Triple redundant storage with voting                             ║
 * ║  Rule 4.7:  Validate before AND after NVS writes                             ║
 * ║  Rule 13.2: User config validated before saving                              ║
 * ║  Safety Level: HIGH                                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H

#include "system_types.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              TYPES
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    NVS_OK = 0,
    NVS_ERR_NOT_INITIALIZED,
    NVS_ERR_READ_FAILED,
    NVS_ERR_WRITE_FAILED,
    NVS_ERR_INVALID_DATA,
    NVS_ERR_CORRUPT,
    NVS_ERR_NO_SPACE,
} nvs_result_t;

typedef struct {
    uint32_t loads_total;
    uint32_t loads_success;
    uint32_t saves_total;
    uint32_t saves_success;
    uint32_t corruptions_detected;
    uint32_t factory_resets;
} nvs_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize NVS storage
 * @return NVS_OK on success
 */
nvs_result_t nvs_config_init(void);

/**
 * @brief Load protection config from NVS
 * @param[out] config Destination for loaded config
 * @return NVS_OK on success, factory defaults used on any error
 * 
 * @note If NVS is corrupt, this returns factory defaults and logs error
 */
nvs_result_t nvs_config_load(protection_config_t *config);

/**
 * @brief Save protection config to NVS
 * @param[in] config Config to save (will be validated)
 * @return NVS_OK on success
 * 
 * @note Rule 13.2: Config is validated before saving
 * @note Rule 4.7: Config is verified after saving
 */
nvs_result_t nvs_config_save(const protection_config_t *config);

/**
 * @brief Reset to factory defaults
 * @return NVS_OK on success
 * 
 * @note Clears NVS and writes factory defaults
 */
nvs_result_t nvs_config_factory_reset(void);

/**
 * @brief Check if valid config exists in NVS
 * @return true if valid config found
 */
bool nvs_config_exists(void);

/**
 * @brief Get NVS statistics
 */
nvs_result_t nvs_config_get_stats(nvs_stats_t *stats);

/**
 * @brief Save a single fault entry to fault log
 * @param[in] fault Fault to save
 * @return NVS_OK on success
 */
nvs_result_t nvs_config_save_fault(const fault_entry_t *fault);

/**
 * @brief Load fault log
 * @param[out] log Destination for fault log
 * @return NVS_OK on success
 */
nvs_result_t nvs_config_load_fault_log(fault_log_t *log);

/**
 * @brief Clear fault log
 */
nvs_result_t nvs_config_clear_fault_log(void);

#endif /* NVS_CONFIG_H */