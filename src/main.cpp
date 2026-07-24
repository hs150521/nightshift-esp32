// main.cpp - Nightshift pressure node application coordinator
// Device: pressure-01 | Board: ESP32-S3-DevKitC-1
#include <Arduino.h>
#include "config.h"
#include "debounce.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "payload.h"

// ---------- Boot identity ----------
static char bootId[9];  // 8 hex chars + null

static void generateBootId() {
    uint32_t r = esp_random();
    snprintf(bootId, sizeof(bootId), "%08x", r);
}

// ---------- Global instances ----------
static Debouncer debouncer;
static WiFiManager wifiMgr;
static MqttManager mqttMgr;
static PayloadBuilder* payloadBuilder = nullptr;

// ---------- Timing state ----------
static uint32_t lastSampleMs = 0;
static uint32_t lastStatePublishMs = 0;
static uint32_t lastTelemetryMs = 0;
static bool mqttJustConnected = false;

// ---------- Payload buffer ----------
static char payloadBuf[PAYLOAD_BUF_SIZE];

// ---------- Previous state for change detection ----------
static PressureState prevState = {};

static bool stateChanged(const PressureState& a, const PressureState& b) {
    return a.gpio4 != b.gpio4 || a.gpio5 != b.gpio5 ||
           a.gpio6 != b.gpio6 || a.gpio7 != b.gpio7;
}

// ---------- Publish helpers ----------
static void publishAvailabilityOnline() {
    payloadBuilder->buildAvailabilityOnline(payloadBuf, sizeof(payloadBuf));
    if (mqttMgr.publishAvailability(payloadBuf)) {
        Serial.println("[App] Published availability: online");
    }
}

static void publishCurrentState(uint32_t now) {
    PressureState state = debouncer.getState();
    payloadBuilder->buildState(payloadBuf, sizeof(payloadBuf), state, now);
    if (mqttMgr.publishState(payloadBuf)) {
        Serial.printf("[App] Published state seq=%lu cush=%d foot=%d pres=%d\n",
            payloadBuilder->getSeq(), state.cushion, state.footrest, state.presence);
    }
    prevState = state;
    lastStatePublishMs = now;
}

static void publishTelemetry(uint32_t now) {
    payloadBuilder->buildTelemetry(payloadBuf, sizeof(payloadBuf),
        now,  // uptime_ms (millis since boot)
        wifiMgr.getRSSI(),
        mqttMgr.getReconnectCount(),
        mqttMgr.getPublishCount(),
        now);

    if (mqttMgr.publishTelemetry(payloadBuf)) {
        Serial.println("[App] Published telemetry");
    }
    lastTelemetryMs = now;
}

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);
    // Wait for USB CDC to enumerate (up to 3s, non-blocking if no host)
    uint32_t serialWait = millis();
    while (!Serial && millis() - serialWait < 3000) { delay(10); }
    delay(200);  // Extra settle time

    generateBootId();

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Nightshift Pressure Node " FW_VERSION);
    Serial.println("  Device: " DEVICE_ID);
    Serial.printf("  Boot ID: %s\n", bootId);
    Serial.println("========================================");
    Serial.println();

    // Initialize subsystems
    debouncer.begin();
    Serial.println("[GPIO] Pins initialized (pull-down, active-high)");

    // Create payload builder with boot-time millis as started_at_ms
    static PayloadBuilder pb(bootId, millis());
    payloadBuilder = &pb;

    // Start Wi-Fi (non-blocking)
    wifiMgr.begin();

    // Initialize MQTT (won't connect until Wi-Fi is ready)
    mqttMgr.begin(bootId);

    // Initialize timing
    uint32_t now = millis();
    lastSampleMs = now;
    lastStatePublishMs = now;
    lastTelemetryMs = now;

    Serial.println("[App] Setup complete, entering main loop");
}

// ========== LOOP ==========
void loop() {
    uint32_t now = millis();

    // --- GPIO sampling at ~10 ms intervals ---
    if (now - lastSampleMs >= GPIO_SAMPLE_INTERVAL_MS) {
        lastSampleMs = now;
        bool changed = debouncer.update(now);

        if (changed) {
            PressureState state = debouncer.getState();
            Serial.printf("[GPIO] Change: G4=%d G5=%d G6=%d G7=%d | cush=%d foot=%d pres=%d\n",
                state.gpio4, state.gpio5, state.gpio6, state.gpio7,
                state.cushion, state.footrest, state.presence);

            // Publish immediately if MQTT is connected
            if (mqttMgr.isConnected()) {
                publishCurrentState(now);
            }
        }
    }

    // --- Wi-Fi lifecycle ---
    bool wifiChanged = wifiMgr.update(now);
    if (wifiChanged) {
        mqttMgr.setNetworkReady(wifiMgr.isConnected());
    }

    // --- MQTT lifecycle ---
    bool mqttChanged = mqttMgr.update(now);
    if (mqttChanged && mqttMgr.isConnected()) {
        // Just connected - publish availability and current state
        mqttJustConnected = true;
        publishAvailabilityOnline();
        publishCurrentState(now);
    }

    // --- Periodic state refresh every 3 seconds ---
    if (mqttMgr.isConnected() && (now - lastStatePublishMs >= STATE_PUBLISH_INTERVAL_MS)) {
        publishCurrentState(now);
    }

    // --- Periodic telemetry every ~30 seconds ---
    if (mqttMgr.isConnected() && (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS)) {
        publishTelemetry(now);
    }

    // Small yield to avoid watchdog
    delay(1);
}
