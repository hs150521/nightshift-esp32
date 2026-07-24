// mqtt_manager.cpp - MQTT lifecycle implementation
#include "mqtt_manager.h"
#include "config.h"
#include "secrets.h"
#include <Arduino.h>

// Static LWT payload buffer (must persist for PubSubClient)
static char _lwtPayload[128];

void MqttManager::begin(const char* bootId) {
    _bootId = bootId;

    // Build client ID: "pressure-01-<bootId>"
    snprintf(_clientId, sizeof(_clientId), "%s%s", MQTT_CLIENT_ID_PREFIX, _bootId);

    _mqttClient.setClient(_wifiClient);
    _mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    _mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
    _mqttClient.setBufferSize(512);  // Ensure payloads fit

    Serial.printf("[MQTT] Client ID: %s\n", _clientId);
    Serial.printf("[MQTT] Broker: %s:%d\n", MQTT_HOST, MQTT_PORT);
}

bool MqttManager::update(uint32_t now_ms) {
    if (!_networkReady) return false;

    bool stateChanged = false;

    if (_mqttClient.connected()) {
        _mqttClient.loop();
        if (!_wasConnected) {
            _wasConnected = true;
            stateChanged = true;
        }
    } else {
        if (_wasConnected) {
            _wasConnected = false;
            Serial.println("[MQTT] Disconnected from broker");
            stateChanged = true;
        }

        // Attempt reconnect with backoff
        if (now_ms - _lastConnectAttempt_ms >= _backoff_ms) {
            if (connect()) {
                resetBackoff();
                stateChanged = true;
                Serial.println("[MQTT] Connected to broker");
            } else {
                increaseBackoff();
                Serial.printf("[MQTT] Connect failed (rc=%d), retry in %lu ms\n",
                    _mqttClient.state(), _backoff_ms);
            }
            _lastConnectAttempt_ms = now_ms;
        }
    }

    return stateChanged;
}

bool MqttManager::connect() {
    configureLWT();

    bool result = _mqttClient.connect(
        _clientId,
        MQTT_USERNAME,
        MQTT_PASSWORD,
        MQTT_TOPIC_AVAILABILITY,  // LWT topic
        1,                         // LWT QoS
        true,                      // LWT retain
        _lwtPayload               // LWT payload
    );

    if (result) {
        _reconnectCount++;
    }
    return result;
}

bool MqttManager::publishAvailability(const char* payload) {
    if (!_mqttClient.connected()) return false;
    bool ok = _mqttClient.publish(MQTT_TOPIC_AVAILABILITY, payload, true);  // QoS 1 retained
    if (ok) _publishCount++;
    else Serial.println("[MQTT] Availability publish failed");
    return ok;
}

bool MqttManager::publishState(const char* payload) {
    if (!_mqttClient.connected()) return false;
    bool ok = _mqttClient.publish(MQTT_TOPIC_STATE, payload, true);  // QoS 1 retained
    if (ok) _publishCount++;
    else Serial.println("[MQTT] State publish failed");
    return ok;
}

bool MqttManager::publishTelemetry(const char* payload) {
    if (!_mqttClient.connected()) return false;
    bool ok = _mqttClient.publish(MQTT_TOPIC_TELEMETRY, payload, false);  // QoS 0 not retained
    if (ok) _publishCount++;
    else Serial.println("[MQTT] Telemetry publish failed");
    return ok;
}

bool MqttManager::isConnected() {
    return _mqttClient.connected();
}

void MqttManager::configureLWT() {
    // LWT payload: offline marker
    snprintf(_lwtPayload, sizeof(_lwtPayload),
        "{\"schema\":\"nightshift.sensor-availability.v1\","
        "\"device_id\":\"" DEVICE_ID "\","
        "\"online\":false}");
}

void MqttManager::increaseBackoff() {
    _backoff_ms = min(_backoff_ms * 2, (uint32_t)30000);
}

void MqttManager::resetBackoff() {
    _backoff_ms = 1000;
}
