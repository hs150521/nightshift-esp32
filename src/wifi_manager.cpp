// wifi_manager.cpp - Wi-Fi lifecycle implementation
#include "wifi_manager.h"
#include "config.h"
#include "secrets.h"
#include <WiFi.h>
#include <Arduino.h>

static char _ipBuf[16] = "0.0.0.0";

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnect ourselves
    WiFi.disconnect(true);
    delay(100);

    Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
    startConnection();
}

bool WiFiManager::update(uint32_t now_ms) {
    bool stateChanged = false;

    if (WiFi.status() == WL_CONNECTED) {
        if (_status != WiFiStatus::CONNECTED) {
            _status = WiFiStatus::CONNECTED;
            resetBackoff();

            // Cache IP string
            IPAddress ip = WiFi.localIP();
            snprintf(_ipBuf, sizeof(_ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

            Serial.printf("[WiFi] Connected! IP: %s  RSSI: %d dBm\n", _ipBuf, WiFi.RSSI());
            stateChanged = true;
            _wasConnected = true;
        }
    } else {
        if (_status == WiFiStatus::CONNECTED) {
            // Lost connection
            _status = WiFiStatus::DISCONNECTED;
            Serial.println("[WiFi] Connection lost");
            stateChanged = true;
        }

        if (_status == WiFiStatus::DISCONNECTED) {
            // Attempt reconnect with backoff
            if (now_ms - _lastAttempt_ms >= jitteredBackoff()) {
                Serial.printf("[WiFi] Reconnecting (backoff: %lu ms)...\n", _backoff_ms);
                startConnection();
                increaseBackoff();
            }
        }
    }

    return stateChanged;
}

int WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

const char* WiFiManager::getIP() const {
    return _ipBuf;
}

void WiFiManager::startConnection() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    _status = WiFiStatus::CONNECTING;
    _lastAttempt_ms = millis();
}

void WiFiManager::resetBackoff() {
    _backoff_ms = WIFI_RECONNECT_MIN_MS;
}

void WiFiManager::increaseBackoff() {
    _backoff_ms = min(_backoff_ms * 2, WIFI_RECONNECT_MAX_MS);
}

uint32_t WiFiManager::jitteredBackoff() const {
    // Add up to 25% jitter
    uint32_t jitter = random(0, _backoff_ms / 4);
    return _backoff_ms + jitter;
}
