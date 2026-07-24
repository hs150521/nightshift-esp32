#include "debounce.h"
#include "config.h"

PressureState makePressureState(const GpioSnapshot& gpio) {
    PressureState state;
    state.gpio4 = gpio[0];
    state.gpio5 = gpio[1];
    state.gpio6 = gpio[2];
    state.gpio7 = gpio[3];
    state.cushion = state.gpio4 || state.gpio5;
    state.footrest = state.gpio6 || state.gpio7;
    state.presence = state.cushion || state.footrest;
    return state;
}

void Debouncer::reset(const GpioSnapshot& initialRaw, uint32_t nowMs) {
    changed_ = false;
    for (size_t i = 0; i < pins_.size(); ++i) {
        pins_[i].raw = initialRaw[i];
        pins_[i].stable = false;
        pins_[i].rawChangedAtMs = nowMs;
    }
}

bool Debouncer::update(const GpioSnapshot& raw, uint32_t nowMs) {
    changed_ = false;

    for (size_t i = 0; i < pins_.size(); ++i) {
        PinState& pin = pins_[i];
        if (raw[i] != pin.raw) {
            pin.raw = raw[i];
            pin.rawChangedAtMs = nowMs;
        }

        if (pin.raw == pin.stable) {
            continue;
        }

        const uint32_t threshold = pin.raw
            ? nightshift::DEBOUNCE_PRESS_MS
            : nightshift::DEBOUNCE_RELEASE_MS;
        if (nowMs - pin.rawChangedAtMs >= threshold) {
            pin.stable = pin.raw;
            changed_ = true;
        }
    }

    return changed_;
}

PressureState Debouncer::getState() const {
    return makePressureState({
        pins_[0].stable, pins_[1].stable, pins_[2].stable, pins_[3].stable
    });
}

GpioSnapshot Debouncer::getRaw() const {
    return {pins_[0].raw, pins_[1].raw, pins_[2].raw, pins_[3].raw};
}
