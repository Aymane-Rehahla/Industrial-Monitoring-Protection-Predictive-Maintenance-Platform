#ifndef DRV_VOLTAGE_SENSOR_H
#define DRV_VOLTAGE_SENSOR_H

#include "system_types.h"
#include <stdint.h>

error_code_t drv_voltage_sensor_init(void);
error_code_t drv_voltage_sensor_read_phase(uint8_t phase,
                                           sensor_reading_t *out);

#endif
