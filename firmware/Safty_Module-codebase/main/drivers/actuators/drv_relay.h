#ifndef DRV_RELAY_H
#define DRV_RELAY_H

#include "system_types.h"
#include <stdbool.h>

error_code_t drv_relay_init(void);
error_code_t drv_relay_set(bool on);
error_code_t drv_relay_get_commanded(bool *on_out);
error_code_t drv_relay_get_confirmed(bool *on_out);
void drv_relay_force_safe(void);

#endif
