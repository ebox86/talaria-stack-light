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