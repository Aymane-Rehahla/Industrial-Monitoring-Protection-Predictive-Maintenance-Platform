#include "debug_log.h"
#include "esp_log.h"
static const char *TAG = "DEBUG";
error_code_t debug_log_init(void) { 
    ESP_LOGI(TAG, "Debug log stub"); 
    return ERR_OK; 
}