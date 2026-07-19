/**
 * @file    auth_handler.h
 * @brief   XTEA challenge responder — WROOM side of auth handshake.
 * @version 1.0.0
 */

#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include "app_config.h"

error_code_t auth_handler_init(void);

#endif /* AUTH_HANDLER_H */