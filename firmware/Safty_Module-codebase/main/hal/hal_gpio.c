// ═══ FILE: main/hal/hal_gpio.c ═══
/**
 * @file    hal_gpio.c
 * @brief   GPIO HAL implementation — relay safety, pin validation, ISR setup.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  CRITICAL — GPIO 15 controls the safety relay.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hal/hal_gpio.h"
#include "app_config.h"

#include "driver/gpio.h"
#include "soc/gpio_struct.h"   /* Direct register access for force_relay_safe */
#include "esp_log.h"

#include <string.h>

static const char *TAG = "hal_gpio";

/* ── Pin direction tracking ──────────────────────────────────────────── */
typedef enum {
    PIN_DIR_UNUSED = 0,
    PIN_DIR_INPUT  = 1,
    PIN_DIR_OUTPUT = 2,
    PIN_DIR_ISR    = 3   /* Input with interrupt */
} pin_direction_t;

/**
 * Maximum GPIO number on ESP32-S3.
 * Array is sized [0 .. GPIO_NUM_MAX] inclusive.
 */
#define GPIO_NUM_MAX  48

/* Justification: Tracks per-pin configuration state.  Must persist across
 * calls to detect misconfiguration (e.g., writing to an input pin).
 * Static file scope — not visible outside this module. */
static pin_direction_t s_pin_dir[GPIO_NUM_MAX + 1];

/* Justification: Module initialisation flag.  Prevents use before init. */
static bool s_gpio_initialized = false;

/* Justification: ISR service install flag.  Install once, never twice. */
static bool s_isr_service_installed = false;

/**
 * Complete list of GPIO pins used by this project.
 * hal_gpio_is_valid_pin() checks against this list.
 * Any pin NOT in this list is rejected — prevents accidental writes
 * to strapping pins, flash pins, or unconnected GPIOs.
 */
static const uint8_t VALID_PINS[] = {
     1,  2,  3,  4,  5,  6,  7,       /* ADC + mux             */
     8,  9,                            /* I2C Bus 0             */
    10, 11, 12, 13, 14, 15, 16,       /* Buttons, buzzer, relay */
    17, 18,                            /* I2C Bus 1             */
    21,                                /* Button DOWN           */
    38, 39, 40, 41, 42,               /* Spare, LED, Hall, OK  */
    43, 44,                            /* UART peer             */
    47, 48                             /* ATtiny warn, RGB LED  */
};
#define VALID_PIN_COUNT  ARRAY_SIZE(VALID_PINS)

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_is_valid_pin
 * ═══════════════════════════════════════════════════════════════════════ */
bool hal_gpio_is_valid_pin(uint8_t gpio_num)
{
    /* Bounded loop — never exceeds VALID_PIN_COUNT iterations. */
    for (size_t i = 0; i < VALID_PIN_COUNT; i++) {
        if (VALID_PINS[i] == gpio_num) {
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_force_relay_safe  —  THE "OH SHIT" FUNCTION
 *
 *  Direct register write.  No function calls.  No validation.
 *  GPIO 15 is in the low bank (0–31), so we use GPIO.out_w1tc.
 *  W1TC = "Write 1 To Clear" — writing a 1 to bit 15 forces output LOW
 *  without affecting any other pin.  Atomic single-cycle operation.
 * ═══════════════════════════════════════════════════════════════════════ */
void hal_gpio_force_relay_safe(void)
{
    GPIO.out_w1tc = (1U << PIN_RELAY_DRIVE);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_init
 *
 *  RULE 8.11: The VERY FIRST thing after reset is to open the relay.
 *  We do this with bare ESP-IDF calls before any logging or validation.
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_gpio_init(void)
{
    /* ── STEP 0: RELAY SAFE STATE — before ANYTHING else ───────────── */
    gpio_reset_pin(PIN_RELAY_DRIVE);
    gpio_set_direction(PIN_RELAY_DRIVE, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RELAY_DRIVE, 0);   /* LOW = relay off = SAFE */

    /* Also hit the direct register for belt-and-suspenders safety. */
    hal_gpio_force_relay_safe();

    /* ── STEP 1: Clear pin tracking array ──────────────────────────── */
    memset(s_pin_dir, PIN_DIR_UNUSED, sizeof(s_pin_dir));

    /* Record that relay pin is already configured as output. */
    s_pin_dir[PIN_RELAY_DRIVE] = PIN_DIR_OUTPUT;

    /* ── STEP 2: Reclaim JTAG pins for GPIO use ────────────────────── */
    /* On ESP32-S3, GPIOs 39–42 are shared with USB-JTAG.
     * gpio_reset_pin() disconnects them from the JTAG peripheral. */
    gpio_reset_pin(39);
    gpio_reset_pin(40);
    gpio_reset_pin(41);
    gpio_reset_pin(42);

    s_gpio_initialized = true;
    ESP_LOGI(TAG, "GPIO initialised — relay GPIO %d forced LOW (safe)",
             PIN_RELAY_DRIVE);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_config_output
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_gpio_config_output(uint8_t gpio_num, bool initial_level)
{
    if (!s_gpio_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if (!hal_gpio_is_valid_pin(gpio_num)) {
        ESP_LOGE(TAG, "config_output: GPIO %u not in valid pin list", gpio_num);
        return ERR_INVALID_ARG;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };

    if (gpio_config(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "config_output: gpio_config failed for GPIO %u", gpio_num);
        return ERR_HW_INIT_FAILED;
    }

    gpio_set_level(gpio_num, initial_level ? 1 : 0);
    s_pin_dir[gpio_num] = PIN_DIR_OUTPUT;

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_config_input
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_gpio_config_input(uint8_t gpio_num, bool enable_pullup)
{
    if (!s_gpio_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if (!hal_gpio_is_valid_pin(gpio_num)) {
        ESP_LOGE(TAG, "config_input: GPIO %u not in valid pin list", gpio_num);
        return ERR_INVALID_ARG;
    }

    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = enable_pullup ? GPIO_PULLUP_ENABLE
                                      : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };

    if (gpio_config(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "config_input: gpio_config failed for GPIO %u", gpio_num);
        return ERR_HW_INIT_FAILED;
    }

    s_pin_dir[gpio_num] = PIN_DIR_INPUT;

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_config_interrupt
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_gpio_config_interrupt(uint8_t gpio_num, int edge_type,
                                       void (*isr_handler)(void *),
                                       void *arg)
{
    if (!s_gpio_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if (!hal_gpio_is_valid_pin(gpio_num)) {
        ESP_LOGE(TAG, "config_isr: GPIO %u not in valid pin list", gpio_num);
        return ERR_INVALID_ARG;
    }
    if (isr_handler == NULL) {
        ESP_LOGE(TAG, "config_isr: NULL handler for GPIO %u", gpio_num);
        return ERR_NULL_POINTER;
    }

    /* Map edge_type to ESP-IDF interrupt type. */
    gpio_int_type_t intr;
    switch (edge_type) {
        case GPIO_EDGE_RISING:  intr = GPIO_INTR_POSEDGE; break;
        case GPIO_EDGE_FALLING: intr = GPIO_INTR_NEGEDGE; break;
        case GPIO_EDGE_ANY:     intr = GPIO_INTR_ANYEDGE; break;
        default:
            ESP_LOGE(TAG, "config_isr: invalid edge_type %d", edge_type);
            return ERR_INVALID_ARG;
    }

    /* Install ISR service once (idempotent). */
    if (!s_isr_service_installed) {
        esp_err_t err = gpio_install_isr_service(0);
        /* ESP_ERR_INVALID_STATE means already installed — that is OK. */
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "config_isr: isr_service install failed (%d)", err);
            return ERR_HW_INIT_FAILED;
        }
        s_isr_service_installed = true;
    }

    gpio_set_intr_type(gpio_num, intr);
    if (gpio_isr_handler_add(gpio_num, isr_handler, arg) != ESP_OK) {
        ESP_LOGE(TAG, "config_isr: handler_add failed for GPIO %u", gpio_num);
        return ERR_HW_INIT_FAILED;
    }

    s_pin_dir[gpio_num] = PIN_DIR_ISR;

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_write
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_gpio_write(uint8_t gpio_num, bool level)
{
    if (!s_gpio_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if (gpio_num > GPIO_NUM_MAX) {
        return ERR_INVALID_ARG;
    }
    if (s_pin_dir[gpio_num] != PIN_DIR_OUTPUT) {
        ESP_LOGE(TAG, "write: GPIO %u is not configured as output", gpio_num);
        return ERR_INVALID_ARG;
    }

    if (gpio_set_level(gpio_num, level ? 1 : 0) != ESP_OK) {
        ESP_LOGE(TAG, "write: gpio_set_level failed for GPIO %u", gpio_num);
        return ERR_HW_WRITE_FAILED;
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  hal_gpio_read
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t hal_gpio_read(uint8_t gpio_num, bool *level_out)
{
    if (!s_gpio_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if (level_out == NULL) {
        return ERR_NULL_POINTER;
    }
    if (gpio_num > GPIO_NUM_MAX) {
        return ERR_INVALID_ARG;
    }
    if (s_pin_dir[gpio_num] == PIN_DIR_UNUSED) {
        ESP_LOGW(TAG, "read: GPIO %u not configured — reading anyway", gpio_num);
        /* Allow the read but warn — some callers read during setup. */
    }

    *level_out = (gpio_get_level(gpio_num) != 0);

    return ERR_OK;
}