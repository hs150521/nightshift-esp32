// config.h - Centralized configuration for nightshift pressure node
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#define FW_VERSION "0.1.0"
#define DEVICE_ID "pressure-01"

namespace nightshift {

constexpr int PIN_CUSHION_LEFT = 4;
constexpr int PIN_CUSHION_RIGHT = 5;
constexpr int PIN_FOOTREST_LEFT = 6;
constexpr int PIN_FOOTREST_RIGHT = 7;
constexpr std::array<int, 4> SENSOR_PINS = {
    PIN_CUSHION_LEFT, PIN_CUSHION_RIGHT, PIN_FOOTREST_LEFT, PIN_FOOTREST_RIGHT
};
constexpr size_t SENSOR_COUNT = SENSOR_PINS.size();

constexpr uint32_t GPIO_SAMPLE_INTERVAL_MS = 10;
constexpr uint32_t DEBOUNCE_PRESS_MS = 30;
constexpr uint32_t DEBOUNCE_RELEASE_MS = 100;
constexpr uint32_t STATE_PUBLISH_INTERVAL_MS = 3000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 30000;
constexpr uint32_t PUBLISH_RETRY_MS = 500;

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_RECONNECT_MIN_MS = 1000;
constexpr uint32_t WIFI_RECONNECT_MAX_MS = 30000;
constexpr uint32_t MQTT_RECONNECT_MIN_MS = 1000;
constexpr uint32_t MQTT_RECONNECT_MAX_MS = 30000;
constexpr uint16_t MQTT_KEEPALIVE_S = 30;
constexpr int MQTT_OUTBOX_LIMIT_BYTES = 2048;

#define MQTT_HOST "192.168.51.1"
constexpr uint16_t MQTT_PORT = 1884;

#define MQTT_TOPIC_PREFIX "nightshift/v1/sensor/pressure/" DEVICE_ID
#define MQTT_TOPIC_AVAILABILITY MQTT_TOPIC_PREFIX "/availability"
#define MQTT_TOPIC_STATE MQTT_TOPIC_PREFIX "/state"
#define MQTT_TOPIC_TELEMETRY MQTT_TOPIC_PREFIX "/telemetry"
#define MQTT_CLIENT_ID_PREFIX "pressure-01-"

constexpr const char* TIME_BASE = "monotonic_boot_ms";

}  // namespace nightshift
