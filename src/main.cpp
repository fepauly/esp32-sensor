#include "wifi.hpp"

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h" 

static QueueHandle_t wifiStatusQ = nullptr;
static WifiManager* wifi = nullptr;

/** Debug task to log wifi status changes */
void taskDebug(void*  arg){
    WifiManager::Status state{};
    for(;;) {
        if (wifiStatusQ &&  xQueueReceive(wifiStatusQ, &state, portMAX_DELAY)) {
            if (state == WifiManager::Status::Connected) {
                ESP_LOGI("taskDebug", "Wifi is connected!");
            } else if (state == WifiManager::Status::Disconnected) {
                ESP_LOGI("taskDebug", "Wifi is disconnected!");
            }
        }
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

    wifiStatusQ = xQueueCreate(4, sizeof(WifiManager::Status));
    if(wifiStatusQ == nullptr) {
        ESP_LOGE("MAIN", "Failed to create Wifi Status queue!");
        return;
    }

    static WifiManager wifiManager{wifiStatusQ};
    wifi = &wifiManager;

    // Run debuf task
    xTaskCreatePinnedToCore(
        taskDebug,
        "Debug",
        2048,
        nullptr,
        1,
        nullptr,
        tskNO_AFFINITY
    );
}