// debounce.cpp - GPIO sampling and asymmetric debounce implementation
#include "debounce.h"
#include "config.h"
#include <Arduino.h>

void Debouncer::begin() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        // Active-high inputs with internal pull-down
        pinMode(SENSOR_PINS[i], INPUT_PULLDOWN);
        _pins[i].raw = false;
        _pins[i].stable = false;
        _pins[i].last_change_ms = 0;
    }
}

bool Debouncer::update(uint32_t now_ms) {
    _changed = false;

    for (int i = 0; i < NUM_SENSORS; i++) {
        bool current = digitalRead(SENSOR_PINS[i]) == HIGH;

        // Detect raw state change
        if (current != _pins[i].raw) {
            _pins[i].raw = current;
            _pins[i].last_change_ms = now_ms;
        }

        // Apply asymmetric debounce
        if (_pins[i].raw != _pins[i].stable) {
            uint32_t elapsed = now_ms - _pins[i].last_change_ms;
            uint32_t threshold = _pins[i].raw ? DEBOUNCE_PRESS_MS : DEBOUNCE_RELEASE_MS;

            if (elapsed >= threshold) {
                _pins[i].stable = _pins[i].raw;
                _changed = true;
            }
        }
    }

    return _changed;
}

PressureState Debouncer::getState() const {
    PressureState state;
    state.gpio4 = _pins[0].stable;
    state.gpio5 = _pins[1].stable;
    state.gpio6 = _pins[2].stable;
    state.gpio7 = _pins[3].stable;
    computeGroups(state);
    return state;
}

bool Debouncer::getRaw(int index) const {
    if (index < 0 || index >= NUM_SENSORS) return false;
    return _pins[index].raw;
}

void Debouncer::computeGroups(PressureState& state) const {
    state.cushion  = state.gpio4 || state.gpio5;
    state.footrest = state.gpio6 || state.gpio7;
    state.presence = state.cushion || state.footrest;
}
