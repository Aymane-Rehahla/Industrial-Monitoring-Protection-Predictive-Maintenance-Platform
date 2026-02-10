/**
 * @file hal_adc.h
 * @brief Internal ADC HAL. FROZEN.
 * @version 1.0.1
 * @safety MEDIUM
 */
#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "system_types.h"

typedef struct {
    int32_t        raw;
    int32_t        mv;
    int32_t        filtered_mv;
    uint32_t       timestamp_ms;
    data_quality_t quality;
    bool           rate_ok;
} adc_reading_t;

typedef struct {
    uint32_t total;
    uint32_t valid;
    uint32_t rate_violations;
    int32_t  min_raw;
    int32_t  max_raw;
} adc_chan_stats_t;

error_code_t hal_adc_init(void);
error_code_t hal_adc_read_raw(int channel, int32_t *raw_out);
error_code_t hal_adc_read_mv(int channel, int32_t *mv_out);
error_code_t hal_adc_read_filtered(int channel, adc_reading_t *out);
error_code_t hal_adc_set_rate_limit(int channel, int32_t max_mv_per_s);
error_code_t hal_adc_get_stats(int channel, adc_chan_stats_t *out);
bool         hal_adc_is_calibrated(void);
error_code_t hal_adc_self_test(void);

#endif /* HAL_ADC_H */