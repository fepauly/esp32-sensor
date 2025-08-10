#include "config.hpp"
#include "wifi.hpp"
#include <algorithm>

WifiManager::WifiManager(QueueHandle_t statusQueue) : _statusQueue(statusQueue)
{
    // Init wifi
    init_wifi();

    // create task
    _task.emplace(
        "Wifi Manager",
        4096,
        [this]() { this->run(); },
        3,
        tskNO_AFFINITY
    );

    _initialized = _eg.getHandle() && _task && _task->getHandle();

    ESP_LOGI("WIFI", "Wifi manager initialized successfully!");
}

WifiManager::~WifiManager()
{
    _terminate.store(true); // delete task

    esp_event_handler_instance_unregister(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        _wifiEvtInst
    );
    esp_event_handler_instance_unregister(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        _ipEvtInst
    );
    esp_wifi_stop();
};

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

    size_t ssidLen = std::min(cfg::kWifiSsid.size(), static_cast<size_t>(sizeof(wifi_config.sta.ssid) - 1));
    std::copy(cfg::kWifiSsid.begin(), cfg::kWifiSsid.begin() + ssidLen, wifi_config.sta.ssid);
    wifi_config.sta.ssid[ssidLen] = '\0';

    size_t passLen = std::min(cfg::kWifiPass.size(), static_cast<size_t>(sizeof(wifi_config.sta.password) - 1));
    std::copy(cfg::kWifiPass.begin(), cfg::kWifiPass.begin() + passLen, wifi_config.sta.password);
    wifi_config.sta.password[passLen] = '\0';

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "initialization finished!");
}

void WifiManager::run()
{
    uint32_t backoffMs = BACKOFF_MS;

    for(;;) {
        if(_terminate.load()) {
            ESP_LOGI("WIFI", "Terminating Wifi manager task");
            return; 
        }

        if (_status.load() == Status::Disconnected) {
            _status.store(Status::Connecting);
            // Send status in queue
            notify(_status);
            // Try to connect
            esp_wifi_connect();
        }

        EventBits_t bits = _eg.wait(
            CONNECTED_BIT | DISCONNECTED_BIT,
            false,
            true,
            pdMS_TO_TICKS(backoffMs)
        );

        if (bits & CONNECTED_BIT) {
            _status.store(Status::Connected);
            notify(_status);
            backoffMs = BACKOFF_MS;
        } else if (bits & DISCONNECTED_BIT) {
            _status.store(Status::Disconnected);
            notify(_status);
            backoffMs = std::min(backoffMs * 2, BACKOFF_MAX_MS);
        }  
    }
}

void WifiManager::notify(Status status) const
{
    if (_statusQueue) {
        xQueueSend(_statusQueue, &status, 0);
    }
}

void WifiManager::eventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    auto* self = static_cast<WifiManager*>(event_handler_arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        self->_eg.set(DISCONNECTED_BIT);  
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        self->_eg.set(CONNECTED_BIT);
    }
}