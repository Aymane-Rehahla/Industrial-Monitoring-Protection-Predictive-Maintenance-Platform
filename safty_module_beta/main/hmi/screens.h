/**
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║  screens.h - Screen Registry and Declarations                                ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 */

#ifndef SCREENS_H
#define SCREENS_H

#include "system_types.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              SCREEN FUNCTION TYPES
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Screen render function (called to draw/update screen)
 * @param full_redraw true if entire screen needs redraw, false for partial
 */
typedef void (*screen_render_fn)(bool full_redraw);

/**
 * @brief Screen input handler (called when button pressed)
 * @param button Which button was pressed
 * @return New screen ID to navigate to, or current to stay
 */
typedef screen_id_t (*screen_input_fn)(button_id_t button);

/**
 * @brief Screen entry function (called once when entering screen)
 */
typedef void (*screen_enter_fn)(void);

/**
 * @brief Screen exit function (called once when leaving screen)
 */
typedef void (*screen_exit_fn)(void);

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              SCREEN DEFINITION
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    screen_id_t id;
    const char *name;
    screen_render_fn render;
    screen_input_fn handle_input;
    screen_enter_fn on_enter;       /* Can be NULL */
    screen_exit_fn on_exit;         /* Can be NULL */
} screen_def_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 *                              SCREEN DECLARATIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Home Screen */
void screen_home_render(bool full_redraw);
screen_id_t screen_home_input(button_id_t button);

/* Voltage Screen */
void screen_voltage_render(bool full_redraw);
screen_id_t screen_voltage_input(button_id_t button);

/* Current Screen */
void screen_current_render(bool full_redraw);
screen_id_t screen_current_input(button_id_t button);

/* Temperature Screen */
void screen_temp_render(bool full_redraw);
screen_id_t screen_temp_input(button_id_t button);

/* Gas Screen */
void screen_gas_render(bool full_redraw);
screen_id_t screen_gas_input(button_id_t button);

/* Settings Screen */
void screen_settings_render(bool full_redraw);
screen_id_t screen_settings_input(button_id_t button);
void screen_settings_enter(void);
void screen_settings_exit(void);

/* Fault Log Screen */
void screen_fault_render(bool full_redraw);
screen_id_t screen_fault_input(button_id_t button);

/* System Info Screen */
void screen_system_render(bool full_redraw);
screen_id_t screen_system_input(button_id_t button);

#endif /* SCREENS_H */