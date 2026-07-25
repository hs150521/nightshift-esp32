#include "connection_lifecycle.h"

MqttLifecycleTransition MqttLifecycleState::apply(
    MqttLifecycleEventType event
) {
    MqttLifecycleTransition transition;

    switch (event) {
        case MqttLifecycleEventType::Connected:
            connecting_ = false;
            if (!connected_) {
                connected_ = true;
                transition.becameConnected = true;
                transition.resetBackoff = true;
                ++connectionGeneration_;
                if (everConnected_) {
                    ++reconnectCount_;
                }
                everConnected_ = true;
            }
            break;

        case MqttLifecycleEventType::Disconnected:
            connecting_ = false;
            transition.becameDisconnected = connected_;
            connected_ = false;
            transition.scheduleReconnect = true;
            break;

        case MqttLifecycleEventType::Error:
            break;
    }

    return transition;
}

EffectiveConnectionEdge EffectiveConnectivityCoordinator::update(
    bool wifiConnected,
    bool mqttTransportConnected,
    uint32_t mqttConnectionGeneration,
    uint32_t nowMs,
    PublishScheduler& scheduler
) {
    const bool currentlyEffective =
        wifiConnected && mqttTransportConnected;
    const bool recoveredWithinOneUpdate =
        effectiveConnected_ &&
        currentlyEffective &&
        mqttConnectionGeneration != observedMqttGeneration_;

    observedMqttGeneration_ = mqttConnectionGeneration;

    if (recoveredWithinOneUpdate) {
        scheduler.onDisconnected();
        scheduler.onConnected(nowMs);
        return EffectiveConnectionEdge::Reconnected;
    }

    if (currentlyEffective == effectiveConnected_) {
        return EffectiveConnectionEdge::None;
    }

    effectiveConnected_ = currentlyEffective;
    if (effectiveConnected_) {
        scheduler.onConnected(nowMs);
        return EffectiveConnectionEdge::Connected;
    }

    scheduler.onDisconnected();
    return EffectiveConnectionEdge::Disconnected;
}
