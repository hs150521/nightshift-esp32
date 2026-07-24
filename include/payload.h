// payload.h - JSON payload serialization
#pragma once

#include "debounce.h"
#include <cstdint>
#include <cstddef>

// Buffer size for JSON payloads
constexpr size_t PAYLOAD_BUF_SIZE = 384;

class PayloadBuilder {
public:
    PayloadBuilder(const char* bootId, uint32_t startedAtMs);

    // Build availability online payload
    void buildAvailabilityOnline(char* buf, size_t bufSize);

    // Build state payload from current pressure state
    // Increments sequence counter automatically
    void buildState(char* buf, size_t bufSize, const PressureState& state, uint32_t sampledAtMs);

    // Build telemetry payload
    void buildTelemetry(char* buf, size_t bufSize,
                        uint32_t uptimeMs, int wifiRssi,
                        uint32_t mqttReconnects, uint32_t publishCount,
                        uint32_t reportedAtMs);

    // Get current sequence number
    uint32_t getSeq() const { return _seq; }

private:
    const char* _bootId;
    uint32_t _startedAtMs;
    uint32_t _seq = 0;
};
