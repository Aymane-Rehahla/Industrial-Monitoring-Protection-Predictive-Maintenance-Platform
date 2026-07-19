// ═══ FILE: main/hmi/screens/comm/screen_pairing.c ═══
/**
 * @file    screen_pairing.c
 * @brief   Peer ESP32 pairing status and MAC address display.
 * @version 1.0.0
 * @date    2025-01-01
 * @safety  LOW — display and navigation only.
 *
 * CHANGELOG:
 *   1.0.0  2025-01-01  Initial release.
 */

#include "hmi/screens/screens.h"
#include "system_types.h"
#include "app_config.h"

#include "drivers/interface/drv_lcd2004.h"
#include "core/redundancy/redundancy.h"
#include "hmi/hmi_manager.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "screen_pairing";

void screen_pairing_enter(void)
{
    ESP_LOGI(TAG, "enter");
    drv_lcd2004_clear();
}

void screen_pairing_update(void)
{
    peer_info_t info;
    redundancy_get_peer_info(&info);

    char buf[LCD_COLS + 1];

    drv_lcd2004_write_line(0, "   PEER PAIRING   ");

    snprintf(buf, sizeof(buf), " %02X:%02X:%02X:%02X:%02X:%02X",
             info.peer_mac[0], info.peer_mac[1], info.peer_mac[2],
             info.peer_mac[3], info.peer_mac[4], info.peer_mac[5]);
    drv_lcd2004_write_line(1, buf);

    snprintf(buf, sizeof(buf), "Status: %-10s",
             peer_to_str((int)info.status));
    drv_lcd2004_write_line(2, buf);

    drv_lcd2004_write_line(3, "OK=Edit MAC < Back");
}

bool screen_pairing_handle_event(const button_event_t *e)
{
    if (e == NULL) { return false; }
    if (e->event != BTN_EVENT_PRESSED) { return false; }

    if (e->button == BTN_OK) {
        hmi_manager_request_screen(SCREEN_MAC_ENTRY);
        return true;
    }

    if (e->button == BTN_LEFT) {
        return false;  /* Pop back. */
    }

    return false;
}

void screen_pairing_exit(void)
{
    /* Nothing to clean up. */
}