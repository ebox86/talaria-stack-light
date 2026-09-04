#pragma once

#include "signal_types.h"

class LightController {
public:
    void begin();
    void update();

    void setState(const SignalState& state);
    void allOff();
    void allOn();

    SignalState currentState() const;

    // Bench-test helper: runs a full diagnostic sequence (ignoring
    // whatever pattern is currently active) -- all-on, each color solid,
    // each color flashing alone, then multi-channel combinations -- so
    // wiring/polarity faults that only show up when channels interact
    // can't hide behind a one-at-a-time test. Non-blocking -- progresses
    // on subsequent update() calls and then restores whatever state was
    // active before the test started. See selfTestTotalMs() for how long
    // a full run takes.
    void startSelfTest();
    bool selfTestActive() const;
    unsigned long selfTestTotalMs() const;

    // Bench-test helper: turns all three lights on and holds them there
    // (e.g. to check every relay energizes, or total current draw) until
    // stopAllOnTest() turns them back off. Unlike the self-test cycle
    // above, this does not auto-revert -- it stays on until told otherwise.
    void startAllOnTest();
    void stopAllOnTest();
    bool allOnTestActive() const;

    // Forces all lights off and holds them there -- protected from being
    // silently overridden by a later status/heartbeat recompute -- until
    // the next real setState() call. This is what stopAllOnTest() uses,
    // and what the boot sequence uses once it reaches the network instead
    // of immediately showing the "no contact yet" OFFLINE alarm pattern:
    // go quiet first, and only show that alarm once contact was actually
    // established and then genuinely lost.
    void forceOff();

    // True from forceOff()/stopAllOnTest() until the next real setState()
    // call (a genuine status/condition/heartbeat update). Lets
    // api_server.cpp's refreshDisplay() know to leave "all off" alone,
    // the same way it already leaves self-test/all-on/boot alone --
    // otherwise the next recompute just re-asserts whatever the real
    // computed status is (e.g. OFFLINE from heartbeat staleness) right
    // over top of it.
    bool forcedOffActive() const;

    // Boot indicator: all lights on briefly, then cycles
    // red -> yellow -> green repeatedly until stopBootSequence() is
    // called (main.cpp calls it once the device is on the network).
    void startBootSequence();
    void stopBootSequence();
    bool bootSequenceActive() const;

private:
    SignalState state_ {
        SignalColor::NONE,
        SignalPattern::OFF,
        SignalPriority::NORMAL
    };

    unsigned long lastToggleMs_ = 0;
    bool flashOn_ = false;

    bool selfTestActive_ = false;
    int selfTestStep_ = 0;
    unsigned long selfTestStepStartMs_ = 0;
    SignalState preSelfTestState_ {
        SignalColor::NONE,
        SignalPattern::OFF,
        SignalPriority::NORMAL
    };
    bool preSelfTestForcedOff_ = false;

    bool allOnTestActive_ = false;
    bool forcedOffActive_ = false;

    enum class BootPhase { ALL_ON, CYCLE };
    bool bootSequenceActive_ = false;
    BootPhase bootSequencePhase_ = BootPhase::ALL_ON;
    int bootSequenceStep_ = 0;
    unsigned long bootSequenceStepStartMs_ = 0;

    void applyOutputs(bool enabled);
    void writeColor(SignalColor color, bool enabled);
    void updateSelfTest(unsigned long now);
    void updateBootSequence(unsigned long now);
};