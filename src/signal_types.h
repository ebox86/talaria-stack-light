#pragma once

enum class SignalColor {
    NONE,
    RED,
    YELLOW,
    GREEN
};

enum class SignalPattern {
    OFF,
    SOLID,
    SLOW_FLASH,
    FAST_FLASH,
    ALTERNATING
};

enum class SignalPriority {
    NORMAL = 0,
    INFO = 10,
    WARNING = 20,
    CRITICAL = 30,
    EMERGENCY = 40
};

struct SignalState {
    SignalColor color;
    SignalPattern pattern;
    SignalPriority priority;
};