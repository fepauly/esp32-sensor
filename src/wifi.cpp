#include "config.hpp"
#include "wifi.hpp"

void WifiManager::init_wifi() 
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &WifiManager::eventHandler,
        this,
        &_wifiEvtInst
    );
    esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &WifiManager::eventHandler,
        this,
         &_ipEvtInst
    );

    wifi_config_t wifi_config{};

    std::copy(cfg::kWifiSsid.begin(), cfg::kWifiSsid.end(), wifi_config.sta.ssid);
    std::copy(cfg::kWifiPass.begin(), cfg::kWifiPass.end(), wifi_config.sta.password);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "initialization finished!");
}

void WifiManager::run()
{
    uint32_t backoffMs = 1000;
    constexpr uint32_t BACKOFF_MAX = 32000;

    for(;;) {
        if(_terminate) {vTaskDelete(nullptr);}

        if (_status == Status::Disconnected) {
            _status = Status::Connecting;
            // Send status in queue
            notify(_status);
            // Try to connect
            esp_wifi_connect();
        }

        EventBits_t bits = xEventGroupWaitBits(
            _eg,
            CONNECTED_BIT | DISCONNECTED_BIT,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(backoffMs)
        );

        if (bits & CONNECTED_BIT) {
            _status = Status::Connected;
            notify(_status);
            backoffMs = 1000;
        } else if (bits & DISCONNECTED_BIT) {
            _status = Status::Disconnected;
            notify(_status);
            backoffMs = std::min(backoffMs * 2, BACKOFF_MAX);
        }
        
    }
}

void WifiManager::notify(Status status) const
{
    if (_statusQueue) {
        xQueueSend(_statusQueue, &status, 0);
    }
}

void WifiManager::taskEntry(void* arg) 
{
        static_cast<WifiManager*>(arg)->run();
}

void WifiManager::eventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    auto* self = static_cast<WifiManager*>(event_handler_arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(self->_eg, DISCONNECTED_BIT);    
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(self->_eg, CONNECTED_BIT);
    }
}