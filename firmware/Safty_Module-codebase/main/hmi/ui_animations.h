// ═══ FILE: main/hmi/ui_animations.h ═══
/**
 * @file    ui_animations.h
 * @brief   Custom LCD characters and animation helpers.
 *          Provides boot animation, progress bars, and special icons.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — display only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#ifndef UI_ANIMATIONS_H
#define UI_ANIMATIONS_H

#include "system_types.h"
#include <stdint.h>

/**
 * @brief  Load boot/progress-bar custom characters into CGRAM slots 0-7.
 *
 * @pre    drv_lcd2004_init() succeeded.
 * @post   CGRAM slots 0-7 contain bar + icon characters.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_HW_WRITE_FAILED.
 */
error_code_t ui_anim_load_boot_chars(void);

/**
 * @brief  Load runtime icons into CGRAM slots 0-7.
 *
 * Call after boot animation completes to replace bar chars with icons.
 *
 * @pre    drv_lcd2004_init() succeeded.
 * @post   CGRAM slots 0-7 contain runtime icon characters.
 * @return ERR_OK, ERR_NOT_INITIALIZED, ERR_HW_WRITE_FAILED.
 */
error_code_t ui_anim_load_runtime_chars(void);

/**
 * @brief  Draw a smooth progress bar using custom characters.
 *
 * @pre    ui_anim_load_boot_chars() has been called.
 * @post   Progress bar drawn at (row, col) spanning width_chars columns.
 * @param  row          LCD row (0-3).
 * @param  col          Starting column (0-19).
 * @param  width_chars  Bar width in characters.
 * @param  percent      Fill percentage 0-100.
 * @return ERR_OK, ERR_INVALID_ARG.
 */
error_code_t ui_anim_draw_progress_bar(uint8_t row, uint8_t col,
                                       uint8_t width_chars, uint8_t percent);

/**
 * @brief  Draw a spinning cursor (cycles: | / - \).
 *
 * @param  row    LCD row.
 * @param  col    LCD column.
 * @param  frame  Frame counter (modulo 4 selects character).
 * @return ERR_OK, ERR_INVALID_ARG.
 */
error_code_t ui_anim_draw_spinner(uint8_t row, uint8_t col, uint32_t frame);

/**
 * @brief  Write text centered on a 20-character LCD row.
 *
 * @param  row   LCD row (0-3).
 * @param  text  Text to center (must not be NULL).
 * @return ERR_OK, ERR_NULL_POINTER, ERR_INVALID_ARG.
 */
error_code_t ui_anim_center_text(uint8_t row, const char *text);

/**
 * @brief  Display partial text for typewriter animation effect.
 *
 * @param  row            LCD row.
 * @param  col            Starting column.
 * @param  text           Full text string.
 * @param  chars_visible  Number of characters to reveal.
 * @return ERR_OK, ERR_NULL_POINTER, ERR_INVALID_ARG.
 */
error_code_t ui_anim_typewriter(uint8_t row, uint8_t col,
                                const char *text, uint32_t chars_visible);

#endif /* UI_ANIMATIONS_H */