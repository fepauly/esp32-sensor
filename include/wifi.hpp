#pragma once
#include "config.hpp"
#include "freertos_eg.hpp"
#include "freertos_task.hpp"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <array>
#include <atomic>
#include <optional>


/**
 * Manages wifi connection with automatic reconnection and status notifications
 */
class WifiManager
{
public:
    enum class Status : uint8_t {Disconnected, Connecting, Connected};

    /* Init wifi and start management task */
    explicit WifiManager(QueueHandle_t statusQueue = nullptr);    
    /* stop wifi and unregister handlers */
    ~WifiManager();
    /* Get current wifi status */
    Status current() const noexcept { return _status.load(); }
    /* is manager initialized correctly */
    bool isValid() {return _initialized;};
    
private:
    /** Setup wifi and event handlers */
    void init_wifi();
    /** Main connection loop with backoff retry */
    void run();
    /** Send wifi connection status update to queue */
    void notify(Status status) const;
    /** Handle wifi and IP events */
    static void eventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    std::atomic<bool> _terminate{false};
    std::atomic<Status> _status{Status::Disconnected};

    QueueHandle_t _statusQueue{};
    esp_event_handler_instance_t _wifiEvtInst{}, _ipEvtInst{};
    bool _initialized{false};

    FreeRtosEventGroup _eg{"Wifi Events"};
    std::optional<FreeRtosTask> _task;

    static constexpr EventBits_t CONNECTED_BIT = BIT0;
    static constexpr EventBits_t DISCONNECTED_BIT =  BIT1;
    static constexpr uint32_t BACKOFF_MS = 1000;
    static constexpr uint32_t BACKOFF_MAX_MS = 32000;
};