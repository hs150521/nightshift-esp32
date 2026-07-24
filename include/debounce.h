// debounce.h - GPIO sampling and debounce with asymmetric timing
#pragma once

#include <cstdint>

// Individual pin debounce state
struct PinState {
    bool raw;           // Latest sampled value
    bool stable;        // Debounced stable value
    uint32_t last_change_ms;  // When raw last changed
};

// Aggregated pressure state snapshot
struct PressureState {
    // Individual GPIO stable states
    bool gpio4;  // cushion left
    bool gpio5;  // cushion right
    bool gpio6;  // footrest left
    bool gpio7;  // footrest right

    // Logical groups
    bool cushion;   // gpio4 OR gpio5
    bool footrest;  // gpio6 OR gpio7
    bool presence;  // cushion OR footrest
};

class Debouncer {
public:
    // Initialize GPIO pins with internal pull-down
    void begin();

    // Sample all pins and update debounce state.
    // Call this every ~10 ms.
    // Returns true if any stable state changed.
    bool update(uint32_t now_ms);

    // Get current stable pressure state
    PressureState getState() const;

    // Get raw (un-debounced) state of a pin by index (0-3)
    bool getRaw(int index) const;

    // Check if any stable state changed since last call to update()
    bool hasChanged() const { return _changed; }

private:
    PinState _pins[4] = {};
    bool _changed = false;

    void computeGroups(PressureState& state) const;
};
