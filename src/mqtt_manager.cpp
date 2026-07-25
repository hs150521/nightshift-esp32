#include "mqtt_manager.h"

#include "config.h"
#include "payload.h"
#include "secrets.h"

#include <algorithm>
#include <Arduino.h>
#include <esp_system.h>

namespace {

constexpr UBaseType_t MQTT_LIFECYCLE_QUEUE_LENGTH = 8;

}  // namespace

bool MqttManager::begin(const char* clientId, const char* lwtPayload) {
    lifecycleQueue_ = xQueueCreate(
        MQTT_LIFECYCLE_QUEUE_LENGTH,
        sizeof(QueuedLifecycleEvent)
    );
    if (lifecycleQueue_ == nullptr) {
        Serial.println("[MQTT] lifecycle queue allocation failed");
        return false;
    }

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
        vQueueDelete(lifecycleQueue_);
        lifecycleQueue_ = nullptr;
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
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        vQueueDelete(lifecycleQueue_);
        lifecycleQueue_ = nullptr;
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
    QueuedLifecycleEvent event;
    while (
        lifecycleQueue_ != nullptr &&
        xQueueReceive(lifecycleQueue_, &event, 0) == pdTRUE
    ) {
        const bool wasConnecting = lifecycleState_.isConnecting();
        const MqttLifecycleTransition transition =
            lifecycleState_.apply(event.type);

        if (transition.resetBackoff) {
            backoffMs_ = nightshift::MQTT_RECONNECT_MIN_MS;
        }
        if (transition.becameConnected) {
            Serial.printf("[MQTT] connected generation=%lu reconnect_count=%lu\n",
                static_cast<unsigned long>(
                    lifecycleState_.connectionGeneration()
                ),
                static_cast<unsigned long>(lifecycleState_.reconnectCount()));
        }
        if (transition.becameDisconnected || wasConnecting) {
            Serial.println("[MQTT] disconnected");
        }
        if (transition.scheduleReconnect) {
            scheduleRetry(nowMs);
        }
        if (event.type == MqttLifecycleEventType::Error) {
            Serial.printf("[MQTT] transport error type=%d connect_rc=%d\n",
                event.errorType, event.connectReturnCode);
        }
    }

    if (
        !networkReady_ ||
        lifecycleState_.isConnected() ||
        lifecycleState_.isConnecting() ||
        client_ == nullptr
    ) {
        return;
    }

    if (!started_) {
        const esp_err_t result = esp_mqtt_client_start(client_);
        if (result == ESP_OK) {
            started_ = true;
            lifecycleState_.markConnecting();
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
            lifecycleState_.markConnecting();
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
    if (self->lifecycleQueue_ == nullptr) {
        return;
    }

    QueuedLifecycleEvent queued;
    const auto event = static_cast<esp_mqtt_event_handle_t>(eventData);
    switch (eventId) {
        case MQTT_EVENT_CONNECTED:
            queued.type = MqttLifecycleEventType::Connected;
            break;
        case MQTT_EVENT_DISCONNECTED:
            queued.type = MqttLifecycleEventType::Disconnected;
            break;
        case MQTT_EVENT_ERROR:
            queued.type = MqttLifecycleEventType::Error;
            if (event != nullptr && event->error_handle != nullptr) {
                queued.errorType = event->error_handle->error_type;
                queued.connectReturnCode =
                    event->error_handle->connect_return_code;
            }
            break;
        default:
            return;
    }

    xQueueSend(self->lifecycleQueue_, &queued, portMAX_DELAY);
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
