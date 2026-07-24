// wifi_manager.h - Wi-Fi lifecycle with exponential backoff reconnect
#pragma once

#include <cstdint>

enum class WiFiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

class WiFiManager {
public:
    // Initialize Wi-Fi in STA mode and begin connection
    void begin();

    // Call periodically to handle reconnection logic
    // Returns true if connection state changed (connected or disconnected)
    bool update(uint32_t now_ms);

    // Current status
    WiFiStatus getStatus() const { return _status; }
    bool isConnected() const { return _status == WiFiStatus::CONNECTED; }

    // Get RSSI (only valid when connected)
    int getRSSI() const;

    // Get IP address as string (only valid when connected)
    const char* getIP() const;

private:
    WiFiStatus _status = WiFiStatus::DISCONNECTED;
    uint32_t _lastAttempt_ms = 0;
    uint32_t _backoff_ms = 1000;
    bool _wasConnected = false;

    void startConnection();
    void resetBackoff();
    void increaseBackoff();
    uint32_t jitteredBackoff() const;
};
