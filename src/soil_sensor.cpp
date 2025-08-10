#include "soil_sensor.hpp"
#include <algorithm>

SoilMoistureSensor::SoilMoistureSensor() {
    _initialized = initAdc();
    if (_initialized) {
        ESP_LOGI("SOIL", "Soil moisture sensor initialized successfully");
    } else {
        ESP_LOGE("SOIL", "Failed to initialize soil moisture sensor");
    }
}

SoilMoistureSensor::~SoilMoistureSensor() {
    if (_initialized) {
        adc_oneshot_del_unit(_adcHandle);
    }
}

bool SoilMoistureSensor::initAdc() {
    // ADC init configuration
    adc_oneshot_unit_init_cfg_t initConfig{};
    initConfig.unit_id = ADC_UNIT;

    
    // ADC channel configuration
    adc_oneshot_chan_cfg_t chanConfig{};

    chanConfig.bitwidth = ADC_WIDTH;
    chanConfig.atten = ADC_ATTEN;
    
    // Initialize ADC
    esp_err_t ret = adc_oneshot_new_unit(&initConfig, &_adcHandle);
    if (ret != ESP_OK) {
        ESP_LOGE("SOIL", "Failed to initialize ADC: %d", ret);
        return false;
    }
    
    // Configure ADC channel
    ret = adc_oneshot_config_channel(_adcHandle, ADC_CHANNEL, &chanConfig);
    if (ret != ESP_OK) {
        ESP_LOGE("SOIL", "Failed to configure ADC channel: %d", ret);
        adc_oneshot_del_unit(_adcHandle);
        return false;
    }
    
    return true;
}

int SoilMoistureSensor::readRawValue() {
    if (!_initialized) {
        return -1;
    }
    
    int rawValue = 0;
    esp_err_t ret = adc_oneshot_read(_adcHandle, ADC_CHANNEL, &rawValue);
    if (ret != ESP_OK) {
        ESP_LOGE("SOIL", "ADC read error: %d", ret);
        return -1;
    }
    
    return rawValue;
}

int SoilMoistureSensor::readMoisturePercent() {
    int rawValue = readRawValue();
    if (rawValue < 0) {
        return -1;
    }
    
    // Map ADC value to moisture percentage (0-100%)
    // Constrain values to the calibration range
    rawValue = std::max(std::min(rawValue, AIR_VALUE), WATER_VALUE);
    
    // Convert to percentage
    int moisturePercent = map(rawValue, AIR_VALUE, WATER_VALUE, 0, 100);
    
    return moisturePercent;
}

// Helper function to map values from one range to another
int SoilMoistureSensor::map(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}