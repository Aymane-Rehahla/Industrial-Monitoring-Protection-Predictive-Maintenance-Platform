/**
 * @file main.c
 * @brief Firmware version selector
 * @version 1.0.0
 * 
 * Change FIRMWARE_VERSION to select which version to compile:
 *   1 = Sensor test (hardware validation)
 *   2 = Integrated (ESP-NOW + more features)
 *   3 = Full protection (production)
 */

#define FIRMWARE_VERSION  1

#if FIRMWARE_VERSION == 1
    #include "main_v1_sensor_test.c"
#elif FIRMWARE_VERSION == 2
    #include "main_v2_integrated.c"
#elif FIRMWARE_VERSION == 3
    #include "main_v3_protection.c"
#else
    #error "Invalid FIRMWARE_VERSION. Use 1, 2, or 3."
#endif