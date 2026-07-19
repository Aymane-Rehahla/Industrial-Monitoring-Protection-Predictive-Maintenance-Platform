// ═══ FILE: main/drivers/actuators/drv_leds.c ═══
/**
 * @file    drv_leds.c
 * @brief   LED driver: red GPIO LED + WS2812 RGB NeoPixel with blink patterns.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — informational only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "drivers/actuators/drv_leds.h"
#include "system_types.h"
#include "app_config.h"
#include "hal/hal_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/* ── Conditional RGB LED support ─────────────────────────────────────── */
/* WHY conditional: The led_strip component may not be in the build.
 * Without it, red LED still works; RGB calls return gracefully. */
#if __has_include("led_strip.h")
    #include "led_strip.h"
    #define RGB_LED_AVAILABLE  1
#else
    #define RGB_LED_AVAILABLE  0
#endif

static const char *TAG = "drv_leds";

/* ── SOS blink pattern timing table ──────────────────────────────────── */
/* Even indices = ON duration (ms), odd indices = OFF duration (ms).
 * S = ···  O = ———  S = ···  then word gap.
 * Total cycle ≈ 5.4 seconds. */
static const uint16_t SOS_PATTERN[] = {
    /* S: dot dot dot */
    LED_SOS_DOT_MS,  LED_SOS_GAP_MS,
    LED_SOS_DOT_MS,  LED_SOS_GAP_MS,
    LED_SOS_DOT_MS,  LED_SOS_GAP_MS,
    /* O: dash dash dash */
    LED_SOS_DASH_MS, LED_SOS_GAP_MS,
    LED_SOS_DASH_MS, LED_SOS_GAP_MS,
    LED_SOS_DASH_MS, LED_SOS_GAP_MS,
    /* S: dot dot dot */
    LED_SOS_DOT_MS,  LED_SOS_GAP_MS,
    LED_SOS_DOT_MS,  LED_SOS_GAP_MS,
    LED_SOS_DOT_MS,  LED_SOS_GAP_MS,
    /* Word gap (OFF only, no following ON) */
    LED_SOS_WORD_GAP_MS
};
#define SOS_PATTERN_LEN  ARRAY_SIZE(SOS_PATTERN)

/* ── Per-LED state ───────────────────────────────────────────────────── */
typedef struct {
    led_mode_t mode;             /* Current blink mode                    */
    bool       current_on;       /* Is LED physically on right now?       */
    uint32_t   last_toggle_ms;   /* When last toggled (for blink timing)  */
    uint8_t    sos_step;         /* Current step in SOS pattern           */
} led_state_t;

/* Justification: Per-LED blink state must persist across tick() calls.
 * Only HMI task accesses these. File scope. */
static led_state_t s_leds[LED_COUNT];

/* Justification: RGB colour state — separate from on/off mode.
 * Set once, applied repeatedly during "on" phases. */
static uint8_t s_rgb_red   = 0;
static uint8_t s_rgb_green  = 0;
static uint8_t s_rgb_blue   = 0;

/* Justification: Module init flag. */
static bool s_initialized = false;

#if RGB_LED_AVAILABLE
/* Justification: led_strip handle — created once at init, used in tick(). */
static led_strip_handle_t s_rgb_handle = NULL;
#endif

/* ── Forward declarations ────────────────────────────────────────────── */
static uint32_t get_tick_ms(void);
static void     red_led_set(bool on);
static void     rgb_led_set(bool on);
static void     tick_blink(led_state_t *led, uint32_t interval_ms,
                           uint32_t now_ms);
static void     tick_sos(led_state_t *led, uint32_t now_ms);

/* ═══════════════════════════════════════════════════════════════════════
 *  get_tick_ms
 * ═══════════════════════════════════════════════════════════════════════ */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  red_led_set — Direct GPIO control for the red LED.
 * ═══════════════════════════════════════════════════════════════════════ */
static void red_led_set(bool on)
{
    hal_gpio_write(PIN_LED_RED, on);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  rgb_led_set — WS2812 control with brightness scaling.
 *
 *  WHY brightness scaling: The onboard NeoPixel is blindingly bright
 *  at full power.  RGB_BRIGHTNESS (20/255 ≈ 8%) keeps it visible
 *  without washing out in an industrial panel.
 * ═══════════════════════════════════════════════════════════════════════ */
static void rgb_led_set(bool on)
{
#if RGB_LED_AVAILABLE
    if (s_rgb_handle == NULL) { return; }

    if (on) {
        uint8_t r = (uint8_t)(((uint16_t)s_rgb_red   * RGB_BRIGHTNESS) / 255U);
        uint8_t g = (uint8_t)(((uint16_t)s_rgb_green  * RGB_BRIGHTNESS) / 255U);
        uint8_t b = (uint8_t)(((uint16_t)s_rgb_blue   * RGB_BRIGHTNESS) / 255U);
        led_strip_set_pixel(s_rgb_handle, 0, r, g, b);
    } else {
        led_strip_clear(s_rgb_handle);
    }
    led_strip_refresh(s_rgb_handle);
#else
    UNUSED(on);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════
 *  tick_blink — Toggle LED at a fixed interval.
 * ═══════════════════════════════════════════════════════════════════════ */
static void tick_blink(led_state_t *led, uint32_t interval_ms,
                       uint32_t now_ms)
{
    if ((now_ms - led->last_toggle_ms) >= interval_ms) {
        led->current_on     = !led->current_on;
        led->last_toggle_ms = now_ms;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  tick_sos — Step through the SOS timing table.
 *
 *  WHY a step table: SOS has irregular timing (dots vs dashes vs gaps).
 *  A simple toggle interval can't represent it.  The step table
 *  encodes the exact on/off durations in sequence.
 *
 *  Even steps (0, 2, 4…) = ON duration.
 *  Odd steps  (1, 3, 5…) = OFF duration.
 *  Last step (word gap) is always OFF.
 * ═══════════════════════════════════════════════════════════════════════ */
static void tick_sos(led_state_t *led, uint32_t now_ms)
{
    /* Bounds-check SOS step index. */
    if (led->sos_step >= SOS_PATTERN_LEN) {
        led->sos_step = 0;
    }

    uint16_t step_duration = SOS_PATTERN[led->sos_step];
    uint32_t elapsed       = now_ms - led->last_toggle_ms;

    if (elapsed < step_duration) { return; }

    /* Advance to next step. */
    led->sos_step++;
    if (led->sos_step >= SOS_PATTERN_LEN) {
        led->sos_step = 0;
    }

    /* Even index = ON, odd index = OFF. */
    led->current_on     = ((led->sos_step % 2U) == 0U);
    led->last_toggle_ms = now_ms;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_leds_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_leds_init(void)
{
    /* Red LED: simple GPIO output. */
    error_code_t rc = hal_gpio_config_output(PIN_LED_RED, false);
    if (rc != ERR_OK) {
        ESP_LOGE(TAG, "Red LED GPIO %d config failed", PIN_LED_RED);
        /* Continue — try RGB anyway. */
    }

    /* RGB LED: WS2812 via led_strip component. */
#if RGB_LED_AVAILABLE
    led_strip_config_t strip_cfg = {
        .strip_gpio_num   = PIN_RGB_LED,
        .max_leds         = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model        = LED_MODEL_WS2812,
        .flags.invert_out = false
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = 10000000,   /* 10 MHz — standard for WS2812     */
        .flags.with_dma = false
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg,
                                             &s_rgb_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RGB LED init failed (%s) — continuing without RGB",
                 esp_err_to_name(err));
        s_rgb_handle = NULL;
    } else {
        led_strip_clear(s_rgb_handle);
        led_strip_refresh(s_rgb_handle);
        ESP_LOGI(TAG, "RGB LED initialised on GPIO %d", PIN_RGB_LED);
    }
#else
    ESP_LOGW(TAG, "led_strip component not available — RGB disabled");
#endif

    /* Initialise per-LED state. */
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        s_leds[i].mode           = LED_OFF;
        s_leds[i].current_on     = false;
        s_leds[i].last_toggle_ms = 0;
        s_leds[i].sos_step       = 0;
    }

    s_rgb_red   = 0;
    s_rgb_green = 0;
    s_rgb_blue  = 0;

    s_initialized = true;
    ESP_LOGI(TAG, "LED driver initialised (red=GPIO%d, rgb=GPIO%d)",
             PIN_LED_RED, PIN_RGB_LED);

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_leds_set_mode
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_leds_set_mode(led_id_t led, led_mode_t mode)
{
    if (!s_initialized)  { return ERR_NOT_INITIALIZED; }
    if (led >= LED_COUNT) { return ERR_INVALID_ARG; }

    led_state_t *state = &s_leds[led];

    /* Reset blink state when mode changes. */
    if (mode != state->mode) {
        state->mode           = mode;
        state->last_toggle_ms = get_tick_ms();
        state->sos_step       = 0;
        state->current_on     = (mode == LED_ON || mode == LED_BLINK_SLOW ||
                                  mode == LED_BLINK_FAST || mode == LED_BLINK_SOS);
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_leds_set_rgb_color
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_leds_set_rgb_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    s_rgb_red   = red;
    s_rgb_green = green;
    s_rgb_blue  = blue;

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  apply_led_state — Write the current_on state to hardware.
 *
 *  WHY separated: Avoids duplicating the LED_RED vs LED_RGB dispatch
 *  logic in multiple places within tick().
 * ═══════════════════════════════════════════════════════════════════════ */
static void apply_led_state(led_id_t led, bool on)
{
    if (led == LED_RED) {
        red_led_set(on);
    } else if (led == LED_RGB) {
        rgb_led_set(on);
    }
    /* No other LED IDs exist; bounds checked by caller. */
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_leds_tick
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t drv_leds_tick(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    uint32_t now_ms = get_tick_ms();

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        led_state_t *led = &s_leds[i];

        switch (led->mode) {
            case LED_OFF:
                if (led->current_on) {
                    led->current_on = false;
                    apply_led_state((led_id_t)i, false);
                }
                break;

            case LED_ON:
                if (!led->current_on) {
                    led->current_on = true;
                    apply_led_state((led_id_t)i, true);
                }
                break;

            case LED_BLINK_SLOW:
                tick_blink(led, LED_BLINK_SLOW_MS, now_ms);
                apply_led_state((led_id_t)i, led->current_on);
                break;

            case LED_BLINK_FAST:
                tick_blink(led, LED_BLINK_FAST_MS, now_ms);
                apply_led_state((led_id_t)i, led->current_on);
                break;

            case LED_BLINK_SOS:
                tick_sos(led, now_ms);
                apply_led_state((led_id_t)i, led->current_on);
                break;

            default:
                /* Unknown mode — default to off for safety. */
                led->current_on = false;
                apply_led_state((led_id_t)i, false);
                break;
        }
    }

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  drv_leds_is_initialized
 * ═══════════════════════════════════════════════════════════════════════ */
bool drv_leds_is_initialized(void)
{
    return s_initialized;
}