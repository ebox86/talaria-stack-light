#include "status_engine.h"

SignalState signalForStatus(TalariaStatus status) {
    switch (status) {
        case TalariaStatus::OPEN:
            return {
                SignalColor::GREEN,
                SignalPattern::SOLID,
                SignalPriority::NORMAL
            };

        case TalariaStatus::CLOSED:
            return {
                SignalColor::RED,
                SignalPattern::SOLID,
                SignalPriority::NORMAL
            };

        case TalariaStatus::WARNING:
            return {
                SignalColor::YELLOW,
                SignalPattern::SLOW_FLASH,
                SignalPriority::WARNING
            };

        case TalariaStatus::CRITICAL:
            return {
                SignalColor::RED,
                SignalPattern::FAST_FLASH,
                SignalPriority::CRITICAL
            };

        case TalariaStatus::OFFLINE:
            return {
                SignalColor::YELLOW,
                SignalPattern::FAST_FLASH,
                SignalPriority::CRITICAL
            };
    }

    return {
        SignalColor::NONE,
        SignalPattern::OFF,
        SignalPriority::NORMAL
    };
}

const char* toString(TalariaStatus status) {
    switch (status) {
        case TalariaStatus::OPEN:     return "open";
        case TalariaStatus::CLOSED:   return "closed";
        case TalariaStatus::WARNING:  return "warning";
        case TalariaStatus::CRITICAL: return "critical";
        case TalariaStatus::OFFLINE:  return "offline";
    }

    return "unknown";
}

const char* toString(SignalColor color) {
    switch (color) {
        case SignalColor::NONE:   return "none";
        case SignalColor::RED:    return "red";
        case SignalColor::YELLOW: return "yellow";
        case SignalColor::GREEN:  return "green";
    }

    return "unknown";
}

const char* toString(SignalPattern pattern) {
    switch (pattern) {
        case SignalPattern::OFF:         return "off";
        case SignalPattern::SOLID:       return "solid";
        case SignalPattern::SLOW_FLASH:  return "slow_flash";
        case SignalPattern::FAST_FLASH:  return "fast_flash";
        case SignalPattern::ALTERNATING: return "alternating";
    }

    return "unknown";
}
