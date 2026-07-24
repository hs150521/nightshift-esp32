#pragma once

#include <cstdint>

class PublishScheduler {
public:
    void onConnected(uint32_t nowMs);
    void onDisconnected();
    void onStateChanged(uint32_t nowMs);

    bool availabilityDue(uint32_t nowMs) const;
    bool stateDue(uint32_t nowMs) const;
    bool telemetryDue(uint32_t nowMs) const;

    void availabilityAttempted(uint32_t nowMs, bool success);
    void stateAttempted(uint32_t nowMs, bool success);
    void telemetryAttempted(uint32_t nowMs, bool success);

private:
    bool connected_ = false;
    bool availabilityPending_ = false;
    bool statePending_ = false;
    bool statePublished_ = false;
    uint32_t availabilityDueAtMs_ = 0;
    uint32_t stateDueAtMs_ = 0;
    uint32_t lastStatePublishedAtMs_ = 0;
    uint32_t telemetryDueAtMs_ = 30000;

    static bool reached(uint32_t nowMs, uint32_t dueAtMs);
};
