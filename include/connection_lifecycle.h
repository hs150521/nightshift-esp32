#pragma once

#include "publish_scheduler.h"

#include <cstdint>

enum class MqttLifecycleEventType : uint8_t {
    Connected,
    Disconnected,
    Error
};

struct MqttLifecycleTransition {
    bool becameConnected = false;
    bool becameDisconnected = false;
    bool scheduleReconnect = false;
    bool resetBackoff = false;
};

class MqttLifecycleState {
public:
    void markConnecting() { connecting_ = true; }
    MqttLifecycleTransition apply(MqttLifecycleEventType event);

    bool isConnected() const { return connected_; }
    bool isConnecting() const { return connecting_; }
    uint32_t reconnectCount() const { return reconnectCount_; }
    uint32_t connectionGeneration() const { return connectionGeneration_; }

private:
    bool connected_ = false;
    bool connecting_ = false;
    bool everConnected_ = false;
    uint32_t reconnectCount_ = 0;
    uint32_t connectionGeneration_ = 0;
};

enum class EffectiveConnectionEdge : uint8_t {
    None,
    Connected,
    Disconnected,
    Reconnected
};

class EffectiveConnectivityCoordinator {
public:
    EffectiveConnectionEdge update(
        bool wifiConnected,
        bool mqttTransportConnected,
        uint32_t mqttConnectionGeneration,
        uint32_t nowMs,
        PublishScheduler& scheduler
    );

    bool isConnected() const { return effectiveConnected_; }

private:
    bool effectiveConnected_ = false;
    uint32_t observedMqttGeneration_ = 0;
};
