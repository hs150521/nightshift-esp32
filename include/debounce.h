#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

using GpioSnapshot = std::array<bool, 4>;

struct PinState {
    bool raw = false;
    bool stable = false;
    uint32_t rawChangedAtMs = 0;
};

struct PressureState {
    bool gpio4 = false;
    bool gpio5 = false;
    bool gpio6 = false;
    bool gpio7 = false;
    bool cushion = false;
    bool footrest = false;
    bool presence = false;
};

PressureState makePressureState(const GpioSnapshot& gpio);

class Debouncer {
public:
    void reset(const GpioSnapshot& initialRaw = {}, uint32_t nowMs = 0);
    bool update(const GpioSnapshot& raw, uint32_t nowMs);
    PressureState getState() const;
    GpioSnapshot getRaw() const;
    bool hasChanged() const { return changed_; }

private:
    std::array<PinState, 4> pins_{};
    bool changed_ = false;
};
