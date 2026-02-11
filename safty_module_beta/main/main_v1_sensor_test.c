/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  main_v1_sensor_test.c - Hardware Validation Firmware                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║  Version:      1.0.0                                                          ║
 * ║  Purpose:      Prove ALL hardware works before adding complexity              ║
 * ║  Method:       Simple polling loop, no FreeRTOS tasks                         ║
 * ║                                                                               ║
 * ║  WHAT THIS DOES:                                                              ║
 * ║  - Reads all sensors continuously                                            ║
 * ║  - Displays values on LCD (6 screens, navigate with buttons)                 ║
 * ║  - Prints diagnostics to serial every 2 seconds                              ║
 * ║  - Feeds ATtiny85 + NE555 heartbeat                                          ║
 * ║  - Blinks green LED when running                                             ║
 * ║                                                                               ║
 * ║  WHAT THIS DOES NOT DO:                                                       ║
 * ║  - No protection logic (relay stays OPEN)                                    ║
 * ║  - No ESP-NOW / peer communication                                           ║
 * ║  - No NVS storage                                                            ║
 * ║  - No settings editing                                                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

/* ESP-IDF */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

/* Project headers */
#include "app_config.h"
#include "system_types.h"
#include "time_utils.h"

/* HAL */
#include "hal_gpio.h"
#include "hal_i2c.h"
#include "hal_adc.h"

/* Drivers */
#include "drv_ads1115.h"
#include "drv_voltage_sensor.h"
#include "drv_current_sensor.h"
#include "drv_sht45.h"
#include "drv_lcd2004.h"
#include "drv_buttons.h"
#include "drv_leds.h"
#include "drv_buzzer.h"
#include "drv_hall_rpm.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *TAG = "V1_TEST";

/* Timing intervals (ms) */
#define LOOP_INTERVAL_MS            10      /* 100 Hz main loop */
#define SENSOR_READ_INTERVAL_MS     100     /* 10 Hz sensor reads */
#define DISPLAY_UPDATE_INTERVAL_MS  150     /* ~7 Hz display refresh */
#define HEARTBEAT_INTERVAL_MS       250     /* 4 Hz heartbeat (fast for NE555) */
#define SERIAL_LOG_INTERVAL_MS      2000    /* Every 2 seconds */
#define LED_BLINK_INTERVAL_MS       500     /* Green LED blink */

/* Screens */
#define SCREEN_HOME         0
#define SCREEN_VOLTAGE      1
#define SCREEN_CURRENT      2
#define SCREEN_TEMP         3
#define SCREEN_GAS          4
#define SCREEN_SYSTEM       5
#define SCREEN_COUNT        6

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              SENSOR DATA STORAGE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static struct {
    /* Voltage (3-phase) */
    voltage_reading_t voltage[3];
    bool voltage_ok;
    
    /* Current (3-phase) */
    current_reading_t current[3];
    bool current_ok;
    
    /* Temperature/Humidity (SHT45) */
    sht45_reading_t sht45;
    bool sht45_ok;
    
    /* Ambient temperature (LM35) */
    int32_t lm35_mv;
    float lm35_temp_c;
    bool lm35_ok;
    
    /* Gas sensors (raw ADC) */
    int32_t mq2_raw;
    int32_t mq4_raw;
    int32_t mq9_raw;
    bool gas_ok;
    
    /* Audio level */
    int32_t audio_raw;
    int32_t audio_peak;
    
    /* RPM */
    hall_reading_t rpm;
    bool rpm_ok;
    
} g_data;

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static struct {
    /* Timing */
    uint32_t last_sensor_ms;
    uint32_t last_display_ms;
    uint32_t last_serial_ms;
    uint32_t last_heartbeat_ms;
    uint32_t last_led_blink_ms;
    uint32_t boot_time_ms;
    
    /* Display */
    uint8_t current_screen;
    bool force_redraw;
    
    /* Heartbeat */
    bool heartbeat_state;
    
    /* Status */
    bool all_sensors_ok;
    uint32_t loop_count;
    
} g_state;

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              DRIVER HANDLES
 * ═══════════════════════════════════════════════════════════════════════════════ */

static ads1115_handle_t g_ads_voltage;
static ads1115_handle_t g_ads_current;

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              FORWARD DECLARATIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void init_hardware(void);
static void read_all_sensors(void);
static void update_display(void);
static void handle_buttons(void);
static void update_heartbeat(void);
static void print_serial_log(void);
static void render_screen(uint8_t screen);

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              MAIN ENTRY POINT
 * ═══════════════════════════════════════════════════════════════════════════════ */

void app_main(void)
{
    /* ═══════════════════════════════════════════════════════════════════════════
     * RULE 8.11: FIRST INSTRUCTION OPENS RELAY
     * Direct GPIO before any init - bypass all software
     * ═══════════════════════════════════════════════════════════════════════════ */
    gpio_reset_pin(PIN_RELAY_DRIVE);
    gpio_set_direction(PIN_RELAY_DRIVE, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RELAY_DRIVE, 0);  /* 0 = OPEN = SAFE */
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       SAFETY MODULE V1 - SENSOR TEST                      ║");
    ESP_LOGI(TAG, "║       Relay forced OPEN (safe state)                      ║");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    /* Initialize state */
    memset(&g_data, 0, sizeof(g_data));
    memset(&g_state, 0, sizeof(g_state));
    g_state.boot_time_ms = time_get_ms();
    g_state.force_redraw = true;
    
    /* Initialize all hardware */
    init_hardware();
    
    /* Show startup screen */
    drv_lcd_clear();
    drv_lcd_print_at(2, 0, "SAFETY MODULE V1");
    drv_lcd_print_at(3, 1, "SENSOR  TEST");
    drv_lcd_print_at(0, 3, "Initializing...");
    
    drv_buzzer_play(BEEP_STARTUP);
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    /* Ready */
    drv_lcd_print_at(0, 3, "Ready! Use L/R keys");
    drv_leds_set_mode(LED_GREEN, LED_MODE_BLINK_SLOW);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "  ENTERING MAIN LOOP - All systems GO");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    g_state.force_redraw = true;
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * MAIN LOOP - Simple polling, no FreeRTOS tasks
     * Target: 100 Hz (10ms per iteration)
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    while (1) {
        uint32_t now = time_get_ms();
        g_state.loop_count++;
        
        /* ─────────────────────────────────────────────────────────────────────
         * SENSORS: 10 Hz (every 100ms)
         * ───────────────────────────────────────────────────────────────────── */
        if (now - g_state.last_sensor_ms >= SENSOR_READ_INTERVAL_MS) {
            read_all_sensors();
            g_state.last_sensor_ms = now;
        }
        
        /* ─────────────────────────────────────────────────────────────────────
         * BUTTONS: Every loop (for responsiveness)
         * ───────────────────────────────────────────────────────────────────── */
        drv_buttons_update();
        handle_buttons();
        
        /* ─────────────────────────────────────────────────────────────────────
         * DISPLAY: ~7 Hz (every 150ms)
         * ───────────────────────────────────────────────────────────────────── */
        if (now - g_state.last_display_ms >= DISPLAY_UPDATE_INTERVAL_MS || 
            g_state.force_redraw) {
            update_display();
            g_state.last_display_ms = now;
            g_state.force_redraw = false;
        }
        
        /* ─────────────────────────────────────────────────────────────────────
         * HEARTBEAT: 4 Hz (every 250ms) - feed ATtiny + NE555
         * ───────────────────────────────────────────────────────────────────── */
        if (now - g_state.last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
            update_heartbeat();
            g_state.last_heartbeat_ms = now;
        }
        
        /* ─────────────────────────────────────────────────────────────────────
         * SERIAL LOG: Every 2 seconds
         * ───────────────────────────────────────────────────────────────────── */
        if (now - g_state.last_serial_ms >= SERIAL_LOG_INTERVAL_MS) {
            print_serial_log();
            g_state.last_serial_ms = now;
        }
        
        /* ─────────────────────────────────────────────────────────────────────
         * LED/BUZZER PATTERNS: Every loop
         * ───────────────────────────────────────────────────────────────────── */
        drv_leds_update();
        drv_buzzer_update();
        
        /* ─────────────────────────────────────────────────────────────────────
         * LOOP DELAY: 10ms for ~100 Hz
         * ───────────────────────────────────────────────────────────────────── */
        vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL_MS));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              HARDWARE INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void init_hardware(void)
{
    error_code_t err;
    
    ESP_LOGI(TAG, "┌─ Initializing Hardware ─────────────────────────────────┐");
    
    /* ── HAL Layer ────────────────────────────────────────────────────────── */
    
    ESP_LOGI(TAG, "│ HAL GPIO...");
    err = hal_gpio_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "│ HAL I2C (2 buses)...");
    err = hal_i2c_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "│ HAL ADC (internal)...");
    err = hal_adc_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    /* ── I2C Device Scan ──────────────────────────────────────────────────── */
    
    ESP_LOGI(TAG, "│");
    ESP_LOGI(TAG, "│ I2C Bus 0 (ADC sensors):");
    hal_i2c_scan(I2C_BUS_ADC);
    
    ESP_LOGI(TAG, "│ I2C Bus 1 (Display + Env):");
    hal_i2c_scan(I2C_BUS_DISPLAY);
    
    /* ── ADS1115 Initialization ───────────────────────────────────────────── */
    
    ESP_LOGI(TAG, "│");
    ESP_LOGI(TAG, "│ ADS1115 #1 (Voltage, 0x%02X)...", I2C_ADDR_ADS_VOLTAGE);
    err = ads1115_init(&g_ads_voltage, I2C_ADDR_ADS_VOLTAGE, ADS_GAIN_4096MV);
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "OFFLINE");
    
    ESP_LOGI(TAG, "│ ADS1115 #2 (Current, 0x%02X)...", I2C_ADDR_ADS_CURRENT);
    err = ads1115_init(&g_ads_current, I2C_ADDR_ADS_CURRENT, ADS_GAIN_4096MV);
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "OFFLINE");
    
    /* ── Sensor Drivers ───────────────────────────────────────────────────── */
    
    ESP_LOGI(TAG, "│");
    ESP_LOGI(TAG, "│ Voltage sensor...");
    err = voltage_sensor_init(&g_ads_voltage);
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "│ Current sensor...");
    err = current_sensor_init(&g_ads_current);
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "│ SHT45 (temp/humidity)...");
    err = drv_sht45_init();
    ESP_LOGI(TAG, "│   %s", drv_sht45_is_online() ? "OK" : "OFFLINE");
    
    ESP_LOGI(TAG, "│ Hall RPM sensor...");
    err = hall_rpm_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    /* ── HMI Drivers ──────────────────────────────────────────────────────── */
    
    ESP_LOGI(TAG, "│");
    ESP_LOGI(TAG, "│ LCD 2004...");
    err = drv_lcd_init();
    ESP_LOGI(TAG, "│   %s", drv_lcd_is_online() ? "OK" : "OFFLINE");
    
    ESP_LOGI(TAG, "│ Buttons...");
    err = drv_buttons_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "│ LEDs...");
    err = drv_leds_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "│ Buzzer...");
    err = drv_buzzer_init();
    ESP_LOGI(TAG, "│   %s", err == ERR_OK ? "OK" : "FAILED");
    
    ESP_LOGI(TAG, "└──────────────────────────────────────────────────────────┘");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              SENSOR READING
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void read_all_sensors(void)
{
    error_code_t err;
    
    /* ── Voltage (3-phase via ADS1115 #1) ─────────────────────────────────── */
    err = voltage_sensor_read_all(g_data.voltage);
    g_data.voltage_ok = (err == ERR_OK);
    
    /* ── Current (3-phase via ADS1115 #2) ─────────────────────────────────── */
    err = current_sensor_read_all(g_data.current);
    g_data.current_ok = (err == ERR_OK);
    
    /* ── SHT45 (Temperature + Humidity) ───────────────────────────────────── */
    if (drv_sht45_is_online()) {
        err = drv_sht45_read(&g_data.sht45);
        g_data.sht45_ok = (err == ERR_OK && g_data.sht45.is_valid);
    }
    
    /* ── LM35 (Ambient temperature via internal ADC) ──────────────────────── */
    adc_reading_t adc;
    err = hal_adc_read_filtered(ADC_CH_LM35, &adc);
    if (err == ERR_OK) {
        g_data.lm35_mv = adc.filtered_mv * 2;  /* Compensate divider */
        g_data.lm35_temp_c = (float)g_data.lm35_mv / 10.0f;  /* 10mV/°C */
        g_data.lm35_ok = true;
    }
    
    /* ── Gas sensors (MQ-2, MQ-4, MQ-9 via internal ADC) ──────────────────── */
    int32_t raw;
    
    err = hal_adc_read_mv(ADC_CH_MQ2, &raw);
    g_data.mq2_raw = (err == ERR_OK) ? raw * 2 : 0;  /* Compensate divider */
    
    err = hal_adc_read_mv(ADC_CH_MQ4, &raw);
    g_data.mq4_raw = (err == ERR_OK) ? raw * 2 : 0;
    
    err = hal_adc_read_mv(ADC_CH_MQ9, &raw);
    g_data.mq9_raw = (err == ERR_OK) ? raw * 2 : 0;
    
    g_data.gas_ok = true;
    
    /* ── Audio level (MAX9814 - no divider) ───────────────────────────────── */
    err = hal_adc_read_mv(ADC_CH_MAX9814, &g_data.audio_raw);
    if (err == ERR_OK && g_data.audio_raw > g_data.audio_peak) {
        g_data.audio_peak = g_data.audio_raw;
    }
    
    /* ── RPM (Hall sensor) ────────────────────────────────────────────────── */
    err = hall_rpm_read(&g_data.rpm);
    g_data.rpm_ok = (err == ERR_OK);
    
    /* ── Update overall status ────────────────────────────────────────────── */
    g_state.all_sensors_ok = g_data.voltage_ok && g_data.current_ok;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              BUTTON HANDLING
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void handle_buttons(void)
{
    /* Navigate screens with LEFT/RIGHT */
    if (drv_buttons_just_pressed(BTN_LEFT)) {
        drv_buzzer_play(BEEP_CLICK);
        if (g_state.current_screen == 0) {
            g_state.current_screen = SCREEN_COUNT - 1;
        } else {
            g_state.current_screen--;
        }
        g_state.force_redraw = true;
    }
    
    if (drv_buttons_just_pressed(BTN_RIGHT)) {
        drv_buzzer_play(BEEP_CLICK);
        g_state.current_screen = (g_state.current_screen + 1) % SCREEN_COUNT;
        g_state.force_redraw = true;
    }
    
    /* OK button - currently just beeps (could trigger calibration in future) */
    if (drv_buttons_just_pressed(BTN_OK)) {
        drv_buzzer_play(BEEP_CONFIRM);
        /* Reset audio peak on OK press */
        g_data.audio_peak = 0;
    }
    
    /* UP/DOWN - could be used for scrolling within a screen */
    if (drv_buttons_just_pressed(BTN_UP)) {
        drv_buzzer_play(BEEP_CLICK);
    }
    
    if (drv_buttons_just_pressed(BTN_DOWN)) {
        drv_buzzer_play(BEEP_CLICK);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              DISPLAY UPDATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void update_display(void)
{
    if (!drv_lcd_is_online()) {
        return;
    }
    
    render_screen(g_state.current_screen);
}

static void render_screen(uint8_t screen)
{
    char line[21];  /* 20 chars + null */
    
    switch (screen) {
        
    /* ═══════════════════════════════════════════════════════════════════════
     * SCREEN 0: HOME (Overview)
     * ═══════════════════════════════════════════════════════════════════════ */
    case SCREEN_HOME:
        drv_lcd_print_at(0, 0, "\x7E SAFETY MODULE V1 \x7F");  /* Arrows: ►  ◄ */
        
        /* Voltage line */
        snprintf(line, sizeof(line), "V:%3.0f %3.0f %3.0f V    ",
                 g_data.voltage[0].voltage_rms_v,
                 g_data.voltage[1].voltage_rms_v,
                 g_data.voltage[2].voltage_rms_v);
        drv_lcd_print_at(0, 1, line);
        
        /* Current line */
        snprintf(line, sizeof(line), "I:%4.1f%4.1f%4.1f A  ",
                 g_data.current[0].current_a,
                 g_data.current[1].current_a,
                 g_data.current[2].current_a);
        drv_lcd_print_at(0, 2, line);
        
        /* Temp + RPM line */
        snprintf(line, sizeof(line), "T:%4.1fC  RPM:%5.0f",
                 g_data.sht45_ok ? g_data.sht45.temperature_c : g_data.lm35_temp_c,
                 g_data.rpm.rpm);
        drv_lcd_print_at(0, 3, line);
        break;
        
    /* ═══════════════════════════════════════════════════════════════════════
     * SCREEN 1: VOLTAGE (Detail)
     * ═══════════════════════════════════════════════════════════════════════ */
    case SCREEN_VOLTAGE:
        drv_lcd_print_at(0, 0, "== VOLTAGE (Vrms) ==");
        
        snprintf(line, sizeof(line), "L1: %6.1f V  %s",
                 g_data.voltage[0].voltage_rms_v,
                 g_data.voltage[0].is_valid ? "OK" : "--");
        drv_lcd_print_at(0, 1, line);
        
        snprintf(line, sizeof(line), "L2: %6.1f V  %s",
                 g_data.voltage[1].voltage_rms_v,
                 g_data.voltage[1].is_valid ? "OK" : "--");
        drv_lcd_print_at(0, 2, line);
        
        snprintf(line, sizeof(line), "L3: %6.1f V  %s",
                 g_data.voltage[2].voltage_rms_v,
                 g_data.voltage[2].is_valid ? "OK" : "--");
        drv_lcd_print_at(0, 3, line);
        break;
        
    /* ═══════════════════════════════════════════════════════════════════════
     * SCREEN 2: CURRENT (Detail)
     * ═══════════════════════════════════════════════════════════════════════ */
    case SCREEN_CURRENT:
        drv_lcd_print_at(0, 0, "== CURRENT (Arms) ==");
        
        snprintf(line, sizeof(line), "L1: %6.2f A  %s",
                 g_data.current[0].current_a,
                 g_data.current[0].is_valid ? "OK" : "--");
        drv_lcd_print_at(0, 1, line);
        
        snprintf(line, sizeof(line), "L2: %6.2f A  %s",
                 g_data.current[1].current_a,
                 g_data.current[1].is_valid ? "OK" : "--");
        drv_lcd_print_at(0, 2, line);
        
        snprintf(line, sizeof(line), "L3: %6.2f A  %s",
                 g_data.current[2].current_a,
                 g_data.current[2].is_valid ? "OK" : "--");
        drv_lcd_print_at(0, 3, line);
        break;
        
    /* ═══════════════════════════════════════════════════════════════════════
     * SCREEN 3: TEMPERATURE
     * ═══════════════════════════════════════════════════════════════════════ */
    case SCREEN_TEMP:
        drv_lcd_print_at(0, 0, "== TEMPERATURE =====");
        
        if (g_data.sht45_ok) {
            snprintf(line, sizeof(line), "SHT45: %5.1f C      ",
                     g_data.sht45.temperature_c);
        } else {
            snprintf(line, sizeof(line), "SHT45: OFFLINE      ");
        }
        drv_lcd_print_at(0, 1, line);
        
        snprintf(line, sizeof(line), "LM35:  %5.1f C      ",
                 g_data.lm35_temp_c);
        drv_lcd_print_at(0, 2, line);
        
        if (g_data.sht45_ok) {
            snprintf(line, sizeof(line), "Humid: %5.1f %%      ",
                     g_data.sht45.humidity_percent);
        } else {
            snprintf(line, sizeof(line), "Humid: --           ");
        }
        drv_lcd_print_at(0, 3, line);
        break;
        
    /* ═══════════════════════════════════════════════════════════════════════
     * SCREEN 4: GAS SENSORS
     * ═══════════════════════════════════════════════════════════════════════ */
    case SCREEN_GAS:
        drv_lcd_print_at(0, 0, "== GAS SENSORS (mV)==");
        
        snprintf(line, sizeof(line), "MQ2 (Smoke): %4ld   ", (long)g_data.mq2_raw);
        drv_lcd_print_at(0, 1, line);
        
        snprintf(line, sizeof(line), "MQ4 (CH4):   %4ld   ", (long)g_data.mq4_raw);
        drv_lcd_print_at(0, 2, line);
        
        snprintf(line, sizeof(line), "MQ9 (CO):    %4ld   ", (long)g_data.mq9_raw);
        drv_lcd_print_at(0, 3, line);
        break;
        
    /* ═══════════════════════════════════════════════════════════════════════
     * SCREEN 5: SYSTEM INFO
     * ═══════════════════════════════════════════════════════════════════════ */
    case SCREEN_SYSTEM:
        drv_lcd_print_at(0, 0, "== SYSTEM INFO =====");
        
        drv_lcd_print_at(0, 1, "FW: V1 SENSOR TEST  ");
        
        uint32_t uptime_s = (time_get_ms() - g_state.boot_time_ms) / 1000;
        uint32_t hours = uptime_s / 3600;
        uint32_t mins = (uptime_s % 3600) / 60;
        uint32_t secs = uptime_s % 60;
        snprintf(line, sizeof(line), "Up: %02lu:%02lu:%02lu        ",
                 (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
        drv_lcd_print_at(0, 2, line);
        
        uint32_t heap = esp_get_free_heap_size() / 1024;
        snprintf(line, sizeof(line), "Heap: %lu KB free   ", (unsigned long)heap);
        drv_lcd_print_at(0, 3, line);
        break;
        
    default:
        g_state.current_screen = SCREEN_HOME;
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              HEARTBEAT
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void update_heartbeat(void)
{
    /* Toggle heartbeat pin - feeds ATtiny85 and NE555 */
    g_state.heartbeat_state = !g_state.heartbeat_state;
    hal_gpio_set_output(PIN_HEARTBEAT_OUT, g_state.heartbeat_state);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              SERIAL DIAGNOSTICS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void print_serial_log(void)
{
    uint32_t uptime_s = (time_get_ms() - g_state.boot_time_ms) / 1000;
    
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "  DIAGNOSTICS @ T+%lu seconds", (unsigned long)uptime_s);
    ESP_LOGI(TAG, "────────────────────────────────────────────────────────────");
    
    /* Voltage */
    ESP_LOGI(TAG, "  VOLTAGE: L1=%.1fV  L2=%.1fV  L3=%.1fV  [%s]",
             g_data.voltage[0].voltage_rms_v,
             g_data.voltage[1].voltage_rms_v,
             g_data.voltage[2].voltage_rms_v,
             g_data.voltage_ok ? "OK" : "ERR");
    
    /* Current */
    ESP_LOGI(TAG, "  CURRENT: L1=%.2fA  L2=%.2fA  L3=%.2fA  [%s]",
             g_data.current[0].current_a,
             g_data.current[1].current_a,
             g_data.current[2].current_a,
             g_data.current_ok ? "OK" : "ERR");
    
    /* Temperature */
    ESP_LOGI(TAG, "  TEMP:    SHT45=%.1fC  LM35=%.1fC  RH=%.1f%%",
             g_data.sht45.temperature_c,
             g_data.lm35_temp_c,
             g_data.sht45.humidity_percent);
    
    /* Gas */
    ESP_LOGI(TAG, "  GAS:     MQ2=%ld  MQ4=%ld  MQ9=%ld mV",
             (long)g_data.mq2_raw, (long)g_data.mq4_raw, (long)g_data.mq9_raw);
    
    /* RPM + Audio */
    ESP_LOGI(TAG, "  RPM:     %.0f  [%s]   Audio: %ld mV (peak: %ld)",
             g_data.rpm.rpm,
             g_data.rpm.is_rotating ? "ROTATING" : "STOPPED",
             (long)g_data.audio_raw, (long)g_data.audio_peak);
    
    /* System */
    ESP_LOGI(TAG, "  SYSTEM:  Heap=%luKB  Loops=%lu  Screen=%d",
             (unsigned long)(esp_get_free_heap_size() / 1024),
             (unsigned long)g_state.loop_count,
             g_state.current_screen);
    
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════════");
}