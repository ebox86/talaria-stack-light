#include "heartbeat_monitor.h"

HeartbeatMonitor::HeartbeatMonitor(unsigned long timeoutMs) : timeoutMs_(timeoutMs) {}

void HeartbeatMonitor::markContact(unsigned long nowMs) {
    lastContactMs_ = nowMs;
    everContacted_ = true;
}

bool HeartbeatMonitor::isStale(unsigned long nowMs) const {
    if (!everContacted_) {
        return true;
    }

    // Unsigned subtraction wraps correctly around millis() rollover, same
    // as the flash-timing checks in LightController.
    return (nowMs - lastContactMs_) >= timeoutMs_;
}

long HeartbeatMonitor::msSinceContact(unsigned long nowMs) const {
    if (!everContacted_) {
        return -1;
    }

    return static_cast<long>(nowMs - lastContactMs_);
}
