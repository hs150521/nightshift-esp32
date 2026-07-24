#include "publish_scheduler.h"

#include "config.h"

#include <cstdint>

bool PublishScheduler::reached(uint32_t nowMs, uint32_t dueAtMs) {
    return static_cast<int32_t>(nowMs - dueAtMs) >= 0;
}

void PublishScheduler::onConnected(uint32_t nowMs) {
    connected_ = true;
    availabilityPending_ = true;
    statePending_ = true;
    availabilityDueAtMs_ = nowMs;
    stateDueAtMs_ = nowMs;
}

void PublishScheduler::onDisconnected() {
    connected_ = false;
}

void PublishScheduler::onStateChanged(uint32_t nowMs) {
    statePending_ = true;
    stateDueAtMs_ = nowMs;
}

bool PublishScheduler::availabilityDue(uint32_t nowMs) const {
    return connected_ && availabilityPending_ && reached(nowMs, availabilityDueAtMs_);
}

bool PublishScheduler::stateDue(uint32_t nowMs) const {
    if (!connected_) {
        return false;
    }
    if (statePending_) {
        return reached(nowMs, stateDueAtMs_);
    }
    return statePublished_ &&
        nowMs - lastStatePublishedAtMs_ >= nightshift::STATE_PUBLISH_INTERVAL_MS;
}

bool PublishScheduler::telemetryDue(uint32_t nowMs) const {
    return connected_ && reached(nowMs, telemetryDueAtMs_);
}

void PublishScheduler::availabilityAttempted(uint32_t nowMs, bool success) {
    availabilityPending_ = !success;
    if (!success) {
        availabilityDueAtMs_ = nowMs + nightshift::PUBLISH_RETRY_MS;
    }
}

void PublishScheduler::stateAttempted(uint32_t nowMs, bool success) {
    statePending_ = !success;
    if (success) {
        statePublished_ = true;
        lastStatePublishedAtMs_ = nowMs;
    } else {
        stateDueAtMs_ = nowMs + nightshift::PUBLISH_RETRY_MS;
    }
}

void PublishScheduler::telemetryAttempted(uint32_t nowMs, bool success) {
    telemetryDueAtMs_ = nowMs + (
        success
            ? nightshift::TELEMETRY_INTERVAL_MS
            : nightshift::PUBLISH_RETRY_MS
    );
}
