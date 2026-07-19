# ═══ FILE: main/CMakeLists.txt ═══
# Safety Module — ESP-IDF component build file
# Complete source list from ALL prompts (1 through 6).
#
# WHY REQUIRES vs PRIV_REQUIRES:
#   REQUIRES     = components whose headers appear in OUR public headers
#   PRIV_REQUIRES = components used only in our .c files (not exposed)

idf_component_register(
    SRCS
        # Application entry point
        "main.c"

        # HAL layer
        "hal/hal_gpio.c"
        "hal/hal_i2c.c"
        "hal/hal_adc.c"
        "hal/hal_uart.c"

        # Drivers — Interface
        "drivers/interface/drv_lcd2004.c"
        "drivers/interface/drv_buttons.c"

        # Drivers — Actuators
        "drivers/actuators/drv_buzzer.c"
        "drivers/actuators/drv_leds.c"
        "drivers/actuators/drv_relay.c"

        # Drivers - Safety ADC sensors
        "drivers/sensors/drv_ads1115.c"
        "drivers/sensors/drv_voltage_sensor.c"
        "drivers/sensors/drv_current_sensor.c"

        # Core modules
        "core/system_status.c"
        "core/config_manager.c"
        "core/measurement/measurement.c"
        "core/protection/protection.c"
        "core/protection/fault_handler.c"
        "core/redundancy/redundancy.c"
        "core/sensor_manager/sensor_manager.c"

        # Storage
        "storage/nvs_config.c"

        # Utilities
        "utils/crc_utils.c"

        # Communication
        "comm/espnow_link.c"
        "comm/pairing.c"
        "comm/telemetry.c"

        # HMI framework
        "hmi/hmi_manager.c"
        "hmi/menu_engine.c"
        "hmi/led_status.c"
        "hmi/ui_animations.c"
        "hmi/screens/screen_stubs.c"

        # Navigation / special screens
        "hmi/screens/nav/screen_boot.c"
        "hmi/screens/nav/screen_fault.c"
        "hmi/screens/nav/screen_home.c"
        "hmi/screens/nav/screen_voltage.c"
        "hmi/screens/nav/screen_current.c"
        "hmi/screens/nav/screen_temp.c"
        "hmi/screens/nav/screen_gas.c"
        "hmi/screens/nav/screen_vibration.c"
        "hmi/screens/nav/screen_rpm.c"
        "hmi/screens/nav/screen_system.c"

        # Menu data tables
        "hmi/menus/menu_main.c"
        "hmi/menus/menu_settings.c"

        # Settings screens
        "hmi/screens/settings/screen_settings.c"
        "hmi/screens/settings/screen_threshold.c"
        "hmi/screens/settings/screen_diagnostics.c"
        "hmi/screens/settings/screen_inject_fault.c"

        # Sensor screens
        "hmi/screens/sensors/screen_sensor_view.c"
        "hmi/screens/sensors/screen_sensor_add.c"
        "hmi/screens/sensors/screen_sensor_remove.c"
        "hmi/screens/sensors/screen_sensor_test.c"

        # Calibration screens
        "hmi/screens/calibration/screen_cal_select.c"
        "hmi/screens/calibration/screen_cal_manual.c"
        "hmi/screens/calibration/screen_cal_auto.c"

        # Communication screens
        "hmi/screens/comm/screen_pairing.c"
        "hmi/screens/comm/screen_mac_entry.c"

    INCLUDE_DIRS
        "."
        "hal"
        "drivers/interface"
        "drivers/actuators"
        "drivers/sensors"
        "core"
        "core/measurement"
        "core/protection"
        "core/redundancy"
        "core/sensor_manager"
        "core/state_machine"
        "hmi"
        "hmi/screens"
        "hmi/menus"
        "comm"
        "storage"
        "utils"

    # Components whose headers we use in OUR public .h files
    REQUIRES
        driver
        freertos

    # Components used only inside our .c files
    PRIV_REQUIRES
        esp_rom
        soc
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_timer
)
