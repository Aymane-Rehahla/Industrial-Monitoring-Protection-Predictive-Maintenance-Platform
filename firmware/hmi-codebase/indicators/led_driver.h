/**
 * @file    led_driver.h
 * @brief   GPIO control for 4 status LEDs.
 * @version 1.0.0
 */

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LED_GREEN  = 0,
    LED_YELLOW = 1,
    LED_ORANGE = 2,
    LED_RED    = 3
} led_id_t;

typedef enum {
    LED_OFF        = 0,
    LED_ON         = 1,
    LED_BLINK_SLOW = 2,
    LED_BLINK_FAST = 3
} led_mode_t;

error_code_t led_driver_init(void);
void led_driver_set(led_id_t led, led_mode_t mode);
void led_driver_all_off(void);
void led_driver_tick(void);  /* Call at LED_UPDATE_INTERVAL_MS */

#endif /* LED_DRIVER_H */