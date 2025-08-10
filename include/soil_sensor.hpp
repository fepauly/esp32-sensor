#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include <cstdint>
#include <atomic>
#include <optional>
#include "freertos_task.hpp"

class SoilMoistureSensor {
public:
    SoilMoistureSensor();
    ~SoilMoistureSensor();

    // Read current moisture level (0-100%)
    int readMoisturePercent();
    int readRawValue();
    bool isValid() const { return _initialized; }

private:
    // Initialize ADC
    bool initAdc();
    int map(int x, int in_min, int in_max, int out_min, int out_max);
    
    // Constants for calibration
    static constexpr adc_channel_t ADC_CHANNEL = ADC_CHANNEL_4;  // GPIO 32 = ADC1_CH4
    static constexpr adc_unit_t ADC_UNIT = ADC_UNIT_1;
    static constexpr adc_bitwidth_t ADC_WIDTH = ADC_BITWIDTH_12;
    static constexpr adc_atten_t ADC_ATTEN = ADC_ATTEN_DB_12; // 0-3.3V range
    
    // Calibration values
    static constexpr int AIR_VALUE = 3000;    // TODO: Needs calibrations
    static constexpr int WATER_VALUE = 1400;  // TODO: Needs calibration
    
    // ADC handle
    adc_oneshot_unit_handle_t _adcHandle;
    bool _initialized{false};
};