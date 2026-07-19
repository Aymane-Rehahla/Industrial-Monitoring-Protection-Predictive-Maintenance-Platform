/**
 * @file    nvs_config.h
 * @brief   Non-volatile storage abstraction layer.
 *          Wraps ESP-IDF NVS API with error_code_t return values
 *          and a single namespace for all safety module data.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  HIGH — Stores thresholds, calibration, peer MACs.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief  Initialise NVS flash partition and open namespace.
 *
 * Calls nvs_flash_init().  If partition is corrupt or new layout,
 * erases and re-initialises automatically.
 *
 * @pre    None — call early in app_main(), before any NVS consumers.
 * @post   NVS ready for read/write operations.
 * @return ERR_OK on success, ERR_HW_INIT_FAILED on flash failure.
 * @wcet   < 200 ms (flash erase worst case).
 * @thread_safety  Not thread-safe — call once from app_main().
 */
error_code_t nvs_config_init(void);

/**
 * @brief  Read a uint8 value from NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @param  value    Pointer to receive the value.  Must not be NULL.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_NOT_FOUND, ERR_NVS_READ_FAILED.
 */
error_code_t nvs_config_read_u8(const char *key, uint8_t *value);

/**
 * @brief  Write a uint8 value to NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @param  value    Value to store.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_WRITE_FAILED.
 */
error_code_t nvs_config_write_u8(const char *key, uint8_t value);

/**
 * @brief  Read a uint32 value from NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @param  value    Pointer to receive the value.  Must not be NULL.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_NOT_FOUND, ERR_NVS_READ_FAILED.
 */
error_code_t nvs_config_read_u32(const char *key, uint32_t *value);

/**
 * @brief  Write a uint32 value to NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @param  value    Value to store.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_WRITE_FAILED.
 */
error_code_t nvs_config_write_u32(const char *key, uint32_t value);

/**
 * @brief  Read a binary blob from NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @param  buf      Buffer to receive data.  Must not be NULL.
 * @param  buf_size Size of buf in bytes.  On return, updated to actual size.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_NOT_FOUND, ERR_NVS_READ_FAILED,
 *         ERR_INVALID_ARG if buf_size too small.
 */
error_code_t nvs_config_read_blob(const char *key, void *buf,
                                  size_t *buf_size);

/**
 * @brief  Write a binary blob to NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @param  data     Data to store.  Must not be NULL.
 * @param  len      Number of bytes.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_WRITE_FAILED.
 */
error_code_t nvs_config_write_blob(const char *key, const void *data,
                                   size_t len);

/**
 * @brief  Erase a single key from NVS.
 *
 * @param  key      NVS key string.  Must not be NULL.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_NVS_NOT_FOUND.
 */
error_code_t nvs_config_erase_key(const char *key);

/**
 * @brief  Erase all keys in the safety namespace.  Factory reset.
 *
 * @return ERR_OK, ERR_NVS_WRITE_FAILED.
 */
error_code_t nvs_config_erase_all(void);

#endif /* NVS_CONFIG_H */