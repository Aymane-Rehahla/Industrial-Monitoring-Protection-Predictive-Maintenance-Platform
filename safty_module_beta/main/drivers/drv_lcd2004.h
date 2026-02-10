/**
 * @file  drv_lcd2004.h
 * @brief LCD 2004 driver via PCF8574 I2C backpack.
 * @version 1.0.0
 *
 * @safety LOW
 * @hardware HD44780 20×4 + PCF8574 on I2C Bus 1 (SHARED), 0x27 or 0x3F
 *
 * Rule 6.9: Hardware — PCF8574 pin map:
 *   P0=RS  P1=RW  P2=EN  P3=BL  P4=D4  P5=D5  P6=D6  P7=D7
 *
 * Rule 14.9: Display remains readable during failover.
 */
#ifndef DRV_LCD2004_H
#define DRV_LCD2004_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

/* ── Custom character slots (HD44780 supports 0-7) ───────────────────── */
typedef enum {
    LCD_CHAR_PROGRESS_EMPTY = 0,
    LCD_CHAR_PROGRESS_FULL  = 1,
    LCD_CHAR_ARROW_RIGHT    = 2,
    LCD_CHAR_ARROW_LEFT     = 3,
    LCD_CHAR_HEART          = 4,
    LCD_CHAR_WARNING        = 5,
    LCD_CHAR_CHECK          = 6,
    LCD_CHAR_THERMOMETER    = 7,
} lcd_custom_char_t;

/**
 * @brief  Initialize LCD with auto-detect I2C address.
 * @return ERR_OK, ERR_LCD_OFFLINE
 * @wcet   200 ms | thread-safe NO | isr-safe NO
 */
error_code_t lcd_init(void);

/**
 * @brief  Clear entire display.
 * @return ERR_OK or ERR_LCD_OFFLINE
 * @wcet   5 ms | thread-safe YES (internal mutex) | isr-safe NO
 */
error_code_t lcd_clear(void);

/**
 * @brief  Set cursor position.
 * @param  row 0-3, col 0-19
 * @return ERR_OK, ERR_INVALID_PARAMETER, ERR_LCD_OFFLINE
 * @wcet   1 ms | thread-safe YES | isr-safe NO
 */
error_code_t lcd_set_cursor(uint8_t row, uint8_t col);

/**
 * @brief  Write string at current cursor position.
 * @param  str  Null-terminated string (truncated to remaining cols)
 * @return ERR_OK, ERR_NULL_POINTER, ERR_LCD_OFFLINE
 * @wcet   5 ms | thread-safe YES | isr-safe NO
 */
error_code_t lcd_write_string(const char *str);

/**
 * @brief  Write string at specific row/col, padded to field_width.
 * @param  row, col    Position
 * @param  str         String to write
 * @param  field_width Pad/truncate to this width (0 = no padding)
 * @return ERR_OK or error
 * @wcet   5 ms | thread-safe YES | isr-safe NO
 */
error_code_t lcd_write_at(uint8_t row, uint8_t col,
                           const char *str, uint8_t field_width);

/**
 * @brief  Draw a horizontal progress bar.
 * @param  row       Display row 0-3
 * @param  col       Start column
 * @param  width     Bar width in characters
 * @param  pct       Percentage 0-100
 * @return ERR_OK or error
 * @wcet   5 ms | thread-safe YES | isr-safe NO
 */
error_code_t lcd_draw_progress(uint8_t row, uint8_t col,
                                uint8_t width, uint8_t pct);

/**
 * @brief  Control backlight.
 * @param  on  true=backlight on
 * @return ERR_OK
 * @wcet   1 ms | thread-safe YES | isr-safe NO
 */
error_code_t lcd_set_backlight(bool on);

/**
 * @brief  Check if LCD is online.
 */
bool lcd_is_online(void);

/**
 * @brief  Get detected I2C address (0 if not found).
 */
uint8_t lcd_get_address(void);

/**
 * @brief  Self-test: write test pattern and verify bus.
 * @return ERR_OK or ERR_LCD_OFFLINE
 * @wcet   300 ms | thread-safe NO | isr-safe NO
 */
error_code_t lcd_self_test(void);

#endif /* DRV_LCD2004_H */