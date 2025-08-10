#include "mqtt.hpp"
#include "config.hpp"
#include <algorithm>

MqttManager::MqttManager(QueueHandle_t statusQueue, QueueHandle_t pubQueue) 
    : _wifiStatusQueue(statusQueue), _pubQueue(pubQueue)
{
    // Create config
    esp_mqtt_client_config_t mqtt_cfg = {};

    strncpy(_mqttUri, cfg::kMqttBrokerUri.data(), 
            std::min<size_t>(cfg::kMqttBrokerUri.size(), sizeof(_mqttUri) - 1));
    _mqttUri[sizeof(_mqttUri) - 1] = '\0';
    
    mqtt_cfg.broker.address.uri = _mqttUri;
    mqtt_cfg.network.disable_auto_reconnect = false,
    mqtt_cfg.session.keepalive = 30,
    mqtt_cfg.task.priority = 5;

    _client = esp_mqtt_client_init(&mqtt_cfg);
    if (!_client) {
        ESP_LOGE("MQTT", "Failed to create mqtt client!");
        return;
    }

    // event callback
    esp_mqtt_client_register_event(
        _client,
        MQTT_EVENT_ANY,
        &MqttManager::eventHandler,
        this
    );

    _task.emplace(
        "MQTT Manager",
        4096,
        [this](){ this->run(); },
        3,
        tskNO_AFFINITY
    );

    _initialized = _eg.getHandle() && _task && _task->getHandle();
    ESP_LOGI("MQTT", "Mqtt Manager initialized successfully!");
}

MqttManager::~MqttManager()
{
    _terminate.store(true); // delete task

    esp_mqtt_client_unregister_event(
        _client,
        MQTT_EVENT_ANY,
        &MqttManager::eventHandler
    );

    esp_mqtt_client_destroy(_client);
};

bool MqttManager::waitForConnection(TickType_t timeout)
{
    EventBits_t bits = _eg.wait(
        CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        timeout
    );
    
    return (bits & CONNECTED_BIT) != 0;
}

void MqttManager::run()
{
    // TODO: Make queue operations non blocking to handle termination properly
    WifiManager::Status wifiState{};
    PublishMessage pubMsg{};
    bool processedQueue = false;

    for(;;) {
        if(_terminate.load()) {
            ESP_LOGI("MQTT", "Terminating mqtt manager task");
            return;
        };
        
        // Handle wifi status change
        if(xQueueReceive(_wifiStatusQueue, &wifiState, pdMS_TO_TICKS(500))) {
            processedQueue = true;
            if (wifiState == WifiManager::Status::Connected && _status.load() == Status::Disconnected) {
                ESP_LOGI("MQTT", "Wifi connected, starting MQTT client");
                esp_mqtt_client_start(_client);
            } else if (wifiState == WifiManager::Status::Disconnected && _status.load() == Status::Connected) {
                ESP_LOGI("MQTT", "Wifi disconnected, stopping MQTT client");
                esp_mqtt_client_stop(_client);
                _status.store(Status::Disconnected);
            }   
        } 

        // Handle publish messages from queue
        if(_pubQueue) {
            while(xQueueReceive(_pubQueue, &pubMsg, 0) == pdTRUE) {
                processedQueue = true;
                if(_status.load() == Status::Connected) {
                    esp_err_t result = esp_mqtt_client_publish(
                        _client,
                        pubMsg.topic.data(),
                        pubMsg.payload.data(),
                        0,
                        pubMsg.qos,
                        pubMsg.retain
                    );

                    if(result < 0) {
                        ESP_LOGE("MQTT", "Failed to publish topic %s with payload %s", pubMsg.topic, pubMsg.payload);
                        if (pubMsg.retryCount < MAX_RETRY_COUNT) {
                            pubMsg.retryCount++;
                            xQueueSendToFront(_pubQueue, &pubMsg, pdMS_TO_TICKS(QUEUE_RETRY_TIMEOUT_MS));
                        } else {
                            ESP_LOGE("MQTT", "Retry limit reached for topic %s with payload %s, dropping.", pubMsg.topic, pubMsg.payload);
                        }
                    } else {
                        ESP_LOGD("MQTT", "Published topic %s with payload %s", pubMsg.topic, pubMsg.payload);
                    }
                } else {
                    ESP_LOGD("MQTT", "MQTT offline, keeping message for later: %s", pubMsg.topic);
                    if (xQueueSendToBack(_pubQueue, &pubMsg, 0) != pdTRUE) {
                        ESP_LOGW("MQTT", "Queue full, dropping offline message");
                    }
                }
            }
        }

        // Delay task
        if(!processedQueue) {
            vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_DELAY_MS));
        }
    }
}

void MqttManager::eventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto *self = static_cast<MqttManager*>(event_handler_arg);

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            self->_status.store(Status::Connected);
            self->_eg.set(CONNECTED_BIT);
            ESP_LOGI("MQTT", "Connected to broker");
            break;
        case MQTT_EVENT_DISCONNECTED:
            self->_status.store(Status::Disconnected);
            self->_eg.set(DISCONNECTED_BIT);
            ESP_LOGW("MQTT", "Disconnected from broker");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE("MQTT", "MQTT Error occurred");
            break;
    }
}

esp_err_t MqttManager::publish(const char* topic, const char* payload, int qos) const
{
    if (_status.load() != Status::Connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return esp_mqtt_client_publish(_client, topic, payload, 0, qos, 0);
}

esp_err_t MqttManager::queuePublish(const char* topic, const char* payload, int qos) const
{
    if (!_pubQueue || !topic || !payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(topic) >= sizeof(PublishMessage::topic) || 
        strlen(payload) >= sizeof(PublishMessage::payload)) {
        ESP_LOGW("MQTT", "Message too large - topic: %zu, payload: %zu", 
                 strlen(topic), strlen(payload));
        return ESP_ERR_INVALID_SIZE;
    }

    PublishMessage msg{};
    std::copy_n(topic, std::min(strlen(topic), msg.topic.size() - 1), msg.topic.begin());
    msg.topic[std::min(strlen(topic), msg.topic.size() - 1)] = '\0';

    std::copy_n(payload, std::min(strlen(payload), msg.payload.size() - 1), msg.payload.begin());
    msg.payload[std::min(strlen(payload), msg.payload.size() - 1)] = '\0';

    msg.qos = qos;
    msg.retain = 0;
    msg.retryCount = 0;

    if (xQueueSend(_pubQueue, &msg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) != pdPASS) {
        ESP_LOGW("MQTT", "Failed to queue publish message for topic %s", msg.topic.data());
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}