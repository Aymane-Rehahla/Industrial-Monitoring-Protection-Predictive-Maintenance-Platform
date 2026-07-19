// ═══ FILE: main/drivers/interface/drv_lcd2004.h ═══
/**
 * @file    drv_lcd2004.h
 * @brief   Driver for HD44780-based 20×4 LCD with PCF8574 I2C backpack.
 *          Lives on I2C_BUS_SHARED (Bus 1). NOT safety-critical.
 *          If LCD dies, machine protection continues unaffected.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — LCD failure must not affect safety chain or I2C Bus 1 health.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef DRV_LCD2004_H
#define DRV_LCD2004_H

#include "system_types.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Initialise the LCD2004 display.
 *
 * Probes I2C_ADDR_LCD2004 (0x27), then I2C_ADDR_LCD2004_ALT (0x3F).
 * If neither responds, returns ERR_HW_NOT_FOUND (device is SILENT role).
 * If found, runs HD44780 4-bit initialisation sequence.
 *
 * @pre    hal_i2c_init(I2C_BUS_SHARED) has been called.
 * @post   LCD cleared, backlight on, cursor off, ready for writing.
 * @return ERR_OK on success,
 *         ERR_HW_NOT_FOUND if no LCD detected (not fatal),
 *         ERR_HW_INIT_FAILED on I2C error during init sequence.
 * @wcet   ~80 ms (power-on delays + init commands)
 * @thread_safety  Not thread-safe — call once from app_main().
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_init(void);

/**
 * @brief  Clear the entire display.
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.
 * @post   Display cleared, cursor at (0,0).
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_HW_WRITE_FAILED.
 * @wcet   ~5 ms (clear command needs 1.52 ms execution)
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_clear(void);

/**
 * @brief  Set the cursor position.
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.
 * @post   Next character write will appear at (row, col).
 * @param  row  Row index 0–3.
 * @param  col  Column index 0–19.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_INVALID_ARG, ERR_HW_WRITE_FAILED.
 * @wcet   < 2 ms
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_set_cursor(uint8_t row, uint8_t col);

/**
 * @brief  Print a string at the current cursor position.
 *
 * Stops at null terminator or LCD_COLS, whichever comes first.
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.  str not NULL.
 * @post   String displayed from current cursor position.
 * @param  str  Null-terminated string to print.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_NULL_POINTER, ERR_HW_WRITE_FAILED.
 * @wcet   < 20 ms (worst case 20 characters)
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_print(const char *str);

/**
 * @brief  Print a string at a specific position.
 *
 * Convenience wrapper: set_cursor(row, col) then print(str).
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.  str not NULL.
 * @post   String displayed starting at (row, col).
 * @param  row  Row index 0–3.
 * @param  col  Column index 0–19.
 * @param  str  Null-terminated string to print.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_NULL_POINTER, ERR_INVALID_ARG,
 *         ERR_HW_WRITE_FAILED.
 * @wcet   < 22 ms
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_print_at(uint8_t row, uint8_t col, const char *str);

/**
 * @brief  Write exactly LCD_COLS characters to a row.
 *
 * Short strings are padded with spaces; long strings are truncated.
 * This prevents ghost characters from previous content.
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.  str not NULL.
 * @post   Entire row overwritten with str (padded/truncated).
 * @param  row  Row index 0–3.
 * @param  str  Null-terminated string to write.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_NULL_POINTER, ERR_INVALID_ARG,
 *         ERR_HW_WRITE_FAILED.
 * @wcet   < 25 ms (20 characters + cursor positioning)
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_write_line(uint8_t row, const char *str);

/**
 * @brief  Turn the backlight on or off.
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.
 * @post   Backlight state updated; takes effect on next I2C write.
 * @param  on  true = backlight on, false = off.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_HW_WRITE_FAILED.
 * @wcet   < 2 ms
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_set_backlight(bool on);

/**
 * @brief  Create a custom character in CGRAM.
 *
 * @pre    drv_lcd2004_init() returned ERR_OK.  pattern not NULL.
 * @post   Custom character stored at location (0–7).
 * @param  location  CGRAM slot 0–7.
 * @param  pattern   Array of 8 bytes (each byte = one row, 5 LSBs used).
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_NULL_POINTER, ERR_INVALID_ARG,
 *         ERR_HW_WRITE_FAILED.
 * @wcet   < 10 ms
 * @thread_safety  Not thread-safe — call from HMI task only.
 * @isr_safety     Not ISR-safe.
 */
error_code_t drv_lcd2004_create_char(uint8_t location,
                                     const uint8_t pattern[8]);

/**
 * @brief  Check if an LCD was detected during init.
 *
 * Used by application to determine INFORMER vs SILENT role.
 *
 * @return true if LCD responded to I2C probe.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe (reads static bool).
 * @isr_safety     ISR-safe.
 */
bool drv_lcd2004_is_detected(void);

/**
 * @brief  Check if the LCD is still healthy.
 *
 * @return true if consecutive error count < LCD_ERROR_THRESHOLD.
 * @wcet   < 1 µs
 * @thread_safety  Thread-safe (reads static counters).
 * @isr_safety     ISR-safe.
 */
bool drv_lcd2004_is_healthy(void);

#endif /* DRV_LCD2004_H */