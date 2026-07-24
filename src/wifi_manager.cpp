#include "wifi_manager.h"

#include "config.h"
#include "secrets.h"

#include <algorithm>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    WiFi.disconnect(false, false);
    startConnection(millis());
}

bool WiFiManager::reached(uint32_t nowMs, uint32_t dueAtMs) {
    return static_cast<int32_t>(nowMs - dueAtMs) >= 0;
}

bool WiFiManager::update(uint32_t nowMs) {
    const wl_status_t link = WiFi.status();

    if (link == WL_CONNECTED) {
        if (status_ != WiFiStatus::CONNECTED) {
            status_ = WiFiStatus::CONNECTED;
            backoffMs_ = nightshift::WIFI_RECONNECT_MIN_MS;
            const IPAddress ip = WiFi.localIP();
            snprintf(ip_, sizeof(ip_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("[WiFi] connected ip=%s rssi=%d dBm\n", ip_, WiFi.RSSI());
            return true;
        }
        return false;
    }

    if (status_ == WiFiStatus::CONNECTED) {
        status_ = WiFiStatus::DISCONNECTED;
        Serial.printf("[WiFi] disconnected status=%d\n", static_cast<int>(link));
        scheduleRetry(nowMs);
        return true;
    }

    if (status_ == WiFiStatus::CONNECTING) {
        const bool terminalFailure =
            link == WL_CONNECT_FAILED || link == WL_NO_SSID_AVAIL;
        const bool timedOut =
            nowMs - attemptStartedAtMs_ >= nightshift::WIFI_CONNECT_TIMEOUT_MS;
        if (terminalFailure || timedOut) {
            WiFi.disconnect(false, false);
            status_ = WiFiStatus::DISCONNECTED;
            Serial.printf("[WiFi] attempt failed status=%d\n", static_cast<int>(link));
            scheduleRetry(nowMs);
        }
        return false;
    }

    if (reached(nowMs, nextAttemptAtMs_)) {
        startConnection(nowMs);
    }

    return false;
}

int WiFiManager::getRSSI() const {
    return isConnected() ? WiFi.RSSI() : 0;
}

const char* WiFiManager::getIP() const {
    return ip_;
}

void WiFiManager::startConnection(uint32_t nowMs) {
    Serial.printf("[WiFi] connecting ssid=%s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    status_ = WiFiStatus::CONNECTING;
    attemptStartedAtMs_ = nowMs;
}

void WiFiManager::scheduleRetry(uint32_t nowMs) {
    const uint32_t jitterWindow = backoffMs_ / 4U;
    const uint32_t jitter = jitterWindow == 0
        ? 0
        : esp_random() % (jitterWindow + 1U);
    nextAttemptAtMs_ = nowMs + backoffMs_ + jitter;
    Serial.printf("[WiFi] retry in %lu ms\n",
        static_cast<unsigned long>(backoffMs_ + jitter));
    backoffMs_ = std::min(
        backoffMs_ * 2U, nightshift::WIFI_RECONNECT_MAX_MS
    );
}
