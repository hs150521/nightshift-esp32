// test_debounce.cpp - Unit tests for debounce logic and payload serialization
// Runs on native platform (host) using Unity test framework
//
// We mock Arduino APIs to test logic in isolation.

#include <unity.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

// ============================================================
// Mock Arduino environment for native testing
// ============================================================
#define HIGH 1
#define LOW 0
#define INPUT_PULLDOWN 0x09

// Mock pin states (4 pins: GPIO4=index0, GPIO5=index1, GPIO6=index2, GPIO7=index3)
static int mock_pin_values[8] = {0};  // indexed by GPIO number

void pinMode(int pin, int mode) { (void)pin; (void)mode; }
int digitalRead(int pin) { return mock_pin_values[pin]; }

// Provide min template used in config.h / debounce
template<typename T> T min(T a, T b) { return a < b ? a : b; }

// ============================================================
// Include the actual implementation (header-only for testing)
// ============================================================

// Replicate config constants for test
constexpr int PIN_CUSHION_LEFT  = 4;
constexpr int PIN_CUSHION_RIGHT = 5;
constexpr int PIN_FOOTREST_LEFT  = 6;
constexpr int PIN_FOOTREST_RIGHT = 7;
constexpr int SENSOR_PINS[] = {4, 5, 6, 7};
constexpr int NUM_SENSORS = 4;
constexpr uint32_t GPIO_SAMPLE_INTERVAL_MS = 10;
constexpr uint32_t DEBOUNCE_PRESS_MS = 30;
constexpr uint32_t DEBOUNCE_RELEASE_MS = 100;

// Include debounce header
#include "debounce.h"

// Inline the debounce implementation for native testing
// (Avoids Arduino.h dependency issues)
void Debouncer::begin() {
    for (int i = 0; i < NUM_SENSORS; i++) {
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
        if (current != _pins[i].raw) {
            _pins[i].raw = current;
            _pins[i].last_change_ms = now_ms;
        }
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

// ============================================================
// Helper: set all mock pins
// ============================================================
static void setMockPins(bool g4, bool g5, bool g6, bool g7) {
    mock_pin_values[4] = g4 ? HIGH : LOW;
    mock_pin_values[5] = g5 ? HIGH : LOW;
    mock_pin_values[6] = g6 ? HIGH : LOW;
    mock_pin_values[7] = g7 ? HIGH : LOW;
}

static Debouncer deb;

void setUp(void) {
    setMockPins(false, false, false, false);
    deb.begin();
}

void tearDown(void) {}

// ============================================================
// Test: All four low = no activation
// ============================================================
void test_all_low(void) {
    setMockPins(false, false, false, false);
    deb.update(0);
    deb.update(200);  // Well past any debounce
    PressureState s = deb.getState();
    TEST_ASSERT_FALSE(s.gpio4);
    TEST_ASSERT_FALSE(s.gpio5);
    TEST_ASSERT_FALSE(s.gpio6);
    TEST_ASSERT_FALSE(s.gpio7);
    TEST_ASSERT_FALSE(s.cushion);
    TEST_ASSERT_FALSE(s.footrest);
    TEST_ASSERT_FALSE(s.presence);
}

// ============================================================
// Test: GPIO4 only -> cushion triggered
// ============================================================
void test_gpio4_only_cushion(void) {
    setMockPins(true, false, false, false);
    deb.update(0);
    // Not yet stable at 20ms
    deb.update(20);
    PressureState s = deb.getState();
    TEST_ASSERT_FALSE(s.cushion);  // Still debouncing

    // Stable after 30ms
    deb.update(30);
    s = deb.getState();
    TEST_ASSERT_TRUE(s.gpio4);
    TEST_ASSERT_FALSE(s.gpio5);
    TEST_ASSERT_TRUE(s.cushion);
    TEST_ASSERT_FALSE(s.footrest);
    TEST_ASSERT_TRUE(s.presence);
}

// ============================================================
// Test: GPIO5 only -> cushion triggered
// ============================================================
void test_gpio5_only_cushion(void) {
    setMockPins(false, true, false, false);
    deb.update(0);
    deb.update(30);
    PressureState s = deb.getState();
    TEST_ASSERT_TRUE(s.gpio5);
    TEST_ASSERT_TRUE(s.cushion);
    TEST_ASSERT_TRUE(s.presence);
}

// ============================================================
// Test: Both cushion sensors
// ============================================================
void test_both_cushion(void) {
    setMockPins(true, true, false, false);
    deb.update(0);
    deb.update(30);
    PressureState s = deb.getState();
    TEST_ASSERT_TRUE(s.gpio4);
    TEST_ASSERT_TRUE(s.gpio5);
    TEST_ASSERT_TRUE(s.cushion);
    TEST_ASSERT_FALSE(s.footrest);
    TEST_ASSERT_TRUE(s.presence);
}

// ============================================================
// Test: GPIO6 only -> footrest triggered
// ============================================================
void test_gpio6_only_footrest(void) {
    setMockPins(false, false, true, false);
    deb.update(0);
    deb.update(30);
    PressureState s = deb.getState();
    TEST_ASSERT_TRUE(s.gpio6);
    TEST_ASSERT_FALSE(s.gpio7);
    TEST_ASSERT_FALSE(s.cushion);
    TEST_ASSERT_TRUE(s.footrest);
    TEST_ASSERT_TRUE(s.presence);
}

// ============================================================
// Test: GPIO7 only -> footrest triggered
// ============================================================
void test_gpio7_only_footrest(void) {
    setMockPins(false, false, false, true);
    deb.update(0);
    deb.update(30);
    PressureState s = deb.getState();
    TEST_ASSERT_TRUE(s.gpio7);
    TEST_ASSERT_TRUE(s.footrest);
    TEST_ASSERT_TRUE(s.presence);
}

// ============================================================
// Test: Both footrest sensors
// ============================================================
void test_both_footrest(void) {
    setMockPins(false, false, true, true);
    deb.update(0);
    deb.update(30);
    PressureState s = deb.getState();
    TEST_ASSERT_TRUE(s.gpio6);
    TEST_ASSERT_TRUE(s.gpio7);
    TEST_ASSERT_TRUE(s.footrest);
}

// ============================================================
// Test: Press debounce - must wait 30ms
// ============================================================
void test_press_debounce(void) {
    setMockPins(true, false, false, false);
    deb.update(0);

    // At 10ms, still not stable
    deb.update(10);
    TEST_ASSERT_FALSE(deb.getState().gpio4);

    // At 29ms, still not stable
    deb.update(29);
    TEST_ASSERT_FALSE(deb.getState().gpio4);

    // At 30ms, becomes stable
    deb.update(30);
    TEST_ASSERT_TRUE(deb.getState().gpio4);
}

// ============================================================
// Test: Release debounce - must wait 100ms
// ============================================================
void test_release_debounce(void) {
    // First press the pin
    setMockPins(true, false, false, false);
    deb.update(0);
    deb.update(30);
    TEST_ASSERT_TRUE(deb.getState().gpio4);

    // Now release
    setMockPins(false, false, false, false);
    deb.update(31);

    // At 80ms after release (111ms total), not yet stable
    deb.update(111);
    TEST_ASSERT_TRUE(deb.getState().gpio4);  // Still high (within 100ms)

    // At 100ms after release (131ms total), stable low
    deb.update(131);
    TEST_ASSERT_FALSE(deb.getState().gpio4);
}

// ============================================================
// Test: Rapid bounce pattern - should not trigger
// ============================================================
void test_rapid_bounce(void) {
    // Simulate bouncing contact: alternating every 5ms
    for (uint32_t t = 0; t < 60; t += 10) {
        setMockPins((t / 5) % 2 == 0, false, false, false);
        deb.update(t);
    }
    // Should still be in initial state (false) because never stable for 30ms
    TEST_ASSERT_FALSE(deb.getState().gpio4);
}

// ============================================================
// Test: Group state consistency
// ============================================================
void test_group_consistency(void) {
    // All high
    setMockPins(true, true, true, true);
    deb.update(0);
    deb.update(30);
    PressureState s = deb.getState();
    TEST_ASSERT_TRUE(s.cushion);
    TEST_ASSERT_TRUE(s.footrest);
    TEST_ASSERT_TRUE(s.presence);
    TEST_ASSERT_EQUAL(s.presence, s.cushion || s.footrest);
}

// ============================================================
// Test: hasChanged flag
// ============================================================
void test_has_changed_flag(void) {
    setMockPins(false, false, false, false);
    deb.update(0);
    TEST_ASSERT_FALSE(deb.hasChanged());

    setMockPins(true, false, false, false);
    deb.update(100);
    // Raw changed but not yet stable
    TEST_ASSERT_FALSE(deb.hasChanged());

    deb.update(130);  // 30ms later -> stable
    TEST_ASSERT_TRUE(deb.hasChanged());

    // Next update with no change
    deb.update(140);
    TEST_ASSERT_FALSE(deb.hasChanged());
}

// ============================================================
// Main
// ============================================================
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_all_low);
    RUN_TEST(test_gpio4_only_cushion);
    RUN_TEST(test_gpio5_only_cushion);
    RUN_TEST(test_both_cushion);
    RUN_TEST(test_gpio6_only_footrest);
    RUN_TEST(test_gpio7_only_footrest);
    RUN_TEST(test_both_footrest);
    RUN_TEST(test_press_debounce);
    RUN_TEST(test_release_debounce);
    RUN_TEST(test_rapid_bounce);
    RUN_TEST(test_group_consistency);
    RUN_TEST(test_has_changed_flag);

    return UNITY_END();
}
