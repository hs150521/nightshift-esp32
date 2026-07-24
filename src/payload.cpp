#include "payload.h"

#include "config.h"

#include <ArduinoJson.h>

namespace {

bool serializeChecked(const JsonDocument& doc, char* buf, size_t bufSize) {
    if (buf == nullptr || bufSize == 0) {
        return false;
    }
    const size_t required = measureJson(doc);
    if (required + 1 > bufSize) {
        buf[0] = '\0';
        return false;
    }
    return serializeJson(doc, buf, bufSize) == required;
}

}  // namespace

PayloadBuilder::PayloadBuilder(const char* bootId) : bootId_(bootId) {}

bool PayloadBuilder::buildAvailabilityOnline(char* buf, size_t bufSize) const {
    JsonDocument doc;
    doc["schema"] = "nightshift.sensor-availability.v1";
    doc["device_id"] = DEVICE_ID;
    doc["online"] = true;
    doc["boot_id"] = bootId_;
    doc["version"] = FW_VERSION;
    doc["started_at_ms"] = 0;
    doc["time_base"] = nightshift::TIME_BASE;
    return serializeChecked(doc, buf, bufSize);
}

bool PayloadBuilder::buildAvailabilityOffline(char* buf, size_t bufSize) {
    JsonDocument doc;
    doc["schema"] = "nightshift.sensor-availability.v1";
    doc["device_id"] = DEVICE_ID;
    doc["online"] = false;
    return serializeChecked(doc, buf, bufSize);
}

bool PayloadBuilder::buildState(
    char* buf,
    size_t bufSize,
    const PressureState& state,
    uint32_t sampledAtMs
) {
    const GpioSnapshot gpio = {state.gpio4, state.gpio5, state.gpio6, state.gpio7};
    const PressureState normalized = makePressureState(gpio);
    ++seq_;

    JsonDocument doc;
    doc["schema"] = "nightshift.pressure-state.v1";
    doc["device_id"] = DEVICE_ID;
    doc["boot_id"] = bootId_;
    doc["seq"] = seq_;
    doc["sampled_at_ms"] = sampledAtMs;
    doc["time_base"] = nightshift::TIME_BASE;

    JsonObject gpioJson = doc["gpio"].to<JsonObject>();
    gpioJson["4"] = normalized.gpio4;
    gpioJson["5"] = normalized.gpio5;
    gpioJson["6"] = normalized.gpio6;
    gpioJson["7"] = normalized.gpio7;

    doc["cushion"] = normalized.cushion;
    doc["footrest"] = normalized.footrest;
    doc["presence"] = normalized.presence;

    return serializeChecked(doc, buf, bufSize);
}

bool PayloadBuilder::buildTelemetry(
    char* buf,
    size_t bufSize,
    uint32_t uptimeMs,
    int wifiRssi,
    uint32_t mqttReconnects,
    uint32_t publishCount,
    uint32_t reportedAtMs
) const {
    JsonDocument doc;
    doc["schema"] = "nightshift.pressure-telemetry.v1";
    doc["device_id"] = DEVICE_ID;
    doc["boot_id"] = bootId_;
    doc["uptime_ms"] = uptimeMs;
    doc["wifi_rssi_dbm"] = wifiRssi;
    doc["mqtt_reconnect_count"] = mqttReconnects;
    doc["publish_count"] = publishCount;
    doc["reported_at_ms"] = reportedAtMs;
    doc["time_base"] = nightshift::TIME_BASE;

    return serializeChecked(doc, buf, bufSize);
}
