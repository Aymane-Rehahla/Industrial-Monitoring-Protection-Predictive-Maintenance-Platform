// ═══ FILE: main/drivers/actuators/drv_buzzer.c ═══
/**
 * @file    drv_buzzer.c
 * @brief   Non-blocking passive buzzer driver using LEDC PWM
 * @version 1.0
 * @date    2025-01-01
 * @safety  LOW — informational only, not safety-critical
 *
 * WHY non-blocking: The HMI task calls drv_buzzer_tick() every 20ms.
 * A state machine advances through tone pattern steps without ever
 * calling vTaskDelay or blocking.  This keeps the HMI responsive
 * even during multi-note patterns like the startup chime.
 *
 * Changelog:
 *   1.0 — Initial implementation
 */

#include "drv_buzzer.h"
#include "system_types.h"
#include "app_config.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "drv_buzzer";

/* ═══════════════════════════════════════════════════════════════════ */
/*  Tone step definition                                              */
/* ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    uint16_t frequency_hz;  /**< 0 = silence (pause between notes)  */
    uint16_t duration_ms;   /**< How long this step lasts            */
} tone_step_t;

typedef struct {
    const tone_step_t *steps;
    uint8_t            step_count;
    bool               is_repeating;
} tone_pattern_t;

/* ═══════════════════════════════════════════════════════════════════ */
/*  Tone pattern definitions                                          */
/* ═══════════════════════════════════════════════════════════════════ */

static const tone_step_t s_steps_click[] = {
    { TONE_CLICK_HZ, TONE_CLICK_MS }
};

static const tone_step_t s_steps_nav[] = {
    { TONE_NAV_HZ, TONE_NAV_MS }
};

static const tone_step_t s_steps_confirm[] = {
    { TONE_STARTUP_HZ, TONE_CONFIRM_MS },
    { TONE_CONFIRM_HZ, TONE_CONFIRM_MS }
};

static const tone_step_t s_steps_error[] = {
    { TONE_ERROR_HZ, TONE_ERROR_MS }
};

static const tone_step_t s_steps_warning[] = {
    { TONE_WARNING_HZ, 200 },
    { 0,               300 }
};

static const tone_step_t s_steps_alarm[] = {
    { TONE_ALARM_HZ, 150 },
    { 0,             100 }
};

static const tone_step_t s_steps_startup[] = {
    { 1000, 100 },
    { 0,     50 },
    { 1500, 100 },
    { 0,     50 },
    { 2000, 150 }
};

/* WHY array indexed by buzzer_action_t: O(1) lookup, no switch. */
static const tone_pattern_t s_patterns[BUZZER_ACTION_COUNT] = {
    [BUZZER_SILENT]  = { NULL,             0, false },
    [BUZZER_CLICK]   = { s_steps_click,    1, false },
    [BUZZER_NAV]     = { s_steps_nav,      1, false },
    [BUZZER_CONFIRM] = { s_steps_confirm,  2, false },
    [BUZZER_WARNING] = { s_steps_warning,  2, true  },
    [BUZZER_ERROR]   = { s_steps_error,    1, false },
    [BUZZER_ALARM]   = { s_steps_alarm,    2, true  },
    [BUZZER_STARTUP] = { s_steps_startup,  5, false },
};

/* ═══════════════════════════════════════════════════════════════════ */
/*  State variables                                                   */
/*  WHY static: Persist across tick calls; only one buzzer exists.    */
/* ═══════════════════════════════════════════════════════════════════ */
static bool            s_initialized    = false;
static bool            s_is_active      = false;
static buzzer_action_t s_current_action = BUZZER_SILENT;
static uint8_t         s_step_index     = 0;
static uint32_t        s_step_start_ms  = 0;

/* ═══════════════════════════════════════════════════════════════════ */
/*  Helper: get current tick in milliseconds                          */
/* ═══════════════════════════════════════════════════════════════════ */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  Helper: set buzzer tone or silence                                */
/* ═══════════════════════════════════════════════════════════════════ */
/**
 * @brief  Set LEDC output to specified frequency, or silence if 0.
 * @pre    LEDC timer and channel initialized.
 * @post   Buzzer produces tone or is silent.
 * @wcet   < 50 us
 * @thread Not thread-safe — called only from HMI task.
 */
static void buzzer_set_tone(uint16_t freq_hz)
{
    if (freq_hz == 0) {
        /* Silence: set duty to 0 */
        ledc_set_duty(BUZZER_LEDC_SPEED_MODE, BUZZER_LEDC_CHANNEL, 0);
        ledc_update_duty(BUZZER_LEDC_SPEED_MODE, BUZZER_LEDC_CHANNEL);
        return;
    }

    /* Set frequency first, then duty for audible output */
    ledc_set_freq(BUZZER_LEDC_SPEED_MODE, BUZZER_LEDC_TIMER, freq_hz);
    ledc_set_duty(BUZZER_LEDC_SPEED_MODE, BUZZER_LEDC_CHANNEL,
                  BUZZER_DEFAULT_DUTY);
    ledc_update_duty(BUZZER_LEDC_SPEED_MODE, BUZZER_LEDC_CHANNEL);
}

/* ═══════════════════════════════════════════════════════════════════ */
/*  PUBLIC API                                                        */
/* ═══════════════════════════════════════════════════════════════════ */

/**
 * @brief  Initialize LEDC timer and channel for passive buzzer.
 * @pre    hal_gpio_init() called (JTAG pins reclaimed).
 * @post   LEDC configured, buzzer silent.
 * @return ERR_OK on success.
 * @wcet   < 1 ms
 * @thread Safe (called once during init).
 */
error_code_t drv_buzzer_init(void)
{
    if (s_initialized) {
        return ERR_ALREADY_INITIALIZED;
    }

    const ledc_timer_config_t timer_cfg = {
        .speed_mode      = BUZZER_LEDC_SPEED_MODE,
        .timer_num       = BUZZER_LEDC_TIMER,
        .duty_resolution = BUZZER_LEDC_RESOLUTION,
        .freq_hz         = TONE_CLICK_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s",
                 esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    const ledc_channel_config_t ch_cfg = {
        .speed_mode = BUZZER_LEDC_SPEED_MODE,
        .channel    = BUZZER_LEDC_CHANNEL,
        .timer_sel  = BUZZER_LEDC_TIMER,
        .gpio_num   = PIN_BUZZER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };

    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s",
                 esp_err_to_name(err));
        return ERR_HW_INIT_FAILED;
    }

    s_is_active      = false;
    s_current_action = BUZZER_SILENT;
    s_step_index     = 0;
    s_initialized    = true;

    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", PIN_BUZZER);
    return ERR_OK;
}

/**
 * @brief  Start playing a tone pattern.
 * @pre    Buzzer initialized.
 * @post   Pattern begins on next tick.  Overrides any current pattern.
 * @param  action  Which pattern to play (BUZZER_SILENT stops).
 * @return ERR_OK on success.
 * @wcet   < 50 us
 * @thread Not thread-safe — called only from HMI task.
 */
error_code_t drv_buzzer_play(buzzer_action_t action)
{
    if (!s_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if ((uint32_t)action >= BUZZER_ACTION_COUNT) {
        return ERR_INVALID_ARG;
    }

    if (action == BUZZER_SILENT) {
        return drv_buzzer_stop();
    }

    const tone_pattern_t *pat = &s_patterns[action];
    if (pat->steps == NULL || pat->step_count == 0) {
        return drv_buzzer_stop();
    }

    s_current_action = action;
    s_step_index     = 0;
    s_step_start_ms  = get_tick_ms();
    s_is_active      = true;

    /* Start first step immediately */
    buzzer_set_tone(pat->steps[0].frequency_hz);
    return ERR_OK;
}

/**
 * @brief  Immediately silence the buzzer.
 * @pre    Buzzer initialized.
 * @post   Buzzer silent, pattern state reset.
 * @return ERR_OK.
 * @wcet   < 50 us
 * @thread Not thread-safe — called only from HMI task.
 */
error_code_t drv_buzzer_stop(void)
{
    if (!s_initialized) {
        return ERR_NOT_INITIALIZED;
    }

    buzzer_set_tone(0);
    s_is_active      = false;
    s_current_action = BUZZER_SILENT;
    s_step_index     = 0;
    return ERR_OK;
}

/**
 * @brief  Advance pattern state machine.  Call every 10-20ms.
 * @pre    Buzzer initialized.
 * @post   Tone output updated if step duration expired.
 * @return ERR_OK.
 * @wcet   < 100 us
 * @thread Not thread-safe — called only from HMI task.
 *
 * WHY tick-based: Avoids blocking delays.  The HMI task calls this
 * at its scan rate.  Each call checks elapsed time and advances
 * to the next step if the current one has expired.
 */
error_code_t drv_buzzer_tick(void)
{
    if (!s_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    if (!s_is_active) {
        return ERR_OK;
    }

    const tone_pattern_t *pat = &s_patterns[s_current_action];
    if (pat->steps == NULL || pat->step_count == 0) {
        drv_buzzer_stop();
        return ERR_OK;
    }

    /* Bounds-check step index — defensive */
    if (s_step_index >= pat->step_count) {
        drv_buzzer_stop();
        return ERR_OK;
    }

    uint32_t elapsed_ms = get_tick_ms() - s_step_start_ms;
    uint16_t step_dur   = pat->steps[s_step_index].duration_ms;

    /* Has current step expired? */
    if (elapsed_ms < step_dur) {
        return ERR_OK;
    }

    /* Move to next step */
    s_step_index++;

    if (s_step_index >= pat->step_count) {
        /* Pattern complete */
        if (pat->is_repeating) {
            /* Loop back to start */
            s_step_index    = 0;
            s_step_start_ms = get_tick_ms();
            buzzer_set_tone(pat->steps[0].frequency_hz);
        } else {
            /* One-shot complete */
            drv_buzzer_stop();
        }
        return ERR_OK;
    }

    /* Start the new step */
    s_step_start_ms = get_tick_ms();
    buzzer_set_tone(pat->steps[s_step_index].frequency_hz);
    return ERR_OK;
}

/**
 * @brief  Check if a pattern is currently playing.
 * @return true if active, false if silent.
 * @wcet   < 1 us
 * @thread Safe (reads single bool).
 */
bool drv_buzzer_is_playing(void)
{
    return s_is_active;
}