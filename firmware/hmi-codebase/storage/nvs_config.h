/**
 * @file    nvs_config.h
 * @brief   NVS abstraction — identical to S3 project.
 * @version 1.0.0
 */

#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int error_code_t;
#define ERR_OK                0
#define ERR_NULL_POINTER     -2
#define ERR_NOT_INITIALIZED  -3
#define ERR_INVALID_ARG      -1
#define ERR_HW_INIT_FAILED  -20
#define ERR_NVS_NOT_FOUND   -62
#define ERR_NVS_READ_FAILED -60
#define ERR_NVS_WRITE_FAILED -61

error_code_t nvs_config_init(void);
error_code_t nvs_config_read_u8(const char *key, uint8_t *value);
error_code_t nvs_config_write_u8(const char *key, uint8_t value);
error_code_t nvs_config_read_u32(const char *key, uint32_t *value);
error_code_t nvs_config_write_u32(const char *key, uint32_t value);
error_code_t nvs_config_read_blob(const char *key, void *buf, size_t *buf_size);
error_code_t nvs_config_write_blob(const char *key, const void *data, size_t len);
error_code_t nvs_config_erase_key(const char *key);
error_code_t nvs_config_erase_all(void);

#endif /* NVS_CONFIG_H */