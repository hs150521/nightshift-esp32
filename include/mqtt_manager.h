// mqtt_manager.h - MQTT lifecycle with LWT and reconnect
#pragma once

#include <cstdint>
#include <PubSubClient.h>
#include <WiFiClient.h>

class MqttManager {
public:
    // Initialize MQTT client with broker settings
    void begin(const char* bootId);

    // Call periodically to maintain connection
    // Returns true if connection state changed
    bool update(uint32_t now_ms);

    // Publish methods (return true on success)
    bool publishAvailability(const char* payload);
    bool publishState(const char* payload);
    bool publishTelemetry(const char* payload);

    // Connection status
    bool isConnected();

    // Reconnect counter
    uint32_t getReconnectCount() const { return _reconnectCount; }
    uint32_t getPublishCount() const { return _publishCount; }

    // Must be called when Wi-Fi becomes available
    void setNetworkReady(bool ready) { _networkReady = ready; }

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    char _clientId[32] = {};
    const char* _bootId = nullptr;
    bool _networkReady = false;
    bool _wasConnected = false;

    uint32_t _lastConnectAttempt_ms = 0;
    uint32_t _backoff_ms = 1000;
    uint32_t _reconnectCount = 0;
    uint32_t _publishCount = 0;

    bool connect();
    void increaseBackoff();
    void resetBackoff();

    // Build and set LWT before connecting
    void configureLWT();
};
