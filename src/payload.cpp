// payload.cpp - JSON payload serialization using ArduinoJson
#include "payload.h"
#include "config.h"
#include <ArduinoJson.h>

PayloadBuilder::PayloadBuilder(const char* bootId, uint32_t startedAtMs)
    : _bootId(bootId), _startedAtMs(startedAtMs), _seq(0) {}

void PayloadBuilder::buildAvailabilityOnline(char* buf, size_t bufSize) {
    JsonDocument doc;
    doc["schema"] = "nightshift.sensor-availability.v1";
    doc["device_id"] = DEVICE_ID;
    doc["online"] = true;
    doc["boot_id"] = _bootId;
    doc["version"] = FW_VERSION;
    doc["started_at_ms"] = _startedAtMs;

    serializeJson(doc, buf, bufSize);
}

void PayloadBuilder::buildState(char* buf, size_t bufSize, const PressureState& state, uint32_t sampledAtMs) {
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

void PayloadBuilder::buildTelemetry(char* buf, size_t bufSize,
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
