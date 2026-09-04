#include "condition_registry.h"

#include <cctype>

namespace {
    std::string toLower(const std::string& text) {
        std::string result = text;
        for (char& c : result) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }
}

SignalState signalForSeverity(SignalSeverity severity) {
    switch (severity) {
        case SignalSeverity::INFO:
            return {
                SignalColor::YELLOW,
                SignalPattern::SOLID,
                SignalPriority::INFO
            };

        case SignalSeverity::WARNING:
            return {
                SignalColor::YELLOW,
                SignalPattern::SLOW_FLASH,
                SignalPriority::WARNING
            };

        case SignalSeverity::CRITICAL:
            return {
                SignalColor::RED,
                SignalPattern::FAST_FLASH,
                SignalPriority::CRITICAL
            };

        case SignalSeverity::EMERGENCY:
            // Alternating red/yellow -- reserved for the most urgent
            // conditions, visually distinct from a plain critical flash.
            return {
                SignalColor::NONE,
                SignalPattern::ALTERNATING,
                SignalPriority::EMERGENCY
            };
    }

    return {
        SignalColor::NONE,
        SignalPattern::OFF,
        SignalPriority::NORMAL
    };
}

const char* toString(SignalSeverity severity) {
    switch (severity) {
        case SignalSeverity::INFO:      return "info";
        case SignalSeverity::WARNING:   return "warning";
        case SignalSeverity::CRITICAL:  return "critical";
        case SignalSeverity::EMERGENCY: return "emergency";
    }

    return "unknown";
}

bool severityFromString(const std::string& text, SignalSeverity& out) {
    const std::string lower = toLower(text);

    if (lower == "info")      { out = SignalSeverity::INFO;      return true; }
    if (lower == "warning")   { out = SignalSeverity::WARNING;   return true; }
    if (lower == "critical")  { out = SignalSeverity::CRITICAL;  return true; }
    if (lower == "emergency") { out = SignalSeverity::EMERGENCY; return true; }

    return false;
}

ConditionRegistry::ConditionRegistry(size_t maxEntries) : maxEntries_(maxEntries) {}

bool ConditionRegistry::upsert(const Condition& next, unsigned long nowMs) {
    for (Condition& existing : conditions_) {
        if (existing.source == next.source && existing.condition == next.condition) {
            existing = next;
            existing.updatedAtMs = nowMs;
            return true;
        }
    }

    if (conditions_.size() >= maxEntries_) {
        return false;
    }

    Condition stored = next;
    stored.updatedAtMs = nowMs;
    conditions_.push_back(stored);
    return true;
}

bool ConditionRegistry::remove(const std::string& source, const std::string& conditionName) {
    for (size_t i = 0; i < conditions_.size(); i++) {
        if (conditions_[i].source == source && conditions_[i].condition == conditionName) {
            conditions_.erase(conditions_.begin() + i);
            return true;
        }
    }

    return false;
}

void ConditionRegistry::clear() {
    conditions_.clear();
}

void ConditionRegistry::pruneExpired(unsigned long nowMs) {
    for (size_t i = 0; i < conditions_.size(); ) {
        const Condition& c = conditions_[i];

        if (c.ttlSeconds > 0 && (nowMs - c.updatedAtMs) >= c.ttlSeconds * 1000UL) {
            conditions_.erase(conditions_.begin() + i);
        } else {
            i++;
        }
    }
}

size_t ConditionRegistry::count() const {
    return conditions_.size();
}

size_t ConditionRegistry::maxEntries() const {
    return maxEntries_;
}

const std::vector<Condition>& ConditionRegistry::all() const {
    return conditions_;
}

const Condition* ConditionRegistry::highestPriority() const {
    const Condition* best = nullptr;
    int bestPriority = -1;

    for (const Condition& c : conditions_) {
        const int priority = static_cast<int>(signalForSeverity(c.severity).priority);
        if (priority > bestPriority) {
            bestPriority = priority;
            best = &c;
        }
    }

    return best;
}
