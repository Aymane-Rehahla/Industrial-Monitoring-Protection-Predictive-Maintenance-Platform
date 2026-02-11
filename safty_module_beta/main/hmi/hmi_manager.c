/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  hmi_manager.c - HMI Manager Implementation                                  ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#include "hmi_manager.h"
#include "screens.h"
#include "drv_lcd2004.h"
#include "drv_buttons.h"
#include "drv_leds.h"
#include "drv_buzzer.h"
#include "time_utils.h"

#include "esp_log.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *TAG = "HMI";

/* Screen registry */
static const screen_def_t SCREENS[] = {
    {SCREEN_HOME,        "HOME",     screen_home_render,     screen_home_input,     NULL, NULL},
    {SCREEN_VOLTAGE,     "VOLTAGE",  screen_voltage_render,  screen_voltage_input,  NULL, NULL},
    {SCREEN_CURRENT,     "CURRENT",  screen_current_render,  screen_current_input,  NULL, NULL},
    {SCREEN_TEMPERATURE, "TEMP",     screen_temp_render,     screen_temp_input,     NULL, NULL},
    {SCREEN_GAS,         "GAS",      screen_gas_render,      screen_gas_input,      NULL, NULL},
    {SCREEN_SETTINGS,    "SETTINGS", screen_settings_render, screen_settings_input, 
                                     screen_settings_enter,  screen_settings_exit},
    {SCREEN_FAULT_LOG,   "FAULTS",   screen_fault_render,    screen_fault_input,    NULL, NULL},
    {SCREEN_SYSTEM_INFO, "SYSTEM",   screen_system_render,   screen_system_input,   NULL, NULL},
};

#define SCREEN_COUNT  (sizeof(SCREENS) / sizeof(SCREENS[0]))

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool s_initialized = false;
static screen_id_t s_current_screen = SCREEN_HOME;
static bool s_needs_redraw = true;
static uint32_t s_last_update_ms = 0;

/* Alarm state */
static bool s_alarm_active = false;
static error_code_t s_alarm_code = ERR_OK;
static float s_alarm_value = 0.0f;
static bool s_alarm_blink_state = false;
static uint32_t s_alarm_blink_time = 0;

/* Data pointers (owned by application) */
static const sensor_set_t *s_sensors = NULL;
static const system_status_t *s_status = NULL;
static protection_config_t *s_config = NULL;

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              PRIVATE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const screen_def_t* get_screen_def(screen_id_t id)
{
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (SCREENS[i].id == id) {
            return &SCREENS[i];
        }
    }
    return &SCREENS[0];  /* Default to home */
}

/* ─────────────────────────────────────────────────────────────────────────────── */

static void handle_buttons(void)
{
    buttons_t btns;
    if (drv_buttons_get_state(&btns) != ERR_OK) {
        return;
    }
    
    /* Check for alarm acknowledgment (OK clears alarm) */
    if (s_alarm_active && btns.ok.just_pressed) {
        hmi_manager_clear_alarm();
        drv_buzzer_stop();
        return;
    }
    
    /* Get current screen definition */
    const screen_def_t *current = get_screen_def(s_current_screen);
    
    /* Check each button */
    button_id_t pressed = BTN_COUNT;
    
    if (btns.up.just_pressed)    pressed = BTN_UP;
    if (btns.down.just_pressed)  pressed = BTN_DOWN;
    if (btns.left.just_pressed)  pressed = BTN_LEFT;
    if (btns.right.just_pressed) pressed = BTN_RIGHT;
    if (btns.ok.just_pressed)    pressed = BTN_OK;
    
    if (pressed != BTN_COUNT && current->handle_input != NULL) {
        /* Button feedback */
        drv_buzzer_play(BEEP_CLICK);
        
        /* Let screen handle input */
        screen_id_t next = current->handle_input(pressed);
        
        if (next != s_current_screen) {
            hmi_manager_set_screen(next);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────────── */

static void render_alarm_overlay(void)
{
    /* Blink the alarm display */
    if (time_elapsed_ms(s_alarm_blink_time) > HMI_ALARM_BLINK_MS) {
        s_alarm_blink_state = !s_alarm_blink_state;
        s_alarm_blink_time = time_get_ms();
    }
    
    if (s_alarm_blink_state) {
        drv_lcd_print_at(0, 0, "!!! ALARM !!!");
        
        /* Show fault type */
        const char *fault_name = "UNKNOWN";
        switch (s_alarm_code) {
            case ERR_OVERCURRENT:  fault_name = "OVERCURRENT"; break;
            case ERR_OVERVOLTAGE:  fault_name = "OVERVOLTAGE"; break;
            case ERR_UNDERVOLTAGE: fault_name = "UNDERVOLTAGE"; break;
            case ERR_OVERTEMP:     fault_name = "OVERTEMP"; break;
            case ERR_GAS_DETECTED: fault_name = "GAS DETECTED"; break;
            default: break;
        }
        
        drv_lcd_printf_at(0, 1, "%-18s", fault_name);
        drv_lcd_printf_at(0, 2, "Value: %.1f", s_alarm_value);
        drv_lcd_print_at(0, 3, "Press OK to ack");
    } else {
        /* Show underlying screen during blink-off */
        const screen_def_t *current = get_screen_def(s_current_screen);
        if (current->render != NULL) {
            current->render(false);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

error_code_t hmi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI...");
    
    /* Initialize drivers */
    error_code_t err;
    
    err = drv_lcd_init();
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "LCD init failed (continuing without display)");
    }
    
    err = drv_buttons_init();
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "Buttons init failed");
    }
    
    err = drv_leds_init();
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "LEDs init failed");
    }
    
    err = drv_buzzer_init();
    if (err != ERR_OK) {
        ESP_LOGW(TAG, "Buzzer init failed");
    }
    
    /* Show startup screen */
    if (drv_lcd_is_online()) {
        drv_lcd_clear();
        drv_lcd_print_at(0, 0, "====================");
        drv_lcd_print_at(0, 1, "   SAFETY MODULE    ");
        drv_lcd_print_at(0, 2, "   Initializing...  ");
        drv_lcd_print_at(0, 3, "====================");
    }
    
    /* Startup beep */
    drv_buzzer_play(BEEP_STARTUP);
    
    s_initialized = true;
    s_current_screen = SCREEN_HOME;
    s_needs_redraw = true;
    
    ESP_LOGI(TAG, "HMI initialized");
    return ERR_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void hmi_manager_update(void)
{
    if (!s_initialized) {
        return;
    }
    
    /* Update button states */
    drv_buttons_update();
    
    /* Update LED patterns */
    drv_leds_update();
    
    /* Update buzzer patterns */
    drv_buzzer_update();
    
    /* Handle button input */
    handle_buttons();
    
    /* Rate-limit display updates */
    if (time_elapsed_ms(s_last_update_ms) < HMI_UPDATE_INTERVAL_MS) {
        return;
    }
    s_last_update_ms = time_get_ms();
    
    /* Render */
    if (s_alarm_active) {
        render_alarm_overlay();
    } else {
        const screen_def_t *current = get_screen_def(s_current_screen);
        if (current->render != NULL) {
            current->render(s_needs_redraw);
            s_needs_redraw = false;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void hmi_manager_set_screen(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT) {
        screen = SCREEN_HOME;
    }
    
    if (screen == s_current_screen) {
        return;
    }
    
    /* Call exit on old screen */
    const screen_def_t *old = get_screen_def(s_current_screen);
    if (old->on_exit != NULL) {
        old->on_exit();
    }
    
    /* Change screen */
    s_current_screen = screen;
    s_needs_redraw = true;
    
    /* Call enter on new screen */
    const screen_def_t *new_screen = get_screen_def(s_current_screen);
    if (new_screen->on_enter != NULL) {
        new_screen->on_enter();
    }
    
    ESP_LOGI(TAG, "Screen: %s", new_screen->name);
}

/* ─────────────────────────────────────────────────────────────────────────────── */

screen_id_t hmi_manager_get_screen(void)
{
    return s_current_screen;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void hmi_manager_request_redraw(void)
{
    s_needs_redraw = true;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void hmi_manager_show_alarm(error_code_t code, float value)
{
    s_alarm_active = true;
    s_alarm_code = code;
    s_alarm_value = value;
    s_alarm_blink_state = true;
    s_alarm_blink_time = time_get_ms();
    
    /* Start alarm buzzer */
    drv_buzzer_play(BEEP_ALARM);
    
    /* Set LEDs */
    drv_leds_set_mode(LED_GREEN, LED_MODE_OFF);
    drv_leds_set_mode(LED_RED, LED_MODE_BLINK_VERY_FAST);
    
    ESP_LOGW(TAG, "ALARM: code=0x%02X value=%.1f", code, value);
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void hmi_manager_clear_alarm(void)
{
    s_alarm_active = false;
    s_needs_redraw = true;
    
    ESP_LOGI(TAG, "Alarm cleared");
}

/* ─────────────────────────────────────────────────────────────────────────────── */

bool hmi_manager_has_alarm(void)
{
    return s_alarm_active;
}

/* ─────────────────────────────────────────────────────────────────────────────── */

void hmi_manager_set_sensor_data(const sensor_set_t *sensors)
{
    s_sensors = sensors;
}

void hmi_manager_set_system_status(const system_status_t *status)
{
    s_status = status;
}

void hmi_manager_set_config(protection_config_t *config)
{
    s_config = config;
}

/* Accessor for screens */
const sensor_set_t* hmi_get_sensors(void) { return s_sensors; }
const system_status_t* hmi_get_status(void) { return s_status; }
protection_config_t* hmi_get_config(void) { return s_config; }