#pragma once

#include <cstdint>

enum class WiFiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
};

class WiFiManager {
public:
    void begin();
    bool update(uint32_t nowMs);
    WiFiStatus getStatus() const { return status_; }
    bool isConnected() const { return status_ == WiFiStatus::CONNECTED; }
    int getRSSI() const;
    const char* getIP() const;

private:
    WiFiStatus status_ = WiFiStatus::DISCONNECTED;
    uint32_t attemptStartedAtMs_ = 0;
    uint32_t nextAttemptAtMs_ = 0;
    uint32_t backoffMs_ = 1000;
    char ip_[16] = "0.0.0.0";

    void startConnection(uint32_t nowMs);
    void scheduleRetry(uint32_t nowMs);
    static bool reached(uint32_t nowMs, uint32_t dueAtMs);
};
