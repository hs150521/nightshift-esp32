// config.h - Centralized configuration for nightshift pressure node
#pragma once

#include <cstdint>

// ---------- Firmware version ----------
#define FW_VERSION "0.1.0"

// ---------- Device identity ----------
#define DEVICE_ID "pressure-01"

// ---------- GPIO pin mapping (active-high digital inputs) ----------
// Cushion group
constexpr int PIN_CUSHION_LEFT  = 4;   // GPIO4
constexpr int PIN_CUSHION_RIGHT = 5;   // GPIO5
// Footrest group
constexpr int PIN_FOOTREST_LEFT  = 6;  // GPIO6
constexpr int PIN_FOOTREST_RIGHT = 7;  // GPIO7

constexpr int SENSOR_PINS[] = {
    PIN_CUSHION_LEFT,
    PIN_CUSHION_RIGHT,
    PIN_FOOTREST_LEFT,
    PIN_FOOTREST_RIGHT
};
constexpr int NUM_SENSORS = 4;

// ---------- Timing (milliseconds) ----------
constexpr uint32_t GPIO_SAMPLE_INTERVAL_MS   = 10;    // ~10 ms sampling
constexpr uint32_t DEBOUNCE_PRESS_MS         = 30;    // 30 ms press debounce
constexpr uint32_t DEBOUNCE_RELEASE_MS       = 100;   // 100 ms release debounce
constexpr uint32_t STATE_PUBLISH_INTERVAL_MS = 3000;  // 3 s periodic state refresh
constexpr uint32_t TELEMETRY_INTERVAL_MS     = 30000; // ~30 s telemetry
constexpr uint16_t MQTT_KEEPALIVE_S          = 30;    // MQTT keepalive in seconds

// ---------- Wi-Fi ----------
constexpr uint32_t WIFI_RECONNECT_MIN_MS = 1000;
constexpr uint32_t WIFI_RECONNECT_MAX_MS = 30000;

// ---------- MQTT broker ----------
#define MQTT_HOST "192.168.51.1"
constexpr uint16_t MQTT_PORT = 1884;

// ---------- MQTT topics ----------
#define MQTT_TOPIC_PREFIX "nightshift/v1/sensor/pressure/" DEVICE_ID
#define MQTT_TOPIC_AVAILABILITY MQTT_TOPIC_PREFIX "/availability"
#define MQTT_TOPIC_STATE        MQTT_TOPIC_PREFIX "/state"
#define MQTT_TOPIC_TELEMETRY    MQTT_TOPIC_PREFIX "/telemetry"

// ---------- MQTT client ID ----------
// Includes device_id plus a boot-unique suffix to avoid duplicate connections
// The suffix is set at runtime from boot_id
#define MQTT_CLIENT_ID_PREFIX "pressure-01-"
