#pragma once
#include "config.hpp"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <array>
#include <atomic>


/**
 * Manages wifi connection with automatic reconnection and status notifications
 */
class WifiManager
{
    public:
    enum class Status : uint8_t {Disconnected, Connecting, Connected};

    /** Init wifi and start management task */
    explicit WifiManager(QueueHandle_t statusQueue = nullptr) : _statusQueue(statusQueue)
    {
        // Init wifi
        init_wifi();
        // Start RTOS Task
        xTaskCreatePinnedToCore(
            taskEntry,
            "WifiManager",
            4096,
            this,
            3,
            nullptr,
            tskNO_AFFINITY
        );
    };

    /** stop wifi and unregister handlers */
    ~WifiManager()
    {
        _terminate.store(true); // delete task
        esp_wifi_stop();
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
    };

    /** Get current wifi status */
    Status current() const noexcept { return _status.load(); }

    
    private:
    /** Setup wifi and event handlers */
    void init_wifi();
    /** Main connection loop with backoff retry */
    void run();
    /** Send wifi connection status update to queue */
    void notify(Status status) const;
    /** Task entry point */
    static void taskEntry(void* arg);
    /** Handle wifi and IP events */
    static void eventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    std::atomic<bool> _terminate{false};
    std::atomic<Status> _status{Status::Disconnected};

    QueueHandle_t _statusQueue{};
    esp_event_handler_instance_t _wifiEvtInst{}, _ipEvtInst{};
    EventGroupHandle_t _eg{xEventGroupCreate()};

    static constexpr EventBits_t CONNECTED_BIT = BIT0;
    static constexpr EventBits_t DISCONNECTED_BIT =  BIT1;
};