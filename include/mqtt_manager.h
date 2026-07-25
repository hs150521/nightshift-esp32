#pragma once

#include "connection_lifecycle.h"

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mqtt_client.h>

class MqttManager {
public:
    bool begin(const char* clientId, const char* lwtPayload);
    void setNetworkReady(bool ready) { networkReady_ = ready; }
    void update(uint32_t nowMs);

    bool publishAvailability(const char* payload);
    bool publishState(const char* payload);
    bool publishTelemetry(const char* payload);

    bool isConnected() const {
        return networkReady_ && lifecycleState_.isConnected();
    }
    bool isTransportConnected() const {
        return lifecycleState_.isConnected();
    }
    uint32_t getConnectionGeneration() const {
        return lifecycleState_.connectionGeneration();
    }
    uint32_t getReconnectCount() const {
        return lifecycleState_.reconnectCount();
    }
    uint32_t getPublishCount() const { return publishCount_; }
    int getOutboxSize() const;

private:
    struct QueuedLifecycleEvent {
        MqttLifecycleEventType type = MqttLifecycleEventType::Error;
        int errorType = 0;
        int connectReturnCode = 0;
    };

    esp_mqtt_client_handle_t client_ = nullptr;
    QueueHandle_t lifecycleQueue_ = nullptr;
    MqttLifecycleState lifecycleState_;
    bool networkReady_ = false;
    bool started_ = false;
    uint32_t publishCount_ = 0;
    uint32_t backoffMs_ = 1000;
    uint32_t nextAttemptAtMs_ = 0;

    static void eventHandler(
        void* handlerArgs,
        esp_event_base_t eventBase,
        int32_t eventId,
        void* eventData
    );
    bool enqueue(const char* topic, const char* payload, int qos, bool retain);
    void scheduleRetry(uint32_t nowMs);
    static bool reached(uint32_t nowMs, uint32_t dueAtMs);
};
