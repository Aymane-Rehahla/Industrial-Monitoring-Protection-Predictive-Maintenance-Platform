// ═══ FILE: main/hmi/screens/comm/screen_mac_entry.c ═══
/**
 * @file    screen_mac_entry.c
 * @brief   Hex digit editor for peer ESP32 MAC address.
 *          12 hex digits, navigate and edit individually.
 *          This is a custom hex editor — no menu_engine used.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — communication config, not safety-critical.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "drivers/actuators/drv_buzzer.h"
#include "core/redundancy/redundancy.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_mac_entry";

#define MAC_BYTES     6
#define MAC_DIGITS   12

/* Justification: MAC and cursor state must persist across calls.
 * File scope, HMI task only. */
static uint8_t  s_mac[MAC_BYTES];
static uint32_t s_digit_index;  /* 0–11: which hex digit is selected */

/**
 * @brief  Get the value of the currently selected hex digit (0–15).
 */
static uint8_t get_current_digit(void)
{
    uint8_t byte_val = s_mac[s_digit_index / 2];

    if ((s_digit_index % 2) == 0) {
        return (byte_val >> 4) & 0x0F;   /* High nibble. */
    }
    return byte_val & 0x0F;              /* Low nibble.  */
}

/**
 * @brief  Set the currently selected hex digit to a new value.
 */
static void set_current_digit(uint8_t digit)
{
    uint8_t idx = (uint8_t)(s_digit_index / 2);
    digit &= 0x0F;

    if ((s_digit_index % 2) == 0) {
        s_mac[idx] = (s_mac[idx] & 0x0F) | (uint8_t)(digit << 4);
    } else {
        s_mac[idx] = (s_mac[idx] & 0xF0) | digit;
    }
}

/**
 * @brief  Calculate the LCD column for the cursor '^' indicator.
 *
 * MAC displayed as: " AA:BB:CC:DD:EE:FF "
 * Starting at col 1, each byte is 2 hex chars + 1 colon separator.
 * Digit 0 → col 1, Digit 1 → col 2, Digit 2 → col 4, etc.
 */
static uint8_t digit_to_col(uint32_t digit_idx)
{
    uint32_t byte_idx   = digit_idx / 2;
    uint32_t nibble_pos = digit_idx % 2;

    return (uint8_t)(1 + byte_idx * 3 + nibble_pos);
}

void screen_mac_entry_enter(void)
{
    ESP_LOGI(TAG, "enter");
    redundancy_get_peer_mac(s_mac);
    s_digit_index = 0;
    drv_lcd2004_clear();
}

void screen_mac_entry_update(void)
{
    char buf[LCD_COLS + 1];

    drv_lcd2004_write_line(0, "  PEER MAC ENTRY  ");

    /* Row 1: MAC address formatted with colons. */
    snprintf(buf, sizeof(buf), " %02X:%02X:%02X:%02X:%02X:%02X ",
             s_mac[0], s_mac[1], s_mac[2],
             s_mac[3], s_mac[4], s_mac[5]);
    drv_lcd2004_write_line(1, buf);

    /* Row 2: Cursor indicator — '^' under the active digit. */
    memset(buf, ' ', LCD_COLS);
    buf[LCD_COLS] = '\0';
    uint8_t col = digit_to_col(s_digit_index);
    if (col < LCD_COLS) {
        buf[col] = '^';
    }
    drv_lcd2004_write_line(2, buf);

    drv_lcd2004_write_line(3, "L/R=Move   OK=Save");
}

bool screen_mac_entry_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED && e->event != BTN_EVENT_REPEAT) {
        return false;
    }

    switch (e->button) {
        case BTN_UP: {
            uint8_t d = (get_current_digit() + 1) & 0x0F;
            set_current_digit(d);
            drv_buzzer_play(BUZZER_CLICK);
            return true;
        }

        case BTN_DOWN: {
            uint8_t d = (get_current_digit() - 1) & 0x0F;
            set_current_digit(d);
            drv_buzzer_play(BUZZER_CLICK);
            return true;
        }

        case BTN_RIGHT:
            if (e->event != BTN_EVENT_PRESSED) { return true; }
            s_digit_index = (s_digit_index + 1) % MAC_DIGITS;
            return true;

        case BTN_LEFT:
            if (e->event != BTN_EVENT_PRESSED) { return true; }
            if (s_digit_index == 0) {
                return false;  /* Pop screen (back). */
            }
            s_digit_index--;
            return true;

        case BTN_OK:
            if (e->event != BTN_EVENT_PRESSED) { return true; }
            redundancy_set_peer_mac(s_mac);
            drv_buzzer_play(BUZZER_CONFIRM);
            ESP_LOGI(TAG, "MAC saved: %02X:%02X:%02X:%02X:%02X:%02X",
                     s_mac[0], s_mac[1], s_mac[2],
                     s_mac[3], s_mac[4], s_mac[5]);
            return false;  /* Pop screen. */

        default:
            return false;
    }
}

void screen_mac_entry_exit(void)
{
    /* Nothing to clean up. */
}