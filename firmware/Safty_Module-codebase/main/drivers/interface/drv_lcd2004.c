// ═══ FILE: main/drivers/interface/drv_lcd2004.c ═══
/**
 * @file    drv_lcd2004.c
 * @brief   HD44780 20×4 LCD driver over PCF8574 I2C backpack.
 *          Implements 4-bit mode with error tracking and auto-disable.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  MEDIUM — LCD failure must never affect safety chain.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "drivers/interface/drv_lcd2004.h"
#include "system_types.h"
#include "app_config.h"
#include "hal/hal_i2c.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rom/ets_sys.h"   /* ets_delay_us() for sub-ms init delays */

static const char *TAG = "drv_lcd2004";

/* ── PCF8574 bit positions ───────────────────────────────────────────── */
#define PCF_RS_BIT    (1U << 0)   /* Register Select: 0=cmd, 1=data      */
#define PCF_RW_BIT    (1U << 1)   /* Read/Write: always 0 (write only)   */
#define PCF_EN_BIT    (1U << 2)   /* Enable: pulse high→low to latch     */
#define PCF_BL_BIT    (1U << 3)   /* Backlight: 1=on                     */

/* ── HD44780 commands ────────────────────────────────────────────────── */
#define CMD_CLEAR_DISPLAY   0x01U
#define CMD_RETURN_HOME     0x02U
#define CMD_ENTRY_MODE      0x06U  /* Increment cursor, no shift          */
#define CMD_DISPLAY_ON      0x0CU  /* Display on, cursor off, blink off   */
#define CMD_FUNCTION_SET    0x28U  /* 4-bit, 2 lines, 5×8 font            */
#define CMD_SET_DDRAM       0x80U  /* OR with address                     */
#define CMD_SET_CGRAM       0x40U  /* OR with address                     */

/* ── DDRAM row offsets (20×4 LCD — rows are NOT sequential!) ─────────── */
static const uint8_t ROW_OFFSETS[LCD_ROWS] = {
    0x00,  /* Row 0: addresses 0x00–0x13 */
    0x40,  /* Row 1: addresses 0x40–0x53 */
    0x14,  /* Row 2: addresses 0x14–0x27 */
    0x54   /* Row 3: addresses 0x54–0x67 */
};

/* ── Module state ────────────────────────────────────────────────────── */

/* Justification: Tracks whether LCD was found and initialised. 
 * Must persist across all driver calls. File scope only. */
static bool     s_lcd_detected     = false;
static bool     s_lcd_initialized  = false;
static bool     s_lcd_disabled     = false;
static bool     s_backlight_on     = true;
static uint8_t  s_dev_addr         = 0;
static uint32_t s_consecutive_errs = 0;

/* ── Forward declarations ────────────────────────────────────────────── */
static error_code_t lcd_write_nibble(uint8_t nibble, bool is_data);
static error_code_t lcd_write_byte(uint8_t byte, bool is_data);
static error_code_t lcd_send_command(uint8_t cmd);
static error_code_t lcd_send_data(uint8_t data);
static void         lcd_track_error(error_code_t err);

/* ═══════════════════════════════════════════════════════════════════════
 *  lcd_track_error — Update consecutive error count, auto-disable.
 *
 *  WHY: I2C bus faults from a misbehaving LCD must not starve SHT45.
 *       After LCD_ERROR_THRESHOLD consecutive failures, we stop trying.
 * ═══════════════════════════════════════════════════════════════════════ */
static void lcd_track_error(error_code_t err)
{
    if (err == ERR_OK) {
        s_consecutive_errs = 0;
        return;
    }

    s_consecutive_errs++;
    if (s_consecutive_errs >= LCD_ERROR_THRESHOLD) {
        s_lcd_disabled = true;
        ESP_LOGE(TAG, "LCD disabled after %lu consecutive I2C errors",
                 (unsigned long)s_consecutive_errs);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  lcd_write_nibble — Send one nibble (4 bits) to the PCF8574.
 *
 *  Composes the PCF8574 byte with data in upper nibble plus control bits.
 *  Two I2C writes: EN=1 (latch), then EN=0 (complete pulse).
 *  WHY two writes: HD44780 reads data on falling edge of EN.
 * ═══════════════════════════════════════════════════════════════════════ */
static error_code_t lcd_write_nibble(uint8_t nibble, bool is_data)
{
    uint8_t pcf_byte = (nibble << 4) & 0xF0U;

    if (s_backlight_on) { pcf_byte |= PCF_BL_BIT; }
    if (is_data)        { pcf_byte |= PCF_RS_BIT; }
    /* RW bit always 0 — we only write. */

    /* EN=1: present data to HD44780. */
    uint8_t en_high = pcf_byte | PCF_EN_BIT;
    error_code_t rc = hal_i2c_write(I2C_BUS_SHARED, s_dev_addr,
                                    &en_high, 1, I2C_BUS1_TIMEOUT_MS);
    if (rc != ERR_OK) { return rc; }

    /* EN=0: falling edge latches data. */
    uint8_t en_low = pcf_byte & ~PCF_EN_BIT;
    rc = hal_i2c_write(I2C_BUS_SHARED, s_dev_addr,
                       &en_low, 1, I2C_BUS1_TIMEOUT_MS);
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  lcd_write_byte — Send a full byte as two nibbles (high then low).
 * ═══════════════════════════════════════════════════════════════════════ */
static error_code_t lcd_write_byte(uint8_t byte, bool is_data)
{
    /* High nibble first. */
    error_code_t rc = lcd_write_nibble((byte >> 4) & 0x0FU, is_data);
    if (rc != ERR_OK) { return rc; }

    /* Low nibble second. */
    rc = lcd_write_nibble(byte & 0x0FU, is_data);
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  lcd_send_command / lcd_send_data — Convenience wrappers.
 * ═══════════════════════════════════════════════════════════════════════ */
static error_code_t lcd_send_command(uint8_t cmd)
{
    return lcd_write_byte(cmd, false);
}

static error_code_t lcd_send_data(uint8_t data)
{
    return lcd_write_byte(data, true);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_init
 *
 *  WHY the long init sequence: HD44780 starts in an unknown state.
 *  The triple-0x03 + 0x02 sequence reliably forces 4-bit mode regardless
 *  of what state the controller was in (even after a soft reset).
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_init(void)
{
    s_lcd_detected     = false;
    s_lcd_initialized  = false;
    s_lcd_disabled     = false;
    s_consecutive_errs = 0;
    s_backlight_on     = true;

    /* Probe primary address. */
    if (hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_LCD2004) == ERR_OK) {
        s_dev_addr = I2C_ADDR_LCD2004;
        ESP_LOGI(TAG, "LCD found at 0x%02X", s_dev_addr);
    } else if (hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_LCD2004_ALT) == ERR_OK) {
        s_dev_addr = I2C_ADDR_LCD2004_ALT;
        ESP_LOGI(TAG, "LCD found at alternate 0x%02X", s_dev_addr);
    } else {
        ESP_LOGW(TAG, "No LCD detected — device is SILENT role");
        return ERR_HW_NOT_FOUND;
    }

    s_lcd_detected = true;

    /* HD44780 power-on init: wait >40ms after VCC rises. */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Force into 4-bit mode from any unknown state. */
    error_code_t rc;
    rc = lcd_write_nibble(0x03, false);  if (rc != ERR_OK) { goto fail; }
    vTaskDelay(pdMS_TO_TICKS(5));

    rc = lcd_write_nibble(0x03, false);  if (rc != ERR_OK) { goto fail; }
    vTaskDelay(pdMS_TO_TICKS(5));

    rc = lcd_write_nibble(0x03, false);  if (rc != ERR_OK) { goto fail; }
    ets_delay_us(1000);

    /* Switch to 4-bit interface. */
    rc = lcd_write_nibble(0x02, false);  if (rc != ERR_OK) { goto fail; }

    /* Function set: 4-bit, 2 lines, 5×8 font. */
    rc = lcd_send_command(CMD_FUNCTION_SET); if (rc != ERR_OK) { goto fail; }

    /* Display on, cursor off, blink off. */
    rc = lcd_send_command(CMD_DISPLAY_ON);   if (rc != ERR_OK) { goto fail; }

    /* Entry mode: increment cursor, no display shift. */
    rc = lcd_send_command(CMD_ENTRY_MODE);   if (rc != ERR_OK) { goto fail; }

    /* Clear display (takes ~1.52 ms). */
    rc = lcd_send_command(CMD_CLEAR_DISPLAY);if (rc != ERR_OK) { goto fail; }
    vTaskDelay(pdMS_TO_TICKS(2));

    s_lcd_initialized = true;
    ESP_LOGI(TAG, "LCD initialised (%dx%d) at 0x%02X",
             LCD_COLS, LCD_ROWS, s_dev_addr);
    return ERR_OK;

fail:
    ESP_LOGE(TAG, "LCD init sequence failed — I2C error");
    return ERR_HW_INIT_FAILED;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_clear
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_clear(void)
{
    if (!s_lcd_initialized) { return ERR_NOT_INITIALIZED; }
    if (s_lcd_disabled)     { return ERR_HW_WRITE_FAILED; }

    error_code_t rc = lcd_send_command(CMD_CLEAR_DISPLAY);
    lcd_track_error(rc);

    if (rc == ERR_OK) {
        /* Clear command requires ~1.52 ms to execute on HD44780. */
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_set_cursor
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_set_cursor(uint8_t row, uint8_t col)
{
    if (!s_lcd_initialized) { return ERR_NOT_INITIALIZED; }
    if (s_lcd_disabled)     { return ERR_HW_WRITE_FAILED; }
    if (row >= LCD_ROWS || col >= LCD_COLS) { return ERR_INVALID_ARG; }

    uint8_t addr = ROW_OFFSETS[row] + col;
    error_code_t rc = lcd_send_command(CMD_SET_DDRAM | addr);
    lcd_track_error(rc);
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_print
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_print(const char *str)
{
    if (str == NULL)        { return ERR_NULL_POINTER;     }
    if (!s_lcd_initialized) { return ERR_NOT_INITIALIZED;  }
    if (s_lcd_disabled)     { return ERR_HW_WRITE_FAILED;  }

    /* Bounded loop: never write more than one line width. */
    for (uint8_t i = 0; i < LCD_COLS && str[i] != '\0'; i++) {
        error_code_t rc = lcd_send_data((uint8_t)str[i]);
        lcd_track_error(rc);
        if (rc != ERR_OK) { return rc; }
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_print_at
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_print_at(uint8_t row, uint8_t col, const char *str)
{
    if (str == NULL) { return ERR_NULL_POINTER; }

    error_code_t rc = drv_lcd2004_set_cursor(row, col);
    if (rc != ERR_OK) { return rc; }

    return drv_lcd2004_print(str);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_write_line
 *
 *  WHY pad with spaces: The HD44780 does not have a "clear line" command.
 *  Without padding, old characters remain visible when a shorter string
 *  replaces a longer one.  This is the standard LCD anti-ghosting trick.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_write_line(uint8_t row, const char *str)
{
    if (str == NULL) { return ERR_NULL_POINTER; }
    if (row >= LCD_ROWS) { return ERR_INVALID_ARG; }

    error_code_t rc = drv_lcd2004_set_cursor(row, 0);
    if (rc != ERR_OK) { return rc; }

    /* Write exactly LCD_COLS characters: real chars then space padding. */
    bool past_end = false;
    for (uint8_t i = 0; i < LCD_COLS; i++) {
        if (!past_end && str[i] == '\0') { past_end = true; }
        uint8_t ch = past_end ? ' ' : (uint8_t)str[i];
        rc = lcd_send_data(ch);
        lcd_track_error(rc);
        if (rc != ERR_OK) { return rc; }
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_set_backlight
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_set_backlight(bool on)
{
    if (!s_lcd_initialized) { return ERR_NOT_INITIALIZED; }
    if (s_lcd_disabled)     { return ERR_HW_WRITE_FAILED; }

    s_backlight_on = on;

    /* Send a no-op I2C write to update the backlight bit immediately.
     * WHY: The BL bit is included in every nibble write, so sending
     * any byte with the correct BL state updates the backlight. */
    uint8_t bl_byte = on ? PCF_BL_BIT : 0x00U;
    error_code_t rc = hal_i2c_write(I2C_BUS_SHARED, s_dev_addr,
                                    &bl_byte, 1, I2C_BUS1_TIMEOUT_MS);
    lcd_track_error(rc);
    return rc;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_create_char
 *
 *  WHY: Custom characters let us show icons (thermometer, signal bars,
 *  warning triangle) on the otherwise ASCII-only display.
 *  CGRAM has 8 slots (0–7), each with an 8-byte pattern.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_lcd2004_create_char(uint8_t location,
                                     const uint8_t pattern[8])
{
    if (pattern == NULL) { return ERR_NULL_POINTER; }
    if (location > 7)    { return ERR_INVALID_ARG; }
    if (!s_lcd_initialized) { return ERR_NOT_INITIALIZED; }
    if (s_lcd_disabled)     { return ERR_HW_WRITE_FAILED; }

    /* Set CGRAM address: location × 8. */
    error_code_t rc = lcd_send_command(CMD_SET_CGRAM | (location << 3));
    lcd_track_error(rc);
    if (rc != ERR_OK) { return rc; }

    /* Write 8 rows of pattern data. */
    for (uint8_t row = 0; row < 8; row++) {
        rc = lcd_send_data(pattern[row] & 0x1FU);
        lcd_track_error(rc);
        if (rc != ERR_OK) { return rc; }
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_is_detected
 * ═══════════════════════════════════════════════════════════════════════ */
bool drv_lcd2004_is_detected(void)
{
    return s_lcd_detected;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_lcd2004_is_healthy
 * ═══════════════════════════════════════════════════════════════════════ */
bool drv_lcd2004_is_healthy(void)
{
    if (!s_lcd_initialized) { return false; }
    return !s_lcd_disabled;
}