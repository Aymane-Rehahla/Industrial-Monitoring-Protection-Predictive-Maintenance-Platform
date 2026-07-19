#ifndef DRV_ADS1115_H
#define DRV_ADS1115_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

#define ADS1115_CHANNEL_COUNT 4U

error_code_t drv_ads1115_init(uint8_t dev_addr);
bool drv_ads1115_is_present(uint8_t dev_addr);
error_code_t drv_ads1115_read_raw(uint8_t dev_addr,
                                  uint8_t channel,
                                  int16_t *raw_out);
error_code_t drv_ads1115_read_millivolts(uint8_t dev_addr,
                                         uint8_t channel,
                                         float *mv_out);

#endif
