#include <Arduino.h>
#include <esp_system.h>

#include "boot_identity.h"
#include "connection_lifecycle.h"
#include "config.h"
#include "debounce.h"
#include "gpio_sampler.h"
#include "mqtt_manager.h"
#include "payload.h"
#include "publish_scheduler.h"
#include "wifi_manager.h"

namespace {

char bootId[9] = {};
char clientId[32] = {};
char lwtPayload[PAYLOAD_BUF_SIZE] = {};
char payload[PAYLOAD_BUF_SIZE] = {};

GpioSampler gpioSampler;
Debouncer debouncer;
WiFiManager wifiManager;
MqttManager mqttManager;
PublishScheduler publishScheduler;
EffectiveConnectivityCoordinator connectivityCoordinator;
PayloadBuilder* payloadBuilder = nullptr;
uint32_t lastSampleAtMs = 0;

void logState(const PressureState& state) {
    Serial.printf(
        "[GPIO] logical stable g4=%d g5=%d g6=%d g7=%d cushion=%d footrest=%d presence=%d\n",
        state.gpio4,
        state.gpio5,
        state.gpio6,
        state.gpio7,
        state.cushion,
        state.footrest,
        state.presence
    );
}

void servicePublishes(uint32_t nowMs) {
    if (publishScheduler.availabilityDue(nowMs)) {
        const bool serialized =
            payloadBuilder->buildAvailabilityOnline(payload, sizeof(payload));
        const bool published =
            serialized && mqttManager.publishAvailability(payload);
        publishScheduler.availabilityAttempted(nowMs, published);
        Serial.printf("[Publish] availability online result=%s\n",
            published ? "queued-qos1-retained" : "deferred");
    }

    if (publishScheduler.stateDue(nowMs)) {
        const PressureState state = debouncer.getState();
        const bool serialized =
            payloadBuilder->buildState(payload, sizeof(payload), state, nowMs);
        const bool published = serialized && mqttManager.publishState(payload);
        publishScheduler.stateAttempted(nowMs, published);
        if (published) {
            Serial.printf(
                "[Publish] state seq=%lu result=queued-qos1-retained "
                "g4=%d g5=%d g6=%d g7=%d "
                "cushion=%d footrest=%d presence=%d\n",
                static_cast<unsigned long>(payloadBuilder->getSeq()),
                state.gpio4,
                state.gpio5,
                state.gpio6,
                state.gpio7,
                state.cushion,
                state.footrest,
                state.presence
            );
        } else {
            Serial.println("[Publish] state result=deferred");
        }
    }

    if (publishScheduler.telemetryDue(nowMs)) {
        const bool serialized = payloadBuilder->buildTelemetry(
            payload,
            sizeof(payload),
            nowMs,
            wifiManager.getRSSI(),
            mqttManager.getReconnectCount(),
            mqttManager.getPublishCount(),
            nowMs
        );
        const bool published = serialized && mqttManager.publishTelemetry(payload);
        publishScheduler.telemetryAttempted(nowMs, published);
        Serial.printf("[Publish] telemetry result=%s\n",
            published ? "queued-qos0-not-retained" : "deferred");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);

    formatBootId(esp_random(), bootId);
    formatClientId(ESP.getEfuseMac(), clientId, sizeof(clientId));
    static PayloadBuilder builder(bootId);
    payloadBuilder = &builder;
    PayloadBuilder::buildAvailabilityOffline(lwtPayload, sizeof(lwtPayload));

    Serial.println();
    Serial.printf(
        "[Boot] Nightshift Pressure Node version=%s device_id=%s boot_id=%s\n",
        FW_VERSION,
        DEVICE_ID,
        bootId
    );
    Serial.printf("[Boot] time_base=%s\n", nightshift::TIME_BASE);

    gpioSampler.begin();
    debouncer.reset(gpioSampler.sample(), millis());
    Serial.println(
        "[GPIO] initialized pins=4,5,6,7 mode=input-pullup "
        "electrical-active-low logical-1=triggered"
    );

    if (!mqttManager.begin(clientId, lwtPayload)) {
        Serial.println("[Boot] MQTT initialization failed; sampling remains active");
    }
    wifiManager.begin();

    lastSampleAtMs = millis();
    Serial.println("[Boot] setup complete");
}

void loop() {
    const uint32_t nowMs = millis();

    if (nowMs - lastSampleAtMs >= nightshift::GPIO_SAMPLE_INTERVAL_MS) {
        lastSampleAtMs = nowMs;
        if (debouncer.update(gpioSampler.sample(), nowMs)) {
            const PressureState state = debouncer.getState();
            logState(state);
            publishScheduler.onStateChanged(nowMs);
        }
    }

    wifiManager.update(nowMs);
    mqttManager.setNetworkReady(wifiManager.isConnected());
    mqttManager.update(nowMs);

    const EffectiveConnectionEdge connectionEdge = connectivityCoordinator.update(
        wifiManager.isConnected(),
        mqttManager.isTransportConnected(),
        mqttManager.getConnectionGeneration(),
        nowMs,
        publishScheduler
    );
    switch (connectionEdge) {
        case EffectiveConnectionEdge::Connected:
            Serial.printf(
                "[Network] effective connected mqtt_generation=%lu\n",
                static_cast<unsigned long>(
                    mqttManager.getConnectionGeneration()
                )
            );
            break;
        case EffectiveConnectionEdge::Disconnected:
            Serial.println("[Network] effective disconnected");
            break;
        case EffectiveConnectionEdge::Reconnected:
            Serial.printf(
                "[Network] effective reconnected mqtt_generation=%lu\n",
                static_cast<unsigned long>(
                    mqttManager.getConnectionGeneration()
                )
            );
            break;
        case EffectiveConnectionEdge::None:
            break;
    }

    servicePublishes(nowMs);
    delay(1);
}
