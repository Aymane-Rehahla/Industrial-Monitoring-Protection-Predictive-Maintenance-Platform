// ═══ FILE: main/hmi/led_status.c ═══
/**
 * @file    led_status.c
 * @brief   LED status mapping — real implementation.
 *          Translates system_state_t to LED modes and colours.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — informational only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/led_status.h"
#include "system_types.h"
#include "app_config.h"

#include "core/system_status.h"
#include "drivers/actuators/drv_leds.h"

#include "esp_log.h"

static const char *TAG = "led_status";

/* ── State-to-LED mapping table ──────────────────────────────────────── */
typedef struct {
    led_mode_t red_mode;
    led_mode_t rgb_mode;
    uint8_t    rgb_r;
    uint8_t    rgb_g;
    uint8_t    rgb_b;
} led_mapping_t;

/* WHY a table: Adding new states or changing LED behaviour requires
 * editing one row instead of hunting through switch/case logic. */
static const led_mapping_t STATE_LED_MAP[] = {
    /* SYS_STATE_BOOT     */ { LED_OFF,        LED_BLINK_SLOW, 0,   0,   255 },
    /* SYS_STATE_CONFIG   */ { LED_OFF,        LED_BLINK_SLOW, 0,   255, 255 },
    /* SYS_STATE_VALIDATE */ { LED_OFF,        LED_BLINK_FAST, 255, 255, 0   },
    /* SYS_STATE_READY    */ { LED_OFF,        LED_ON,         0,   255, 0   },
    /* SYS_STATE_RUN      */ { LED_OFF,        LED_BLINK_SLOW, 0,   255, 0   },
    /* SYS_STATE_FAULT    */ { LED_BLINK_FAST, LED_BLINK_FAST, 255, 0,   0   },
};

#define STATE_MAP_COUNT  ARRAY_SIZE(STATE_LED_MAP)

/* Justification: Track previous state to detect transitions.
 * Static file scope. */
static system_state_t s_last_state    = SYS_STATE_BOOT;
static bool           s_initialized   = false;
static bool           s_override[LED_COUNT];
static led_mode_t     s_override_mode[LED_COUNT];

/* ═══════════════════════════════════════════════════════════════════════
 *  led_status_init
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t led_status_init(void)
{
    s_last_state = SYS_STATE_BOOT;

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        s_override[i] = false;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LED status mapping initialised");
    return ERR_OK;
}

/**
 * @brief  Apply LED mapping for a given system state.
 */
static void apply_state_leds(system_state_t state)
{
    if ((uint32_t)state >= STATE_MAP_COUNT) { return; }

    const led_mapping_t *map = &STATE_LED_MAP[state];

    if (!s_override[LED_RED]) {
        drv_leds_set_mode(LED_RED, map->red_mode);
    }

    if (!s_override[LED_RGB]) {
        drv_leds_set_rgb_color(map->rgb_r, map->rgb_g, map->rgb_b);
        drv_leds_set_mode(LED_RGB, map->rgb_mode);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  led_status_tick
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t led_status_tick(void)
{
    if (!s_initialized) { return ERR_NOT_INITIALIZED; }

    system_state_t current = system_status_get_state();

    /* On state change: apply new LED pattern, clear overrides. */
    if (current != s_last_state) {
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            s_override[i] = false;
        }
        apply_state_leds(current);
        s_last_state = current;
    }

    /* Drive the blink state machines. */
    drv_leds_tick();

    return ERR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  led_status_force
 * ═══════════════════════════════════════════════════════════════════════ */
error_code_t led_status_force(led_id_t led, led_mode_t mode)
{
    if (!s_initialized)  { return ERR_NOT_INITIALIZED; }
    if (led >= LED_COUNT) { return ERR_INVALID_ARG; }

    s_override[led]      = true;
    s_override_mode[led] = mode;
    drv_leds_set_mode(led, mode);

    return ERR_OK;
}