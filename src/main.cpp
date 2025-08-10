#include "wifi.hpp"
#include "mqtt.hpp"

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h" 

void taskPublish(void* arg) {
    auto* mqttManager = static_cast<MqttManager*>(arg);
    for(;;) {
        // Wait for MQTT connection
        if (mqttManager->waitForConnection(pdMS_TO_TICKS(5000))) {
            esp_err_t result1 = mqttManager->queuePublish("sensor/temperature", "23.5", 0);
            esp_err_t result2 = mqttManager->queuePublish("sensor/humidity", "45.2", 0);
            
            if (result1 != ESP_OK || result2 != ESP_OK) {
                ESP_LOGW("PUBLISH", "Failed to publish some messages");
            } else {
                ESP_LOGI("PUBLISH", "Publish successful!");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
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