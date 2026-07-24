#pragma once

#include "debounce.h"

#include <cstddef>
#include <cstdint>

constexpr size_t PAYLOAD_BUF_SIZE = 512;

class PayloadBuilder {
public:
    explicit PayloadBuilder(const char* bootId);

    bool buildAvailabilityOnline(char* buf, size_t bufSize) const;
    static bool buildAvailabilityOffline(char* buf, size_t bufSize);
    bool buildState(
        char* buf,
        size_t bufSize,
        const PressureState& state,
        uint32_t sampledAtMs
    );
    bool buildTelemetry(
        char* buf,
        size_t bufSize,
        uint32_t uptimeMs,
        int wifiRssi,
        uint32_t mqttReconnects,
        uint32_t publishCount,
        uint32_t reportedAtMs
    ) const;

    uint32_t getSeq() const { return seq_; }

private:
    const char* bootId_;
    uint32_t seq_ = 0;
};
