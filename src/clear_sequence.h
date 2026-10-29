#pragma once
#include <cstdint>
#include "signal_types.h"

// One non-blocking acknowledgement, never a repeating alarm.
class ClearSequence {
public:
    static constexpr uint32_t STEP_MS = 400;
    static constexpr SignalColor colorAt(uint32_t elapsed) {
        return elapsed < STEP_MS ? SignalColor::GREEN : elapsed < 2 * STEP_MS ? SignalColor::YELLOW : elapsed < 3 * STEP_MS ? SignalColor::RED : SignalColor::NONE;
    }
    void start(uint32_t now) { started_ = now; active_ = true; }
    void cancel() { active_ = false; }
    bool active() const { return active_; }
    SignalColor update(uint32_t now) {
        if (!active_) return SignalColor::NONE;
        const SignalColor color = colorAt(uint32_t(now - started_));
        if (color == SignalColor::NONE) active_ = false;
        return color;
    }
private:
    uint32_t started_ = 0;
    bool active_ = false;
};

static_assert(ClearSequence::colorAt(0) == SignalColor::GREEN, "Starts green");
static_assert(ClearSequence::colorAt(399) == SignalColor::GREEN, "Green lasts 400 ms");
static_assert(ClearSequence::colorAt(400) == SignalColor::YELLOW, "Then yellow");
static_assert(ClearSequence::colorAt(799) == SignalColor::YELLOW, "Yellow lasts 400 ms");
static_assert(ClearSequence::colorAt(800) == SignalColor::RED, "Then red");
static_assert(ClearSequence::colorAt(1199) == SignalColor::RED, "Red lasts 400 ms");
static_assert(ClearSequence::colorAt(1200) == SignalColor::NONE, "Ends off");
static_assert(ClearSequence::colorAt(100000) == SignalColor::NONE, "Never repeats");
