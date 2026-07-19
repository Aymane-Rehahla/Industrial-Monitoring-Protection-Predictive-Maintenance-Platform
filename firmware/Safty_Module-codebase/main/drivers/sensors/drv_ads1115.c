#include "drivers/sensors/drv_ads1115.h"
#include "app_config.h"
#include "hal/hal_i2c.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "drv_ads1115";

#define ADS1115_REG_CONVERSION 0x00U
#define ADS1115_REG_CONFIG     0x01U

#define ADS1115_CFG_OS_SINGLE      0x8000U
#define ADS1115_CFG_PGA_4V096      0x0200U
#define ADS1115_CFG_MODE_SINGLE    0x0100U
#define ADS1115_CFG_DR_860SPS      0x00E0U
#define ADS1115_CFG_COMP_DISABLE   0x0003U
#define ADS1115_LSB_MV_4V096       0.125f

static bool is_supported_addr(uint8_t dev_addr)
{
    return dev_addr == I2C_ADDR_ADS1115_VOLTAGE ||
           dev_addr == I2C_ADDR_ADS1115_CURRENT;
}

error_code_t drv_ads1115_init(uint8_t dev_addr)
{
    if (!is_supported_addr(dev_addr)) { return ERR_INVALID_ARG; }
    if (!hal_i2c_is_initialized(I2C_BUS_SENSORS)) { return ERR_NOT_INITIALIZED; }

    error_code_t rc = hal_i2c_probe(I2C_BUS_SENSORS, dev_addr);
    if (rc != ERR_OK) {
        ESP_LOGE(TAG, "ADS1115 not found at 0x%02X", dev_addr);
        return ERR_HW_NOT_FOUND;
    }

    ESP_LOGI(TAG, "ADS1115 ready at 0x%02X", dev_addr);
    return ERR_OK;
}

bool drv_ads1115_is_present(uint8_t dev_addr)
{
    if (!is_supported_addr(dev_addr)) { return false; }
    return hal_i2c_probe(I2C_BUS_SENSORS, dev_addr) == ERR_OK;
}

error_code_t drv_ads1115_read_raw(uint8_t dev_addr,
                                  uint8_t channel,
                                  int16_t *raw_out)
{
    if (raw_out == NULL) { return ERR_NULL_POINTER; }
    if (!is_supported_addr(dev_addr) || channel >= ADS1115_CHANNEL_COUNT) {
        return ERR_INVALID_ARG;
    }
    if (!hal_i2c_is_initialized(I2C_BUS_SENSORS)) { return ERR_NOT_INITIALIZED; }

    uint16_t mux = (uint16_t)(0x04U + channel) << 12;
    uint16_t cfg = ADS1115_CFG_OS_SINGLE |
                   mux |
                   ADS1115_CFG_PGA_4V096 |
                   ADS1115_CFG_MODE_SINGLE |
                   ADS1115_CFG_DR_860SPS |
                   ADS1115_CFG_COMP_DISABLE;

    uint8_t tx[3] = {
        ADS1115_REG_CONFIG,
        (uint8_t)(cfg >> 8),
        (uint8_t)(cfg & 0xFFU)
    };

    error_code_t rc = hal_i2c_write(I2C_BUS_SENSORS, dev_addr,
                                    tx, sizeof(tx), I2C_BUS0_TIMEOUT_MS);
    if (rc != ERR_OK) { return rc; }

    vTaskDelay(pdMS_TO_TICKS(2));

    uint8_t reg = ADS1115_REG_CONVERSION;
    uint8_t rx[2] = {0};
    rc = hal_i2c_write_then_read(I2C_BUS_SENSORS, dev_addr,
                                 &reg, 1, rx, sizeof(rx),
                                 I2C_BUS0_TIMEOUT_MS);
    if (rc != ERR_OK) { return rc; }

    *raw_out = (int16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    return ERR_OK;
}

error_code_t drv_ads1115_read_millivolts(uint8_t dev_addr,
                                         uint8_t channel,
                                         float *mv_out)
{
    if (mv_out == NULL) { return ERR_NULL_POINTER; }

    int16_t raw = 0;
    error_code_t rc = drv_ads1115_read_raw(dev_addr, channel, &raw);
    if (rc != ERR_OK) { return rc; }

    *mv_out = (float)raw * ADS1115_LSB_MV_4V096;
    return ERR_OK;
}
