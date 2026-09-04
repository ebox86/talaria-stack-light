#pragma once

#include "signal_types.h"

enum class TalariaStatus {
    CLOSED,
    OPEN,
    WARNING,
    CRITICAL,
    OFFLINE
};

SignalState signalForStatus(TalariaStatus status);

// Human-readable names, used for JSON responses and logging.
const char* toString(TalariaStatus status);
const char* toString(SignalColor color);
const char* toString(SignalPattern pattern);
