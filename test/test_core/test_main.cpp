#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "boot_identity.h"
#include "connection_lifecycle.h"
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

void test_active_low_electrical_level_is_normalized_to_logical_trigger() {
    TEST_ASSERT_TRUE(nightshift::sensorTriggeredFromElectricalLevel(false));
    TEST_ASSERT_FALSE(nightshift::sensorTriggeredFromElectricalLevel(true));
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

void test_initial_mqtt_connection() {
    MqttLifecycleState mqtt;
    mqtt.markConnecting();
    const MqttLifecycleTransition transition =
        mqtt.apply(MqttLifecycleEventType::Connected);

    TEST_ASSERT_TRUE(transition.becameConnected);
    TEST_ASSERT_TRUE(transition.resetBackoff);
    TEST_ASSERT_TRUE(mqtt.isConnected());
    TEST_ASSERT_FALSE(mqtt.isConnecting());
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.reconnectCount());
    TEST_ASSERT_EQUAL_UINT32(1, mqtt.connectionGeneration());

    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Connected,
        coordinator.update(
            true,
            mqtt.isConnected(),
            mqtt.connectionGeneration(),
            100,
            scheduler
        )
    );
    TEST_ASSERT_TRUE(scheduler.availabilityDue(100));
    TEST_ASSERT_TRUE(scheduler.stateDue(100));
}

void test_wifi_down_then_up() {
    MqttLifecycleState mqtt;
    mqtt.apply(MqttLifecycleEventType::Connected);
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    coordinator.update(
        true, true, mqtt.connectionGeneration(), 100, scheduler
    );
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Disconnected,
        coordinator.update(
            false, true, mqtt.connectionGeneration(), 200, scheduler
        )
    );
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::None,
        coordinator.update(
            false, true, mqtt.connectionGeneration(), 201, scheduler
        )
    );
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Connected,
        coordinator.update(
            true, true, mqtt.connectionGeneration(), 300, scheduler
        )
    );
    TEST_ASSERT_TRUE(scheduler.availabilityDue(300));
    TEST_ASSERT_TRUE(scheduler.stateDue(300));
}

void test_mqtt_disconnect_then_reconnect() {
    MqttLifecycleState mqtt;
    mqtt.apply(MqttLifecycleEventType::Connected);
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    coordinator.update(
        true, true, mqtt.connectionGeneration(), 100, scheduler
    );
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    const MqttLifecycleTransition disconnected =
        mqtt.apply(MqttLifecycleEventType::Disconnected);
    TEST_ASSERT_TRUE(disconnected.becameDisconnected);
    TEST_ASSERT_TRUE(disconnected.scheduleReconnect);
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Disconnected,
        coordinator.update(
            true, false, mqtt.connectionGeneration(), 200, scheduler
        )
    );

    mqtt.markConnecting();
    const MqttLifecycleTransition reconnected =
        mqtt.apply(MqttLifecycleEventType::Connected);
    TEST_ASSERT_TRUE(reconnected.becameConnected);
    TEST_ASSERT_EQUAL_UINT32(1, mqtt.reconnectCount());
    TEST_ASSERT_EQUAL_UINT32(2, mqtt.connectionGeneration());
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Connected,
        coordinator.update(
            true, true, mqtt.connectionGeneration(), 300, scheduler
        )
    );
    TEST_ASSERT_TRUE(scheduler.availabilityDue(300));
    TEST_ASSERT_TRUE(scheduler.stateDue(300));
}

void test_disconnected_then_connected_before_one_update() {
    MqttLifecycleState mqtt;
    mqtt.apply(MqttLifecycleEventType::Connected);
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    coordinator.update(
        true, true, mqtt.connectionGeneration(), 100, scheduler
    );
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    const MqttLifecycleTransition disconnected =
        mqtt.apply(MqttLifecycleEventType::Disconnected);
    const MqttLifecycleTransition connected =
        mqtt.apply(MqttLifecycleEventType::Connected);

    TEST_ASSERT_TRUE(disconnected.scheduleReconnect);
    TEST_ASSERT_TRUE(connected.becameConnected);
    TEST_ASSERT_TRUE(mqtt.isConnected());
    TEST_ASSERT_EQUAL_UINT32(1, mqtt.reconnectCount());
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Reconnected,
        coordinator.update(
            true, true, mqtt.connectionGeneration(), 200, scheduler
        )
    );
    TEST_ASSERT_TRUE(scheduler.availabilityDue(200));
    TEST_ASSERT_TRUE(scheduler.stateDue(200));
}

void test_connected_then_disconnected_before_one_update() {
    MqttLifecycleState mqtt;
    mqtt.apply(MqttLifecycleEventType::Connected);
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    coordinator.update(
        true, true, mqtt.connectionGeneration(), 100, scheduler
    );
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    const MqttLifecycleTransition duplicateConnected =
        mqtt.apply(MqttLifecycleEventType::Connected);
    const MqttLifecycleTransition disconnected =
        mqtt.apply(MqttLifecycleEventType::Disconnected);

    TEST_ASSERT_FALSE(duplicateConnected.becameConnected);
    TEST_ASSERT_TRUE(disconnected.becameDisconnected);
    TEST_ASSERT_FALSE(mqtt.isConnected());
    TEST_ASSERT_EQUAL_UINT32(0, mqtt.reconnectCount());
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Disconnected,
        coordinator.update(
            true, false, mqtt.connectionGeneration(), 200, scheduler
        )
    );
    TEST_ASSERT_FALSE(scheduler.availabilityDue(200));
    TEST_ASSERT_FALSE(scheduler.stateDue(200));
}

void test_wifi_recovery_without_fresh_mqtt_event() {
    MqttLifecycleState mqtt;
    mqtt.apply(MqttLifecycleEventType::Connected);
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    coordinator.update(
        true, true, mqtt.connectionGeneration(), 100, scheduler
    );
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    coordinator.update(
        false, mqtt.isConnected(), mqtt.connectionGeneration(), 200, scheduler
    );
    TEST_ASSERT_TRUE(mqtt.isConnected());
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Connected,
        coordinator.update(
            true,
            mqtt.isConnected(),
            mqtt.connectionGeneration(),
            300,
            scheduler
        )
    );
    TEST_ASSERT_TRUE(scheduler.availabilityDue(300));
    TEST_ASSERT_TRUE(scheduler.stateDue(300));
}

void test_scheduler_receives_one_disconnect_edge() {
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    coordinator.update(true, true, 1, 100, scheduler);

    unsigned disconnectEdges = 0;
    for (uint32_t now = 200; now < 300; ++now) {
        if (
            coordinator.update(false, true, 1, now, scheduler) ==
            EffectiveConnectionEdge::Disconnected
        ) {
            ++disconnectEdges;
        }
    }
    TEST_ASSERT_EQUAL_UINT(1, disconnectEdges);
}

void test_every_recovery_republishes_availability_and_state() {
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    const EffectiveConnectionEdge initial =
        coordinator.update(true, true, 1, 100, scheduler);
#ifdef ARDUINO
    Serial.printf(
        "[LifecycleTest] edge=connected availability_due=%d state_due=%d\n",
        scheduler.availabilityDue(100),
        scheduler.stateDue(100)
    );
#endif
    TEST_ASSERT_EQUAL(EffectiveConnectionEdge::Connected, initial);
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    const EffectiveConnectionEdge down =
        coordinator.update(false, true, 1, 200, scheduler);
    const EffectiveConnectionEdge wifiRecovery =
        coordinator.update(true, true, 1, 300, scheduler);
#ifdef ARDUINO
    Serial.printf(
        "[LifecycleTest] edge=disconnected then=connected "
        "availability_due=%d state_due=%d\n",
        scheduler.availabilityDue(300),
        scheduler.stateDue(300)
    );
#endif
    TEST_ASSERT_EQUAL(EffectiveConnectionEdge::Disconnected, down);
    TEST_ASSERT_EQUAL(EffectiveConnectionEdge::Connected, wifiRecovery);
    TEST_ASSERT_TRUE(scheduler.availabilityDue(300));
    TEST_ASSERT_TRUE(scheduler.stateDue(300));
    scheduler.availabilityAttempted(300, true);
    scheduler.stateAttempted(300, true);

    const EffectiveConnectionEdge mqttRecovery =
        coordinator.update(true, true, 2, 400, scheduler);
#ifdef ARDUINO
    Serial.printf(
        "[LifecycleTest] edge=reconnected availability_due=%d state_due=%d\n",
        scheduler.availabilityDue(400),
        scheduler.stateDue(400)
    );
#endif
    TEST_ASSERT_EQUAL(EffectiveConnectionEdge::Reconnected, mqttRecovery);
    TEST_ASSERT_TRUE(scheduler.availabilityDue(400));
    TEST_ASSERT_TRUE(scheduler.stateDue(400));
}

void test_stable_connected_updates_do_not_republish_connection_messages() {
    PublishScheduler scheduler;
    EffectiveConnectivityCoordinator coordinator;
    TEST_ASSERT_EQUAL(
        EffectiveConnectionEdge::Connected,
        coordinator.update(true, true, 1, 100, scheduler)
    );
    scheduler.availabilityAttempted(100, true);
    scheduler.stateAttempted(100, true);

    for (uint32_t now = 101; now < 1000; ++now) {
        TEST_ASSERT_EQUAL(
            EffectiveConnectionEdge::None,
            coordinator.update(true, true, 1, now, scheduler)
        );
        TEST_ASSERT_FALSE(scheduler.availabilityDue(now));
        TEST_ASSERT_FALSE(scheduler.stateDue(now));
    }
}

}  // namespace

void setUp() {}
void tearDown() {}

void runAllTests() {
    RUN_TEST(test_active_low_electrical_level_is_normalized_to_logical_trigger);
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
    RUN_TEST(test_initial_mqtt_connection);
    RUN_TEST(test_wifi_down_then_up);
    RUN_TEST(test_mqtt_disconnect_then_reconnect);
    RUN_TEST(test_disconnected_then_connected_before_one_update);
    RUN_TEST(test_connected_then_disconnected_before_one_update);
    RUN_TEST(test_wifi_recovery_without_fresh_mqtt_event);
    RUN_TEST(test_scheduler_receives_one_disconnect_edge);
    RUN_TEST(test_every_recovery_republishes_availability_and_state);
    RUN_TEST(test_stable_connected_updates_do_not_republish_connection_messages);
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
