#pragma once

#include <string>
#include <vector>

#include "signal_types.h"

enum class SignalSeverity {
    INFO,
    WARNING,
    CRITICAL,
    EMERGENCY
};

// Maps a severity to how it should look on the light.
SignalState signalForSeverity(SignalSeverity severity);

const char* toString(SignalSeverity severity);

// Case-insensitive parse of "info" / "warning" / "critical" / "emergency".
// Returns false (and leaves `out` untouched) if `text` doesn't match one.
bool severityFromString(const std::string& text, SignalSeverity& out);

// A single Talaria-reported condition, keyed by (source, condition).
// ttlSeconds == 0 means "never expires" (the pre-TTL behavior); any other
// value is enforced by ConditionRegistry::pruneExpired(). updatedAtMs is
// bookkeeping set by the registry on every upsert -- callers shouldn't
// need to set it themselves.
struct Condition {
    std::string source;
    std::string condition;
    SignalSeverity severity = SignalSeverity::INFO;
    std::string message;
    unsigned long ttlSeconds = 0;
    unsigned long updatedAtMs = 0;
};

// Tracks every condition Talaria currently has active. Multiple
// conditions can be active at once (from different sources, or different
// condition names on the same source); the light always shows whichever
// has the highest severity, and clearing a condition reveals whatever's
// next underneath -- Talaria doesn't need to re-send the others.
//
// Bounded by maxEntries so a runaway or careless client can't grow this
// (and the device's heap) without limit -- this is a device with no
// physical access once installed, so an unbounded leak is a real outage,
// not just a slowdown.
class ConditionRegistry {
public:
    explicit ConditionRegistry(size_t maxEntries);

    // Adds a new condition, or refreshes the existing one for the same
    // (source, condition) pair -- either way, this (re)starts its TTL
    // clock from nowMs. Refreshing an existing entry is always allowed;
    // returns false (without storing anything) only if this would create
    // a brand-new entry once already at maxEntries.
    bool upsert(const Condition& next, unsigned long nowMs);

    // Removes the condition matching (source, conditionName).
    // Returns true if something was actually removed.
    bool remove(const std::string& source, const std::string& conditionName);

    void clear();

    // Removes every condition whose ttlSeconds has elapsed since its last
    // upsert. Conditions with ttlSeconds == 0 never expire this way. Call
    // this before reading (highestPriority()/all()/count()) so stale
    // conditions can't linger past their TTL.
    void pruneExpired(unsigned long nowMs);

    size_t count() const;
    size_t maxEntries() const;

    const std::vector<Condition>& all() const;

    // The currently active condition with the highest priority, or
    // nullptr if none are active. Ties keep whichever was added first.
    const Condition* highestPriority() const;

private:
    size_t maxEntries_;
    std::vector<Condition> conditions_;
};
