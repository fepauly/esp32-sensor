#pragma once

#include "wifi.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include <atomic>

struct PublishMessage {
    char topic[64];
    char payload[256];
    int qos;
    int retain;
    uint8_t retryCount;
};

/* Manages mqtt connection */
class MqttManager
{
    public:
        enum class Status : uint8_t {Connected, Disconnected};
        explicit MqttManager(QueueHandle_t statusQueue = nullptr, QueueHandle_t pubQueue = nullptr);
        ~MqttManager();

        /* Publish payload directly */
        esp_err_t publish(const char* topic, const char* payload, int qos = 0) const;
        /* Publish payload via queue */
        esp_err_t queuePublish(const char* topic, const char* payload, int qos = 0) const;
        /* Get current connection status */
        Status current() const noexcept { return _status.load(); }
        /* Wait for connection to mqtt broker */
        bool waitForConnection(TickType_t timeout);
        /* manager is correctly initialized */
        bool isValid() {return _initialized;};

    private:
    /* Task entry point */
    static void taskEntry(void* arg);
    /* Main Loop */
    void run();
    /* Mqtt event handler callback */
    static void eventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    QueueHandle_t _wifiStatusQueue{};
    QueueHandle_t _pubQueue{};
    esp_mqtt_client_handle_t _client{};
    std::atomic<bool> _terminate{false};
    std::atomic<Status> _status{Status::Disconnected};
    EventGroupHandle_t _eg{nullptr};
    TaskHandle_t _taskHandle{};
    char _mqttUri[128]{};
    bool _initialized{false};

    static constexpr EventBits_t CONNECTED_BIT = BIT0;
    static constexpr EventBits_t DISCONNECTED_BIT =  BIT1;
    static constexpr uint8_t MAX_RETRY_COUNT = 3;
    static constexpr uint32_t TASK_LOOP_DELAY_MS = 50;
    static constexpr uint32_t QUEUE_SEND_TIMEOUT_MS = 100;
    static constexpr uint32_t QUEUE_RETRY_TIMEOUT_MS = 500;
    
};