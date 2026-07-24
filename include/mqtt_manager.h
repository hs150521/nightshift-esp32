#pragma once

#include <cstdint>
#include <mqtt_client.h>

class MqttManager {
public:
    bool begin(const char* clientId, const char* lwtPayload);
    void setNetworkReady(bool ready) { networkReady_ = ready; }
    void update(uint32_t nowMs);

    bool publishAvailability(const char* payload);
    bool publishState(const char* payload);
    bool publishTelemetry(const char* payload);

    bool isConnected() const { return connected_ && networkReady_; }
    bool takeConnectedEvent();
    bool takeDisconnectedEvent();
    uint32_t getReconnectCount() const { return reconnectCount_; }
    uint32_t getPublishCount() const { return publishCount_; }
    int getOutboxSize() const;

private:
    esp_mqtt_client_handle_t client_ = nullptr;
    volatile bool connected_ = false;
    volatile bool connecting_ = false;
    volatile bool connectedEvent_ = false;
    volatile bool disconnectedEvent_ = false;
    bool connectedEdge_ = false;
    bool disconnectedEdge_ = false;
    bool networkReady_ = false;
    bool started_ = false;
    bool everConnected_ = false;
    uint32_t reconnectCount_ = 0;
    uint32_t publishCount_ = 0;
    uint32_t backoffMs_ = 1000;
    uint32_t nextAttemptAtMs_ = 0;

    static void eventHandler(
        void* handlerArgs,
        esp_event_base_t eventBase,
        int32_t eventId,
        void* eventData
    );
    void handleEvent(int32_t eventId, esp_mqtt_event_handle_t event);
    bool enqueue(const char* topic, const char* payload, int qos, bool retain);
    void scheduleRetry(uint32_t nowMs);
    static bool reached(uint32_t nowMs, uint32_t dueAtMs);
};
