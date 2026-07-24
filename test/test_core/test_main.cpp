#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "boot_identity.h"
#include "config.h"
#include "debounce.h"
#include "payload.h"
#include "publish_scheduler.h"

namespace {

Debouncer debouncer;
char json[PAYLOAD_BUF_SIZE];

PressureState settle(const GpioSnapshot& raw, uint32_t startMs = 0) {
    debouncer.reset({}, startMs);
    debouncer.update(raw, startMs);
    debouncer.update(raw, startMs + nightshift::DEBOUNCE_PRESS_MS);
    return debouncer.getState();
}

void assertGroups(
    const PressureState& state,
    bool cushion,
    bool footrest,
    bool presence
) {
    TEST_ASSERT_EQUAL(cushion, state.cushion);
    TEST_ASSERT_EQUAL(footrest, state.footrest);
    TEST_ASSERT_EQUAL(presence, state.presence);
}

void test_all_four_low() {
    const PressureState state = settle({false, false, false, false});
    TEST_ASSERT_FALSE(state.gpio4);
    TEST_ASSERT_FALSE(state.gpio5);
    TEST_ASSERT_FALSE(state.gpio6);
    TEST_ASSERT_FALSE(state.gpio7);
    assertGroups(state, false, false, false);
}

void test_gpio4_only_activates_cushion() {
    const PressureState state = settle({true, false, false, false});
    TEST_ASSERT_TRUE(state.gpio4);
    TEST_ASSERT_FALSE(state.gpio5);
    assertGroups(state, true, false, true);
}

void test_gpio5_only_activates_cushion() {
    const PressureState state = settle({false, true, false, false});
    TEST_ASSERT_FALSE(state.gpio4);
    TEST_ASSERT_TRUE(state.gpio5);
    assertGroups(state, true, false, true);
}

void test_either_and_both_cushion_inputs() {
    assertGroups(settle({true, false, false, false}), true, false, true);
    assertGroups(settle({false, true, false, false}), true, false, true);
    assertGroups(settle({true, true, false, false}), true, false, true);
}

void test_gpio6_only_activates_footrest() {
    const PressureState state = settle({false, false, true, false});
    TEST_ASSERT_TRUE(state.gpio6);
    TEST_ASSERT_FALSE(state.gpio7);
    assertGroups(state, false, true, true);
}

void test_gpio7_only_activates_footrest() {
    const PressureState state = settle({false, false, false, true});
    TEST_ASSERT_FALSE(state.gpio6);
    TEST_ASSERT_TRUE(state.gpio7);
    assertGroups(state, false, true, true);
}

void test_either_and_both_footrest_inputs() {
    assertGroups(settle({false, false, true, false}), false, true, true);
    assertGroups(settle({false, false, false, true}), false, true, true);
    assertGroups(settle({false, false, true, true}), false, true, true);
}

void test_press_debounce_is_30_ms() {
    debouncer.reset({}, 0);
    debouncer.update({true, false, false, false}, 100);
    TEST_ASSERT_FALSE(debouncer.getState().gpio4);
    debouncer.update({true, false, false, false}, 129);
    TEST_ASSERT_FALSE(debouncer.getState().gpio4);
    TEST_ASSERT_TRUE(debouncer.update({true, false, false, false}, 130));
    TEST_ASSERT_TRUE(debouncer.getState().gpio4);
}

void test_release_debounce_is_100_ms() {
    settle({true, false, false, false});
    debouncer.update({false, false, false, false}, 31);
    debouncer.update({false, false, false, false}, 130);
    TEST_ASSERT_TRUE(debouncer.getState().gpio4);
    TEST_ASSERT_TRUE(debouncer.update({false, false, false, false}, 131));
    TEST_ASSERT_FALSE(debouncer.getState().gpio4);
}

void test_rapid_bounce_requires_full_stable_window() {
    debouncer.reset({}, 0);
    debouncer.update({true, false, false, false}, 0);
    debouncer.update({false, false, false, false}, 5);
    debouncer.update({true, false, false, false}, 10);
    debouncer.update({false, false, false, false}, 15);
    debouncer.update({true, false, false, false}, 20);
    debouncer.update({true, false, false, false}, 49);
    TEST_ASSERT_FALSE(debouncer.getState().gpio4);
    debouncer.update({true, false, false, false}, 50);
    TEST_ASSERT_TRUE(debouncer.getState().gpio4);
}

void test_group_state_consistency_for_all_combinations() {
    for (unsigned mask = 0; mask < 16; ++mask) {
        const GpioSnapshot gpio = {
            (mask & 1U) != 0,
            (mask & 2U) != 0,
            (mask & 4U) != 0,
            (mask & 8U) != 0
        };
        const PressureState state = makePressureState(gpio);
        TEST_ASSERT_EQUAL(state.gpio4 || state.gpio5, state.cushion);
        TEST_ASSERT_EQUAL(state.gpio6 || state.gpio7, state.footrest);
        TEST_ASSERT_EQUAL(state.cushion || state.footrest, state.presence);
    }
}

void test_state_json_fields_and_boolean_consistency() {
    PayloadBuilder builder("0123abcd");
    PressureState intentionallyInconsistent;
    intentionallyInconsistent.gpio4 = true;
    intentionallyInconsistent.gpio7 = true;
    TEST_ASSERT_TRUE(builder.buildState(
        json, sizeof(json), intentionallyInconsistent, 1234
    ));

    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_EQUAL_STRING("nightshift.pressure-state.v1", doc["schema"]);
    TEST_ASSERT_EQUAL_STRING("pressure-01", doc["device_id"]);
    TEST_ASSERT_EQUAL_STRING("0123abcd", doc["boot_id"]);
    TEST_ASSERT_EQUAL_UINT32(1, doc["seq"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(1234, doc["sampled_at_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("monotonic_boot_ms", doc["time_base"]);
    TEST_ASSERT_TRUE(doc["gpio"]["4"].as<bool>());
    TEST_ASSERT_FALSE(doc["gpio"]["5"].as<bool>());
    TEST_ASSERT_FALSE(doc["gpio"]["6"].as<bool>());
    TEST_ASSERT_TRUE(doc["gpio"]["7"].as<bool>());
    TEST_ASSERT_TRUE(doc["cushion"].as<bool>());
    TEST_ASSERT_TRUE(doc["footrest"].as<bool>());
    TEST_ASSERT_TRUE(doc["presence"].as<bool>());
}

void test_sequence_increments_monotonically() {
    PayloadBuilder builder("89abcdef");
    const PressureState state = makePressureState({});
    builder.buildState(json, sizeof(json), state, 1);
    TEST_ASSERT_EQUAL_UINT32(1, builder.getSeq());
    builder.buildState(json, sizeof(json), state, 2);
    TEST_ASSERT_EQUAL_UINT32(2, builder.getSeq());
    builder.buildState(json, sizeof(json), state, 3);
    TEST_ASSERT_EQUAL_UINT32(3, builder.getSeq());
}

void test_new_boot_entropy_changes_boot_id() {
    char first[9];
    char second[9];
    formatBootId(0x7f92ab31U, first);
    formatBootId(0x7f92ab32U, second);
    TEST_ASSERT_EQUAL_STRING("7f92ab31", first);
    TEST_ASSERT_NOT_EQUAL(0, std::strcmp(first, second));
}

void test_lwt_payload() {
    TEST_ASSERT_TRUE(PayloadBuilder::buildAvailabilityOffline(
        json, sizeof(json)
    ));
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_EQUAL_STRING("nightshift.sensor-availability.v1", doc["schema"]);
    TEST_ASSERT_EQUAL_STRING("pressure-01", doc["device_id"]);
    TEST_ASSERT_FALSE(doc["online"].as<bool>());
    TEST_ASSERT_TRUE(doc["boot_id"].isNull());
}

void test_periodic_three_second_state_refresh() {
    PublishScheduler scheduler;
    scheduler.onConnected(100);
    TEST_ASSERT_TRUE(scheduler.stateDue(100));
    scheduler.stateAttempted(100, true);
    TEST_ASSERT_FALSE(scheduler.stateDue(3099));
    TEST_ASSERT_TRUE(scheduler.stateDue(3100));
}

void test_stable_change_is_immediately_due() {
    PublishScheduler scheduler;
    scheduler.onConnected(0);
    scheduler.stateAttempted(0, true);
    scheduler.onStateChanged(75);
    TEST_ASSERT_TRUE(scheduler.stateDue(75));
}

void test_reconnect_republishes_availability_and_state() {
    PublishScheduler scheduler;
    scheduler.onConnected(100);
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);
    scheduler.onDisconnected();
    TEST_ASSERT_FALSE(scheduler.availabilityDue(200));
    TEST_ASSERT_FALSE(scheduler.stateDue(200));
    scheduler.onConnected(500);
    TEST_ASSERT_TRUE(scheduler.availabilityDue(500));
    TEST_ASSERT_TRUE(scheduler.stateDue(500));
}

void test_failed_publish_retries_without_busy_loop() {
    PublishScheduler scheduler;
    scheduler.onConnected(100);
    scheduler.stateAttempted(100, false);
    TEST_ASSERT_FALSE(scheduler.stateDue(599));
    TEST_ASSERT_TRUE(scheduler.stateDue(600));
}

void test_failed_telemetry_retries_without_busy_loop() {
    PublishScheduler scheduler;
    scheduler.onConnected(0);
    TEST_ASSERT_TRUE(scheduler.telemetryDue(30000));
    scheduler.telemetryAttempted(30000, false);
    TEST_ASSERT_FALSE(scheduler.telemetryDue(30499));
    TEST_ASSERT_TRUE(scheduler.telemetryDue(30500));
    scheduler.telemetryAttempted(30500, true);
    TEST_ASSERT_FALSE(scheduler.telemetryDue(60499));
    TEST_ASSERT_TRUE(scheduler.telemetryDue(60500));
}

}  // namespace

void setUp() {}
void tearDown() {}

void runAllTests() {
    RUN_TEST(test_all_four_low);
    RUN_TEST(test_gpio4_only_activates_cushion);
    RUN_TEST(test_gpio5_only_activates_cushion);
    RUN_TEST(test_either_and_both_cushion_inputs);
    RUN_TEST(test_gpio6_only_activates_footrest);
    RUN_TEST(test_gpio7_only_activates_footrest);
    RUN_TEST(test_either_and_both_footrest_inputs);
    RUN_TEST(test_press_debounce_is_30_ms);
    RUN_TEST(test_release_debounce_is_100_ms);
    RUN_TEST(test_rapid_bounce_requires_full_stable_window);
    RUN_TEST(test_group_state_consistency_for_all_combinations);
    RUN_TEST(test_state_json_fields_and_boolean_consistency);
    RUN_TEST(test_sequence_increments_monotonically);
    RUN_TEST(test_new_boot_entropy_changes_boot_id);
    RUN_TEST(test_lwt_payload);
    RUN_TEST(test_periodic_three_second_state_refresh);
    RUN_TEST(test_stable_change_is_immediately_due);
    RUN_TEST(test_reconnect_republishes_availability_and_state);
    RUN_TEST(test_failed_publish_retries_without_busy_loop);
    RUN_TEST(test_failed_telemetry_retries_without_busy_loop);
}

#ifdef ARDUINO

void setup() {
    delay(1500);
    UNITY_BEGIN();
    runAllTests();
    UNITY_END();
}

void loop() {
    delay(1000);
}

#else

int main(int, char**) {
    UNITY_BEGIN();
    runAllTests();
    return UNITY_END();
}

#endif
