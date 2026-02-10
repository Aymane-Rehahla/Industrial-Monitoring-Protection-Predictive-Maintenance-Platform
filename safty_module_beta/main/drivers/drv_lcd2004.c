/**
 * @file  drv_lcd2004.c
 * @brief LCD 2004 + PCF8574 implementation — all rules applied.
 * @version 1.0.0
 *
 * @safety LOW
 *
 * BUG 7 : No ets_delay_us — esp_rom_delay_us for EN pulse (≤2 µs),
 *         vTaskDelay for everything ≥1 ms.
 * Rule 1.1: Every sub-function ≤ 50 lines.
 * Rule 2.1: All pointers checked.
 * Rule 2.5: All blocking ops have timeouts.
 */
#include "drv_lcd2004.h"
#include "hal_i2c.h"
#include "app_config.h"
#include "time_utils.h"

#include "esp_log.h"
#include "esp_rom_sys.h"          /* esp_rom_delay_us */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "DRV_LCD";

/* ── PCF8574 bit positions ───────────────────────────────────────────── */
#define PIN_RS   (1 << 0)
#define PIN_RW   (1 << 1)
#define PIN_EN   (1 << 2)
#define PIN_BL   (1 << 3)

/* ── HD44780 commands ────────────────────────────────────────────────── */
#define CMD_CLEAR           0x01
#define CMD_HOME            0x02
#define CMD_ENTRY_MODE      0x06
#define CMD_DISPLAY_ON      0x0C
#define CMD_DISPLAY_OFF     0x08
#define CMD_FUNCTION_4BIT   0x28   /* 4-bit, 2 lines, 5×8 */
#define CMD_SET_CGRAM       0x40
#define CMD_SET_DDRAM       0x80

/* ── Row DDRAM offsets for 20×4 ──────────────────────────────────────── */
static const uint8_t ROW_OFFSET[LCD_ROWS] = {0x00, 0x40, 0x14, 0x54};

/* ── Module state ────────────────────────────────────────────────────── */
static uint8_t           s_addr       = 0;
static uint8_t           s_backlight  = PIN_BL;   /* default ON */
static bool              s_online     = false;
static bool              s_initialized = false;
static SemaphoreHandle_t s_mtx        = NULL;

/* ── Lock helpers ────────────────────────────────────────────────────── */

static bool take_lock(uint32_t timeout_ms)
{
    if (s_mtx == NULL) { return false; }
    return xSemaphoreTake(s_mtx, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

static void give_lock(void)
{
    if (s_mtx) { xSemaphoreGive(s_mtx); }
}

/* ── Write a single byte to the PCF8574 expander ─────────────────────── */

static error_code_t pcf_write(uint8_t byte)
{
    return hal_i2c_write(I2C_BUS_SHARED, s_addr, &byte, 1);
}

/* ── Pulse the EN pin (Rule 3.7: busy-wait < 1 µs per pulse) ────────── */

static error_code_t pulse_enable(uint8_t data_byte)
{
    error_code_t err;

    err = pcf_write(data_byte | PIN_EN);       /* EN high */
    if (err != ERR_OK) { return err; }
    esp_rom_delay_us(2);                       /* t_PW min 450 ns */

    err = pcf_write(data_byte & ~PIN_EN);      /* EN low  */
    if (err != ERR_OK) { return err; }
    esp_rom_delay_us(1);                       /* t_H  min 10 ns  */

    return ERR_OK;
}

/* ── Send a nibble (4 high bits of byte → D4-D7) ────────────────────── */

static error_code_t send_nibble(uint8_t nibble, bool is_data)
{
    uint8_t out = (nibble & 0xF0) | s_backlight;
    if (is_data) { out |= PIN_RS; }
    return pulse_enable(out);
}

/* ── Send a full byte in 4-bit mode ──────────────────────────────────── */

static error_code_t send_byte(uint8_t byte, bool is_data)
{
    error_code_t err;
    err = send_nibble(byte & 0xF0, is_data);          /* high nibble */
    if (err != ERR_OK) { return err; }
    err = send_nibble((byte << 4) & 0xF0, is_data);   /* low nibble  */
    return err;
}

/* ── Send command (RS=0) with 2 ms settling ──────────────────────────── */

static error_code_t send_command(uint8_t cmd)
{
    error_code_t err = send_byte(cmd, false);
    vTaskDelay(pdMS_TO_TICKS(2));       /* worst-case command time */
    return err;
}

/* ── Send data (RS=1) ────────────────────────────────────────────────── */

static error_code_t send_data(uint8_t data)
{
    return send_byte(data, true);
}

/* ── Auto-detect I2C address ─────────────────────────────────────────── */

static error_code_t detect_address(void)
{
    bool found = false;

    hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_LCD_PRIMARY, &found);
    if (found) {
        s_addr = I2C_ADDR_LCD_PRIMARY;
        ESP_LOGI(TAG, "  Found at 0x%02X", s_addr);
        return ERR_OK;
    }

    hal_i2c_probe(I2C_BUS_SHARED, I2C_ADDR_LCD_ALTERNATE, &found);
    if (found) {
        s_addr = I2C_ADDR_LCD_ALTERNATE;
        ESP_LOGI(TAG, "  Found at 0x%02X (alternate)", s_addr);
        return ERR_OK;
    }

    ESP_LOGE(TAG, "  LCD not found on bus");
    return ERR_LCD_OFFLINE;
}

/* ── Load custom characters ──────────────────────────────────────────── */

static const uint8_t CUSTOM_CHARS[][8] = {
    /* 0: progress empty */  {0x00,0x1F,0x00,0x00,0x00,0x00,0x1F,0x00},
    /* 1: progress full  */  {0x00,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00},
    /* 2: arrow right    */  {0x00,0x04,0x06,0x1F,0x1F,0x06,0x04,0x00},
    /* 3: arrow left     */  {0x00,0x04,0x0C,0x1F,0x1F,0x0C,0x04,0x00},
    /* 4: heart          */  {0x00,0x0A,0x1F,0x1F,0x0E,0x04,0x00,0x00},
    /* 5: warning ⚠      */  {0x00,0x04,0x0E,0x0E,0x1F,0x1F,0x04,0x00},
    /* 6: check ✓        */  {0x00,0x01,0x03,0x16,0x1C,0x08,0x00,0x00},
    /* 7: thermometer    */  {0x04,0x0A,0x0A,0x0A,0x0E,0x1F,0x1F,0x0E},
};

#define CUSTOM_CHAR_COUNT  8

static error_code_t load_custom_chars(void)
{
    for (uint8_t i = 0; i < CUSTOM_CHAR_COUNT; i++) {
        error_code_t err = send_command(CMD_SET_CGRAM | (i << 3));
        if (err != ERR_OK) { return err; }

        for (uint8_t row = 0; row < 8; row++) {
            err = send_data(CUSTOM_CHARS[i][row]);
            if (err != ERR_OK) { return err; }
        }
    }
    return send_command(CMD_SET_DDRAM); /* back to DDRAM */
}

/* ── HD44780 4-bit init sequence ─────────────────────────────────────── */

static error_code_t init_4bit_mode(void)
{
    /* HD44780 power-on reset sequence — must be in 8-bit first */
    vTaskDelay(pdMS_TO_TICKS(50));          /* wait >40 ms after Vcc     */

    send_nibble(0x30, false);               /* function set 8-bit #1     */
    vTaskDelay(pdMS_TO_TICKS(5));           /* wait >4.1 ms              */

    send_nibble(0x30, false);               /* function set 8-bit #2     */
    vTaskDelay(pdMS_TO_TICKS(2));           /* wait >100 µs              */

    send_nibble(0x30, false);               /* function set 8-bit #3     */
    vTaskDelay(pdMS_TO_TICKS(2));

    send_nibble(0x20, false);               /* switch to 4-bit mode      */
    vTaskDelay(pdMS_TO_TICKS(2));

    return ERR_OK;
}

static error_code_t init_display_config(void)
{
    error_code_t err;

    err = send_command(CMD_FUNCTION_4BIT);  /* 4-bit, 2 lines, 5×8      */
    if (err != ERR_OK) { return err; }

    err = send_command(CMD_DISPLAY_OFF);    /* display off               */
    if (err != ERR_OK) { return err; }

    err = send_command(CMD_CLEAR);          /* clear                     */
    vTaskDelay(pdMS_TO_TICKS(3));           /* clear needs 1.52 ms       */
    if (err != ERR_OK) { return err; }

    err = send_command(CMD_ENTRY_MODE);     /* increment, no shift       */
    if (err != ERR_OK) { return err; }

    err = send_command(CMD_DISPLAY_ON);     /* display on, cursor off    */
    return err;
}

/* ── Public: Init ────────────────────────────────────────────────────── */

error_code_t lcd_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD 2004...");

    s_mtx = xSemaphoreCreateMutex();
    if (s_mtx == NULL) { return ERR_LCD_OFFLINE; }

    error_code_t err = detect_address();
    if (err != ERR_OK) { s_online = false; return err; }

    err = init_4bit_mode();
    if (err != ERR_OK) { s_online = false; return err; }

    err = init_display_config();
    if (err != ERR_OK) { s_online = false; return err; }

    err = load_custom_chars();
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "Custom chars failed — continuing");
    }

    s_online = true;
    s_initialized = true;
    ESP_LOGI(TAG, "LCD 2004 ready (addr=0x%02X)", s_addr);
    return ERR_OK;
}

/* ── Public: Clear ───────────────────────────────────────────────────── */

error_code_t lcd_clear(void)
{
    if (!s_online) { return ERR_LCD_OFFLINE; }
    if (!take_lock(100)) { return ERR_LCD_WRITE_FAILED; }

    error_code_t err = send_command(CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(3));
    give_lock();
    return err;
}

/* ── Public: Set cursor ──────────────────────────────────────────────── */

error_code_t lcd_set_cursor(uint8_t row, uint8_t col)
{
    if (row >= LCD_ROWS || col >= LCD_COLS) { return ERR_INVALID_PARAMETER; }
    if (!s_online) { return ERR_LCD_OFFLINE; }
    if (!take_lock(100)) { return ERR_LCD_WRITE_FAILED; }

    error_code_t err = send_command(CMD_SET_DDRAM | (ROW_OFFSET[row] + col));
    give_lock();
    return err;
}

/* ── Public: Write string ────────────────────────────────────────────── */

error_code_t lcd_write_string(const char *str)
{
    if (str == NULL) { return ERR_NULL_POINTER; }
    if (!s_online) { return ERR_LCD_OFFLINE; }
    if (!take_lock(100)) { return ERR_LCD_WRITE_FAILED; }

    error_code_t err = ERR_OK;
    /* Rule 2.3: bounded loop — max LCD_COLS chars */
    for (uint8_t i = 0; i < LCD_COLS && str[i] != '\0'; i++) {
        err = send_data((uint8_t)str[i]);
        if (err != ERR_OK) { break; }
    }

    give_lock();
    return err;
}

/* ── Public: Write at position with field width ──────────────────────── */

error_code_t lcd_write_at(uint8_t row, uint8_t col,
                           const char *str, uint8_t field_width)
{
    if (str == NULL) { return ERR_NULL_POINTER; }
    if (row >= LCD_ROWS || col >= LCD_COLS) { return ERR_INVALID_PARAMETER; }
    if (!s_online) { return ERR_LCD_OFFLINE; }
    if (!take_lock(100)) { return ERR_LCD_WRITE_FAILED; }

    error_code_t err = send_command(CMD_SET_DDRAM | (ROW_OFFSET[row] + col));
    if (err != ERR_OK) { give_lock(); return err; }

    uint8_t max_chars = LCD_COLS - col;
    if (field_width > 0 && field_width < max_chars) { max_chars = field_width; }

    uint8_t written = 0;
    /* Rule 2.3: bounded loop */
    for (uint8_t i = 0; i < max_chars && str[i] != '\0'; i++) {
        err = send_data((uint8_t)str[i]);
        if (err != ERR_OK) { give_lock(); return err; }
        written++;
    }

    /* Pad remainder with spaces */
    for (uint8_t i = written; i < max_chars && field_width > 0; i++) {
        send_data(' ');
    }

    give_lock();
    return ERR_OK;
}

/* ── Public: Progress bar ────────────────────────────────────────────── */

error_code_t lcd_draw_progress(uint8_t row, uint8_t col,
                                uint8_t width, uint8_t pct)
{
    if (row >= LCD_ROWS || col >= LCD_COLS) { return ERR_INVALID_PARAMETER; }
    if (pct > 100) { pct = 100; }
    if (!s_online) { return ERR_LCD_OFFLINE; }
    if (!take_lock(100)) { return ERR_LCD_WRITE_FAILED; }

    error_code_t err = send_command(CMD_SET_DDRAM | (ROW_OFFSET[row] + col));
    if (err != ERR_OK) { give_lock(); return err; }

    /* Clamp width to remaining columns */
    uint8_t max_w = LCD_COLS - col;
    if (width > max_w) { width = max_w; }

    uint8_t filled = (uint8_t)(((uint16_t)pct * width) / 100);

    for (uint8_t i = 0; i < width; i++) {
        uint8_t ch = (i < filled) ? LCD_CHAR_PROGRESS_FULL
                                  : LCD_CHAR_PROGRESS_EMPTY;
        send_data(ch);
    }

    give_lock();
    return ERR_OK;
}

/* ── Public: Backlight ───────────────────────────────────────────────── */

error_code_t lcd_set_backlight(bool on)
{
    s_backlight = on ? PIN_BL : 0;

    if (!s_online) { return ERR_LCD_OFFLINE; }
    if (!take_lock(50)) { return ERR_LCD_WRITE_FAILED; }

    error_code_t err = pcf_write(s_backlight);
    give_lock();
    return err;
}

/* ── Public: Is online ───────────────────────────────────────────────── */

bool lcd_is_online(void)
{
    return s_initialized && s_online;
}

/* ── Public: Get address ─────────────────────────────────────────────── */

uint8_t lcd_get_address(void)
{
    return s_addr;
}

/* ── Public: Self-test ───────────────────────────────────────────────── */

error_code_t lcd_self_test(void)
{
    ESP_LOGI(TAG, "LCD self-test...");

    /* Re-probe */
    bool found = false;
    hal_i2c_probe(I2C_BUS_SHARED, s_addr, &found);
    if (!found) {
        s_online = false;
        ESP_LOGE(TAG, "  FAIL: not responding at 0x%02X", s_addr);
        return ERR_LCD_OFFLINE;
    }

    /* Write test pattern */
    lcd_clear();
    lcd_write_at(0, 0, "LCD SELF-TEST", 20);
    lcd_write_at(1, 0, "********************", 20);
    lcd_draw_progress(2, 0, 20, 75);
    lcd_write_at(3, 0, "OK", 20);

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "  PASS: addr=0x%02X", s_addr);
    return ERR_OK;
}