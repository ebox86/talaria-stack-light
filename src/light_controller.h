#pragma once

#include "signal_types.h"

class LightController {
public:
    void begin();
    void update();

    void setState(const SignalState& state);
    void allOff();

    SignalState currentState() const;

private:
    SignalState state_ {
        SignalColor::NONE,
        SignalPattern::OFF,
        SignalPriority::NORMAL
    };

    unsigned long lastToggleMs_ = 0;
    bool flashOn_ = false;

    void applyOutputs(bool enabled);
    void writeColor(SignalColor color, bool enabled);
};