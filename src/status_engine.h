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