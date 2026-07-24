#include "mqtt_manager.h"

#include "config.h"
#include "payload.h"
#include "secrets.h"

#include <algorithm>
#include <Arduino.h>
#include <esp_system.h>

bool MqttManager::begin(const char* clientId, const char* lwtPayload) {
    esp_mqtt_client_config_t config = {};
    config.host = MQTT_HOST;
    config.port = nightshift::MQTT_PORT;
    config.username = MQTT_USERNAME;
    config.password = MQTT_PASSWORD;
    config.client_id = clientId;
    config.keepalive = nightshift::MQTT_KEEPALIVE_S;
    config.disable_auto_reconnect = true;
    config.lwt_topic = MQTT_TOPIC_AVAILABILITY;
    config.lwt_msg = lwtPayload;
    config.lwt_qos = 1;
    config.lwt_retain = true;
    config.buffer_size = PAYLOAD_BUF_SIZE;
    config.out_buffer_size = PAYLOAD_BUF_SIZE;
    config.user_context = this;

    client_ = esp_mqtt_client_init(&config);
    if (client_ == nullptr) {
        Serial.println("[MQTT] client initialization failed");
        return false;
    }

    const esp_err_t registered = esp_mqtt_client_register_event(
        client_,
        static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
        &MqttManager::eventHandler,
        this
    );
    if (registered != ESP_OK) {
        Serial.printf("[MQTT] event registration failed err=%d\n", registered);
        return false;
    }

    Serial.printf("[MQTT] configured client_id=%s broker=%s:%u qos(state)=1\n",
        clientId, MQTT_HOST, nightshift::MQTT_PORT);
    return true;
}

bool MqttManager::reached(uint32_t nowMs, uint32_t dueAtMs) {
    return static_cast<int32_t>(nowMs - dueAtMs) >= 0;
}

void MqttManager::update(uint32_t nowMs) {
    if (connectedEvent_) {
        connectedEvent_ = false;
        connecting_ = false;
        connected_ = true;
        backoffMs_ = nightshift::MQTT_RECONNECT_MIN_MS;
        if (everConnected_) {
            ++reconnectCount_;
        }
        everConnected_ = true;
        connectedEdge_ = true;
        Serial.printf("[MQTT] connected reconnect_count=%lu\n",
            static_cast<unsigned long>(reconnectCount_));
    }

    if (disconnectedEvent_) {
        disconnectedEvent_ = false;
        const bool wasActive = connected_ || connecting_;
        connected_ = false;
        connecting_ = false;
        if (wasActive) {
            Serial.println("[MQTT] disconnected");
        }
        disconnectedEdge_ = true;
        scheduleRetry(nowMs);
    }

    if (!networkReady_ || connected_ || connecting_ || client_ == nullptr) {
        return;
    }

    if (!started_) {
        const esp_err_t result = esp_mqtt_client_start(client_);
        if (result == ESP_OK) {
            started_ = true;
            connecting_ = true;
            Serial.println("[MQTT] connection attempt started");
        } else {
            Serial.printf("[MQTT] start failed err=%d\n", result);
            scheduleRetry(nowMs);
        }
        return;
    }

    if (reached(nowMs, nextAttemptAtMs_)) {
        const esp_err_t result = esp_mqtt_client_reconnect(client_);
        if (result == ESP_OK) {
            connecting_ = true;
            Serial.println("[MQTT] reconnect attempt started");
        } else {
            Serial.printf("[MQTT] reconnect request failed err=%d\n", result);
            scheduleRetry(nowMs);
        }
    }
}

bool MqttManager::enqueue(
    const char* topic,
    const char* payload,
    int qos,
    bool retain
) {
    if (!isConnected() || client_ == nullptr) {
        return false;
    }
    if (esp_mqtt_client_get_outbox_size(client_) >=
        nightshift::MQTT_OUTBOX_LIMIT_BYTES) {
        Serial.println("[MQTT] publish deferred: bounded outbox is full");
        return false;
    }

    const int messageId = esp_mqtt_client_enqueue(
        client_, topic, payload, 0, qos, retain ? 1 : 0, qos > 0
    );
    if (messageId < 0) {
        Serial.printf("[MQTT] enqueue failed topic=%s qos=%d\n", topic, qos);
        return false;
    }
    ++publishCount_;
    return true;
}

bool MqttManager::publishAvailability(const char* payload) {
    return enqueue(MQTT_TOPIC_AVAILABILITY, payload, 1, true);
}

bool MqttManager::publishState(const char* payload) {
    return enqueue(MQTT_TOPIC_STATE, payload, 1, true);
}

bool MqttManager::publishTelemetry(const char* payload) {
    return enqueue(MQTT_TOPIC_TELEMETRY, payload, 0, false);
}

bool MqttManager::takeConnectedEvent() {
    const bool edge = connectedEdge_;
    connectedEdge_ = false;
    return edge;
}

bool MqttManager::takeDisconnectedEvent() {
    const bool edge = disconnectedEdge_;
    disconnectedEdge_ = false;
    return edge;
}

int MqttManager::getOutboxSize() const {
    return client_ == nullptr ? 0 : esp_mqtt_client_get_outbox_size(client_);
}

void MqttManager::eventHandler(
    void* handlerArgs,
    esp_event_base_t,
    int32_t eventId,
    void* eventData
) {
    auto* self = static_cast<MqttManager*>(handlerArgs);
    self->handleEvent(eventId, static_cast<esp_mqtt_event_handle_t>(eventData));
}

void MqttManager::handleEvent(int32_t eventId, esp_mqtt_event_handle_t event) {
    switch (eventId) {
        case MQTT_EVENT_CONNECTED:
            connectedEvent_ = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            disconnectedEvent_ = true;
            break;
        case MQTT_EVENT_ERROR:
            if (event != nullptr && event->error_handle != nullptr) {
                Serial.printf("[MQTT] transport error type=%d connect_rc=%d\n",
                    event->error_handle->error_type,
                    event->error_handle->connect_return_code);
            }
            break;
        default:
            break;
    }
}

void MqttManager::scheduleRetry(uint32_t nowMs) {
    const uint32_t jitterWindow = backoffMs_ / 4U;
    const uint32_t jitter = jitterWindow == 0
        ? 0
        : esp_random() % (jitterWindow + 1U);
    nextAttemptAtMs_ = nowMs + backoffMs_ + jitter;
    Serial.printf("[MQTT] retry in %lu ms\n",
        static_cast<unsigned long>(backoffMs_ + jitter));
    backoffMs_ = std::min(
        backoffMs_ * 2U, nightshift::MQTT_RECONNECT_MAX_MS
    );
}
