#include "mqtt.hpp"
#include "config.hpp"

MqttManager::MqttManager(QueueHandle_t statusQueue, QueueHandle_t pubQueue) 
    : _wifiStatusQueue(statusQueue), _pubQueue(pubQueue)
{
    // Create config
    esp_mqtt_client_config_t mqtt_cfg = {};

    // Use static string to ensure null termination
    strncpy(_mqttUri, cfg::kMqttBrokerUri.data(), 
            std::min<size_t>(cfg::kMqttBrokerUri.size(), sizeof(_mqttUri) - 1));
    _mqttUri[sizeof(_mqttUri) - 1] = '\0';
    
    // Use URI instead of hostname/port
    mqtt_cfg.broker.address.uri = _mqttUri;
    mqtt_cfg.network.disable_auto_reconnect = false,
    mqtt_cfg.session.keepalive = 30,
    mqtt_cfg.task.priority = 5;

    // Create event group
    _eg = xEventGroupCreate();

    if(!_eg) {
        ESP_LOGE("MQTT", "Failed to create event group!");
        return;
    }

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

    xTaskCreatePinnedToCore(
        taskEntry,
        "MqttManager",
        4096,
        this,
        4,
        &_taskHandle,
        tskNO_AFFINITY
    );
    _initialized = true;
    ESP_LOGI("MQTT", "Mqtt Manager initialized successfully!");
}

MqttManager::~MqttManager()
{
    _terminate.store(true); // delete task

    if(_taskHandle) {
        for(int i = 0; i < 10 && eTaskGetState(_taskHandle) != eDeleted; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if(_taskHandle && eTaskGetState(_taskHandle) != eDeleted) {
            ESP_LOGW("MQTT", "Task did not terminate correctly, forcing.");
            vTaskDelete(_taskHandle);
        }
    }

    esp_mqtt_client_unregister_event(
        _client,
        MQTT_EVENT_ANY,
        &MqttManager::eventHandler
    );

    esp_mqtt_client_destroy(_client);
    
    if (_eg) {
        vEventGroupDelete(_eg);
    }
};

bool MqttManager::waitForConnection(TickType_t timeout)
{
    EventBits_t bits = xEventGroupWaitBits(
        _eg,
        CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        timeout
    );
    
    return (bits & CONNECTED_BIT) != 0;
}

void MqttManager::run()
{
    WifiManager::Status wifiState{};
    PublishMessage pubMsg{};

    for(;;) {
        if(_terminate.load()) {vTaskDelete(nullptr);};
        
        // Handle wifi status change
        if(xQueueReceive(_wifiStatusQueue, &wifiState, 0)) {
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
        if(_pubQueue && xQueueReceive(_pubQueue, &pubMsg, 0)) {
            if(_status.load() == Status::Connected) {
                esp_err_t result = esp_mqtt_client_publish(
                    _client,
                    pubMsg.topic,
                    pubMsg.payload,
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

        // Delay task
        vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_DELAY_MS));
    }
}

void MqttManager::eventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto *self = static_cast<MqttManager*>(event_handler_arg);

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            self->_status.store(Status::Connected);
            xEventGroupSetBits(self->_eg, CONNECTED_BIT);
            ESP_LOGI("MQTT", "Connected to broker");
            break;
        case MQTT_EVENT_DISCONNECTED:
            self->_status.store(Status::Disconnected);
            xEventGroupClearBits(self->_eg, CONNECTED_BIT);
            xEventGroupSetBits(self->_eg, DISCONNECTED_BIT);
            ESP_LOGW("MQTT", "Disconnected from broker");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE("MQTT", "MQTT Error occurred");
            break;
    }
}

void MqttManager::taskEntry(void* arg) 
{
    static_cast<MqttManager*>(arg)->run();
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
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';

    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';

    msg.qos = qos;
    msg.retain = 0;
    msg.retryCount = 0;

    if (xQueueSend(_pubQueue, &msg, pdMS_TO_TICKS(QUEUE_SEND_TIMEOUT_MS)) != pdPASS) {
        ESP_LOGW("MQTT", "Failed to queue publish message for topic %s", msg.topic);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}