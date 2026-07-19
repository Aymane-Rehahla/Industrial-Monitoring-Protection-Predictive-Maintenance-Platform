// ═══ FILE: main/hmi/ui_animations.c ═══
/**
 * @file    ui_animations.c
 * @brief   Custom LCD characters and animation helpers — real implementation.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — display only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/ui_animations.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"

#include <string.h>

/* ── Boot / progress bar custom characters ───────────────────────────── */
/* Slots 0-5: Bar fill levels (0/5 to 5/5 filled pixel columns).
 * Slots 6-7: Heart and checkmark icons (kept during boot). */

static const uint8_t BOOT_CHARS[8][8] = {
    /* Slot 0: Empty bar */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* Slot 1: 1/5 filled */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00},
    /* Slot 2: 2/5 filled */
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    /* Slot 3: 3/5 filled */
    {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x00},
    /* Slot 4: 4/5 filled */
    {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x00},
    /* Slot 5: Full block */
    {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x00},
    /* Slot 6: Heart */
    {0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00},
    /* Slot 7: Checkmark */
    {0x00, 0x01, 0x03, 0x16, 0x1C, 0x08, 0x00, 0x00},
};

/* ── Runtime icons (loaded after boot) ───────────────────────────────── */
static const uint8_t RUNTIME_CHARS[8][8] = {
    /* Slot 0: Up arrow */
    {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00},
    /* Slot 1: Down arrow */
    {0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00},
    /* Slot 2: Right arrow */
    {0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00},
    /* Slot 3: Lightning bolt */
    {0x01, 0x02, 0x04, 0x0F, 0x02, 0x04, 0x08, 0x00},
    /* Slot 4: Thermometer */
    {0x04, 0x0A, 0x0A, 0x0A, 0x0E, 0x1F, 0x1F, 0x0E},
    /* Slot 5: Warning triangle */
    {0x04, 0x04, 0x0E, 0x0E, 0x1F, 0x1F, 0x04, 0x00},
    /* Slot 6: Heart (same as boot) */
    {0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00},
    /* Slot 7: Checkmark (same as boot) */
    {0x00, 0x01, 0x03, 0x16, 0x1C, 0x08, 0x00, 0x00},
};

/* Spinner characters — standard ASCII, no custom chars needed. */
static const char SPINNER_CHARS[4] = {'|', '/', '-', '\\'};

/**
 * @brief  Load an array of 8 custom characters into CGRAM.
 */
static error_code_t load_charset(const uint8_t chars[8][8])
{
    for (uint8_t i = 0; i < 8; i++) {
        error_code_t rc = drv_lcd2004_create_char(i, chars[i]);
        if (rc != ERR_OK) { return rc; }
    }
    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ui_anim_load_boot_chars
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t ui_anim_load_boot_chars(void)
{
    return load_charset(BOOT_CHARS);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ui_anim_load_runtime_chars
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t ui_anim_load_runtime_chars(void)
{
    return load_charset(RUNTIME_CHARS);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ui_anim_draw_progress_bar
 *
 *  WHY 5 sub-segments per char: The HD44780 character cells are 5 pixels
 *  wide.  Using custom characters with 1-5 filled columns gives us
 *  5× the resolution of simple block characters.  A 20-char bar has
 *  100 possible fill levels instead of 20.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t ui_anim_draw_progress_bar(uint8_t row, uint8_t col,
                                       uint8_t width_chars, uint8_t percent)
{
    if (row >= LCD_ROWS) { return ERR_INVALID_ARG; }
    if (width_chars == 0 || (col + width_chars) > LCD_COLS) {
        return ERR_INVALID_ARG;
    }

    uint8_t pct = (percent > 100) ? 100 : percent;

    uint32_t total_segs  = (uint32_t)width_chars * 5U;
    uint32_t filled_segs = (pct * total_segs) / 100U;
    uint32_t full_chars  = filled_segs / 5U;
    uint32_t partial     = filled_segs % 5U;

    drv_lcd2004_set_cursor(row, col);

    /* Bounded loop: never exceeds width_chars iterations. */
    for (uint8_t pos = 0; pos < width_chars; pos++) {
        uint8_t ch;

        if (pos < full_chars) {
            ch = 5;   /* Full block (CGRAM slot 5) */
        } else if (pos == full_chars && partial > 0) {
            ch = (uint8_t)partial;  /* Partial fill (slot 1-4) */
        } else {
            ch = 0;   /* Empty (CGRAM slot 0) */
        }

        /* Custom chars are addressed as values 0-7 in HD44780. */
        drv_lcd2004_print_at(row, col + pos, (const char[]){(char)ch, '\0'});
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ui_anim_draw_spinner
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t ui_anim_draw_spinner(uint8_t row, uint8_t col, uint32_t frame)
{
    if (row >= LCD_ROWS || col >= LCD_COLS) { return ERR_INVALID_ARG; }

    char spin[2] = { SPINNER_CHARS[frame % 4U], '\0' };
    drv_lcd2004_print_at(row, col, spin);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ui_anim_center_text
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t ui_anim_center_text(uint8_t row, const char *text)
{
    if (text == NULL) { return ERR_NULL_POINTER; }
    if (row >= LCD_ROWS) { return ERR_INVALID_ARG; }

    uint8_t len = 0;
    /* Measure length, bounded by LCD_COLS. */
    while (len < LCD_COLS && text[len] != '\0') { len++; }

    char line[LCD_COLS + 1];
    memset(line, ' ', LCD_COLS);
    line[LCD_COLS] = '\0';

    uint8_t pad = (LCD_COLS > len) ? ((LCD_COLS - len) / 2U) : 0;

    /* Copy text into centered position. */
    for (uint8_t i = 0; i < len && (pad + i) < LCD_COLS; i++) {
        line[pad + i] = text[i];
    }

    drv_lcd2004_write_line(row, line);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ui_anim_typewriter
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t ui_anim_typewriter(uint8_t row, uint8_t col,
                                const char *text, uint32_t chars_visible)
{
    if (text == NULL) { return ERR_NULL_POINTER; }
    if (row >= LCD_ROWS || col >= LCD_COLS) { return ERR_INVALID_ARG; }

    uint8_t max_chars = LCD_COLS - col;
    char line[LCD_COLS + 1];

    uint8_t text_len = 0;
    while (text_len < max_chars && text[text_len] != '\0') { text_len++; }

    /* Fill with visible text + space padding. */
    for (uint8_t i = 0; i < max_chars; i++) {
        if (i < chars_visible && i < text_len) {
            line[i] = text[i];
        } else {
            line[i] = ' ';
        }
    }
    line[max_chars] = '\0';

    drv_lcd2004_print_at(row, col, line);

    return ERR_OK;
}