#include "wifi.hpp"
#include "mqtt.hpp"
#include "soil_sensor.hpp"

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h" 

void taskPublish(void* arg) {
    auto* mqttManager = static_cast<MqttManager*>(arg);
    
    SoilMoistureSensor soilSensor{};
    
    if (!soilSensor.isValid()) {
        ESP_LOGE("PUBLISH", "Failed to initialize soil sensor");
        vTaskDelete(nullptr);
        return;
    }

    char buffer[16];
    
    for(;;) {
        if (mqttManager->waitForConnection(pdMS_TO_TICKS(5000))) {
            // Read soil moisture 
            int moisturePercent = soilSensor.readMoisturePercent();
            int rawValue = soilSensor.readRawValue();
            
            if (moisturePercent >= 0) {
                snprintf(buffer, sizeof(buffer), "%d", moisturePercent);
                esp_err_t result1 = mqttManager->queuePublish("sensor/soil/moisture", buffer, 0);
                
                snprintf(buffer, sizeof(buffer), "%d", rawValue);
                esp_err_t result2 = mqttManager->queuePublish("sensor/soil/raw", buffer, 0);
                
                if (result1 != ESP_OK || result2 != ESP_OK) {
                    ESP_LOGW("PUBLISH", "Failed to publish soil moisture data");
                } else {
                    ESP_LOGI("PUBLISH", "Soil moisture: %d%%, Raw: %d", moisturePercent, rawValue);
                }
            } else {
                ESP_LOGW("PUBLISH", "Failed to read soil moisture sensor");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Read every 10 seconds
    }
}

extern "C" void app_main() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    QueueHandle_t wifiStatusQ = xQueueCreate(4, sizeof(WifiManager::Status));
    QueueHandle_t mqttPubQ = xQueueCreate(10, sizeof(PublishMessage));

    if((wifiStatusQ == nullptr) || (mqttPubQ == nullptr)) {
        ESP_LOGE("MAIN", "Failed to create queues!");
        esp_restart();
        return;
    }

    static WifiManager wifiManager{wifiStatusQ};
    static MqttManager mqttManager{wifiStatusQ, mqttPubQ};

    if(!wifiManager.isValid() || !mqttManager.isValid()) {
        ESP_LOGE("MAIN", "Failed to initialize managers!");
        esp_restart();
        return;
    }

    // Example publish task
    // TODO: Use RAII Task Wrapper
    xTaskCreatePinnedToCore(
        taskPublish,
        "Publish",
        2048,
        &mqttManager,
        2,
        nullptr,
        tskNO_AFFINITY
    );
}