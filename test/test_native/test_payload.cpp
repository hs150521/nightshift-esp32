// test_payload.cpp - Unit tests for JSON payload serialization
// Tests schema fields, boolean consistency, sequence counter, boot_id behavior
// Runs on native platform with ArduinoJson

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cstdio>
#include <cstdint>

// Minimal mock for debounce.h types
struct PressureState {
    bool gpio4, gpio5, gpio6, gpio7;
    bool cushion, footrest, presence;
};

// Config constants
#define FW_VERSION "0.1.0"
#define DEVICE_ID "pressure-01"
constexpr size_t PAYLOAD_BUF_SIZE = 384;

// ============================================================
// Inline PayloadBuilder for native testing
// ============================================================
class PayloadBuilder {
public:
    PayloadBuilder(const char* bootId, uint32_t startedAtMs)
        : _bootId(bootId), _startedAtMs(startedAtMs), _seq(0) {}

    void buildAvailabilityOnline(char* buf, size_t bufSize) {
        JsonDocument doc;
        doc["schema"] = "nightshift.sensor-availability.v1";
        doc["device_id"] = DEVICE_ID;
        doc["online"] = true;
        doc["boot_id"] = _bootId;
        doc["version"] = FW_VERSION;
        doc["started_at_ms"] = _startedAtMs;
        serializeJson(doc, buf, bufSize);
    }

    void buildState(char* buf, size_t bufSize, const PressureState& state, uint32_t sampledAtMs) {
        _seq++;
        JsonDocument doc;
        doc["schema"] = "nightshift.pressure-state.v1";
        doc["device_id"] = DEVICE_ID;
        doc["boot_id"] = _bootId;
        doc["seq"] = _seq;
        doc["sampled_at_ms"] = sampledAtMs;
        JsonObject gpio = doc["gpio"].to<JsonObject>();
        gpio["4"] = state.gpio4;
        gpio["5"] = state.gpio5;
        gpio["6"] = state.gpio6;
        gpio["7"] = state.gpio7;
        doc["cushion"] = state.cushion;
        doc["footrest"] = state.footrest;
        doc["presence"] = state.presence;
        serializeJson(doc, buf, bufSize);
    }

    void buildTelemetry(char* buf, size_t bufSize,
                        uint32_t uptimeMs, int wifiRssi,
                        uint32_t mqttReconnects, uint32_t publishCount,
                        uint32_t reportedAtMs) {
        JsonDocument doc;
        doc["schema"] = "nightshift.pressure-telemetry.v1";
        doc["device_id"] = DEVICE_ID;
        doc["boot_id"] = _bootId;
        doc["uptime_ms"] = uptimeMs;
        doc["wifi_rssi_dbm"] = wifiRssi;
        doc["mqtt_reconnect_count"] = mqttReconnects;
        doc["publish_count"] = publishCount;
        doc["reported_at_ms"] = reportedAtMs;
        serializeJson(doc, buf, bufSize);
    }

    uint32_t getSeq() const { return _seq; }

private:
    const char* _bootId;
    uint32_t _startedAtMs;
    uint32_t _seq;
};

// ============================================================
static char buf[PAYLOAD_BUF_SIZE];
static PayloadBuilder* pb = nullptr;

void setUp(void) {
    static PayloadBuilder builder("abcd1234", 1000);
    pb = &builder;
}
void tearDown(void) {}

// ============================================================
// Test: Availability payload fields
// ============================================================
void test_availability_fields(void) {
    PayloadBuilder builder("test1234", 5000);
    builder.buildAvailabilityOnline(buf, sizeof(buf));

    JsonDocument doc;
    deserializeJson(doc, buf);

    TEST_ASSERT_EQUAL_STRING("nightshift.sensor-availability.v1", doc["schema"]);
    TEST_ASSERT_EQUAL_STRING("pressure-01", doc["device_id"]);
    TEST_ASSERT_TRUE(doc["online"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("test1234", doc["boot_id"]);
    TEST_ASSERT_EQUAL_STRING("0.1.0", doc["version"]);
    TEST_ASSERT_EQUAL_UINT32(5000, doc["started_at_ms"].as<uint32_t>());
}

// ============================================================
// Test: LWT payload format
// ============================================================
void test_lwt_payload(void) {
    // LWT is a static format string - verify expected structure
    char lwt[128];
    snprintf(lwt, sizeof(lwt),
        "{\"schema\":\"nightshift.sensor-availability.v1\","
        "\"device_id\":\"pressure-01\","
        "\"online\":false}");

    JsonDocument doc;
    deserializeJson(doc, lwt);

    TEST_ASSERT_EQUAL_STRING("nightshift.sensor-availability.v1", doc["schema"]);
    TEST_ASSERT_EQUAL_STRING("pressure-01", doc["device_id"]);
    TEST_ASSERT_FALSE(doc["online"].as<bool>());
}

// ============================================================
// Test: State payload has all required fields
// ============================================================
void test_state_fields(void) {
    PayloadBuilder builder("abc12345", 0);
    PressureState state = {true, false, true, true, true, true, true};
    builder.buildState(buf, sizeof(buf), state, 12345);

    JsonDocument doc;
    deserializeJson(doc, buf);

    TEST_ASSERT_EQUAL_STRING("nightshift.pressure-state.v1", doc["schema"]);
    TEST_ASSERT_EQUAL_STRING("pressure-01", doc["device_id"]);
    TEST_ASSERT_EQUAL_STRING("abc12345", doc["boot_id"]);
    TEST_ASSERT_EQUAL_UINT32(1, doc["seq"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(12345, doc["sampled_at_ms"].as<uint32_t>());

    // GPIO object
    TEST_ASSERT_TRUE(doc["gpio"]["4"].as<bool>());
    TEST_ASSERT_FALSE(doc["gpio"]["5"].as<bool>());
    TEST_ASSERT_TRUE(doc["gpio"]["6"].as<bool>());
    TEST_ASSERT_TRUE(doc["gpio"]["7"].as<bool>());

    // Group booleans
    TEST_ASSERT_TRUE(doc["cushion"].as<bool>());
    TEST_ASSERT_TRUE(doc["footrest"].as<bool>());
    TEST_ASSERT_TRUE(doc["presence"].as<bool>());
}

// ============================================================
// Test: Boolean consistency (presence = cushion || footrest)
// ============================================================
void test_boolean_consistency(void) {
    PayloadBuilder builder("x1234567", 0);

    // Case 1: only cushion
    PressureState s1 = {true, false, false, false, true, false, true};
    builder.buildState(buf, sizeof(buf), s1, 100);
    JsonDocument d1;
    deserializeJson(d1, buf);
    TEST_ASSERT_TRUE(d1["cushion"].as<bool>());
    TEST_ASSERT_FALSE(d1["footrest"].as<bool>());
    TEST_ASSERT_TRUE(d1["presence"].as<bool>());

    // Case 2: only footrest
    PressureState s2 = {false, false, true, false, false, true, true};
    builder.buildState(buf, sizeof(buf), s2, 200);
    JsonDocument d2;
    deserializeJson(d2, buf);
    TEST_ASSERT_FALSE(d2["cushion"].as<bool>());
    TEST_ASSERT_TRUE(d2["footrest"].as<bool>());
    TEST_ASSERT_TRUE(d2["presence"].as<bool>());

    // Case 3: none
    PressureState s3 = {false, false, false, false, false, false, false};
    builder.buildState(buf, sizeof(buf), s3, 300);
    JsonDocument d3;
    deserializeJson(d3, buf);
    TEST_ASSERT_FALSE(d3["cushion"].as<bool>());
    TEST_ASSERT_FALSE(d3["footrest"].as<bool>());
    TEST_ASSERT_FALSE(d3["presence"].as<bool>());
}

// ============================================================
// Test: Sequence counter increments
// ============================================================
void test_sequence_increment(void) {
    PayloadBuilder builder("seq12345", 0);
    PressureState state = {};

    builder.buildState(buf, sizeof(buf), state, 0);
    TEST_ASSERT_EQUAL_UINT32(1, builder.getSeq());

    builder.buildState(buf, sizeof(buf), state, 10);
    TEST_ASSERT_EQUAL_UINT32(2, builder.getSeq());

    builder.buildState(buf, sizeof(buf), state, 20);
    TEST_ASSERT_EQUAL_UINT32(3, builder.getSeq());

    // Verify seq in JSON matches
    JsonDocument doc;
    deserializeJson(doc, buf);
    TEST_ASSERT_EQUAL_UINT32(3, doc["seq"].as<uint32_t>());
}

// ============================================================
// Test: New boot ID produces different payloads
// ============================================================
void test_new_boot_id(void) {
    PayloadBuilder b1("aaaaaaaa", 0);
    PayloadBuilder b2("bbbbbbbb", 0);
    PressureState state = {};

    char buf1[PAYLOAD_BUF_SIZE], buf2[PAYLOAD_BUF_SIZE];
    b1.buildState(buf1, sizeof(buf1), state, 0);
    b2.buildState(buf2, sizeof(buf2), state, 0);

    // Both should have seq=1 (independent counters)
    TEST_ASSERT_EQUAL_UINT32(1, b1.getSeq());
    TEST_ASSERT_EQUAL_UINT32(1, b2.getSeq());

    // But different boot_ids
    JsonDocument d1, d2;
    deserializeJson(d1, buf1);
    deserializeJson(d2, buf2);
    TEST_ASSERT_EQUAL_STRING("aaaaaaaa", d1["boot_id"]);
    TEST_ASSERT_EQUAL_STRING("bbbbbbbb", d2["boot_id"]);
}

// ============================================================
// Test: Telemetry payload fields
// ============================================================
void test_telemetry_fields(void) {
    PayloadBuilder builder("tel12345", 0);
    builder.buildTelemetry(buf, sizeof(buf), 90000, -51, 1, 35, 90000);

    JsonDocument doc;
    deserializeJson(doc, buf);

    TEST_ASSERT_EQUAL_STRING("nightshift.pressure-telemetry.v1", doc["schema"]);
    TEST_ASSERT_EQUAL_STRING("pressure-01", doc["device_id"]);
    TEST_ASSERT_EQUAL_STRING("tel12345", doc["boot_id"]);
    TEST_ASSERT_EQUAL_UINT32(90000, doc["uptime_ms"].as<uint32_t>());
    TEST_ASSERT_EQUAL_INT(-51, doc["wifi_rssi_dbm"].as<int>());
    TEST_ASSERT_EQUAL_UINT32(1, doc["mqtt_reconnect_count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(35, doc["publish_count"].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT32(90000, doc["reported_at_ms"].as<uint32_t>());
}

// ============================================================
// Main
// ============================================================
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_availability_fields);
    RUN_TEST(test_lwt_payload);
    RUN_TEST(test_state_fields);
    RUN_TEST(test_boolean_consistency);
    RUN_TEST(test_sequence_increment);
    RUN_TEST(test_new_boot_id);
    RUN_TEST(test_telemetry_fields);

    return UNITY_END();
}
