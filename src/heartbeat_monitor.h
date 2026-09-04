#pragma once

// Tracks whether Talaria has said anything recently. "now" is always
// passed in (rather than read from millis() internally) so this can be
// unit-tested natively without a board.
class HeartbeatMonitor {
public:
    explicit HeartbeatMonitor(unsigned long timeoutMs);

    // Call whenever Talaria (or a manual test) makes contact: a status
    // change, a signal post/delete, or a dedicated heartbeat ping.
    void markContact(unsigned long nowMs);

    // True once timeoutMs has passed since the last markContact(), or if
    // markContact() has never been called at all (nothing heard yet).
    bool isStale(unsigned long nowMs) const;

    // Milliseconds since the last contact, or -1 if there's been none yet.
    long msSinceContact(unsigned long nowMs) const;

private:
    unsigned long timeoutMs_;
    unsigned long lastContactMs_ = 0;
    bool everContacted_ = false;
};
