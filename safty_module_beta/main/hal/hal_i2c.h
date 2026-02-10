/**
 * @file hal_i2c.h
 * @brief I2C HAL for 2 buses. FROZEN.
 * @version 1.0.1
 * @safety MEDIUM
 *
 * BUG 2 fix: ESP32-S3 has only 2 I2C ports.
 *   Bus 0 (ADC):    ADS1115 ×2
 *   Bus 1 (SHARED): LCD + SHT45
 */
#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "system_types.h"

typedef enum {
    I2C_BUS_ADC    = 0,
    I2C_BUS_SHARED = 1,
    I2C_BUS_COUNT
} i2c_bus_id_t;

typedef struct {
    uint32_t tx_total;
    uint32_t tx_ok;
    uint32_t err_timeout;
    uint32_t err_nack;
    uint32_t err_bus;
    uint32_t bus_resets;
} i2c_stats_t;

error_code_t hal_i2c_init(void);
error_code_t hal_i2c_write(i2c_bus_id_t bus, uint8_t addr,
                           const uint8_t *data, size_t len);
error_code_t hal_i2c_read(i2c_bus_id_t bus, uint8_t addr,
                          uint8_t *data, size_t len);
error_code_t hal_i2c_write_read(i2c_bus_id_t bus, uint8_t addr,
                                const uint8_t *wr, size_t wr_len,
                                uint8_t *rd, size_t rd_len);
error_code_t hal_i2c_probe(i2c_bus_id_t bus, uint8_t addr, bool *found);
uint8_t      hal_i2c_scan(i2c_bus_id_t bus);
error_code_t hal_i2c_reset_bus(i2c_bus_id_t bus);
error_code_t hal_i2c_get_stats(i2c_bus_id_t bus, i2c_stats_t *out);
error_code_t hal_i2c_self_test(void);

#endif /* HAL_I2C_H */