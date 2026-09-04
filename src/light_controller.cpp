#include <Arduino.h>

#include "light_controller.h"
#include "pins.h"
#include "config.h"

static void writeRelay(int pin, bool enabled) {
    const bool level = RELAY_ACTIVE_LOW ? !enabled : enabled;
    digitalWrite(pin, level ? HIGH : LOW);
}

namespace {
    // Each channel's target during one self-test phase. FLASH toggles at
    // SLOW_FLASH_MS, shared across every FLASHing channel in the phase
    // (so two FLASH channels in the same phase blink in sync, not offset).
    enum class TestChannelMode { OFF, ON, FLASH };

    struct SelfTestPhase {
        TestChannelMode red;
        TestChannelMode yellow;
        TestChannelMode green;
        unsigned long durationMs;
    };

    constexpr unsigned long SELF_TEST_SOLID_MS = 700;
    constexpr unsigned long SELF_TEST_FLASH_MS = 2000; // long enough to see it actually blink
    constexpr unsigned long SELF_TEST_GAP_MS = 300;    // clean break between phases

    using M = TestChannelMode;

    // Full diagnostic: all-on, each color alone (solid, then flashing),
    // then multi-channel combinations. The combinations matter -- a fault
    // that only shows up when channels interact (like a polarity
    // inversion, which showed every "off" channel as "on") can hide
    // behind a test that only ever drives one channel at a time.
    constexpr SelfTestPhase SELF_TEST_PHASES[] = {
        { M::ON,    M::ON,    M::ON,    1000 }, // full stack
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },

        { M::ON,    M::OFF,   M::OFF,   SELF_TEST_SOLID_MS }, // red alone
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::OFF,   M::ON,    M::OFF,   SELF_TEST_SOLID_MS }, // yellow alone
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::OFF,   M::OFF,   M::ON,    SELF_TEST_SOLID_MS }, // green alone
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },

        { M::FLASH, M::OFF,   M::OFF,   SELF_TEST_FLASH_MS }, // red flashing alone
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::OFF,   M::FLASH, M::OFF,   SELF_TEST_FLASH_MS }, // yellow flashing alone
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::OFF,   M::OFF,   M::FLASH, SELF_TEST_FLASH_MS }, // green flashing alone
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },

        { M::ON,    M::FLASH, M::OFF,   SELF_TEST_FLASH_MS }, // red solid + yellow flashing
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::FLASH, M::ON,    M::OFF,   SELF_TEST_FLASH_MS }, // red flashing + yellow solid
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::OFF,   M::FLASH, M::FLASH, SELF_TEST_FLASH_MS }, // yellow + green flashing together
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
        { M::FLASH, M::FLASH, M::FLASH, SELF_TEST_FLASH_MS }, // all three flashing together
        { M::OFF,   M::OFF,   M::OFF,   SELF_TEST_GAP_MS },
    };
    constexpr int SELF_TEST_PHASE_COUNT =
        sizeof(SELF_TEST_PHASES) / sizeof(SELF_TEST_PHASES[0]);

    unsigned long selfTestTotalDurationMs() {
        unsigned long total = 0;
        for (int i = 0; i < SELF_TEST_PHASE_COUNT; i++) {
            total += SELF_TEST_PHASES[i].durationMs;
        }
        return total;
    }

    // Same red/yellow/green sequence, reused for the boot cycle.
    constexpr SignalColor BOOT_CYCLE_STEPS[] = {
        SignalColor::RED,
        SignalColor::YELLOW,
        SignalColor::GREEN
    };
    constexpr int BOOT_CYCLE_STEP_COUNT =
        sizeof(BOOT_CYCLE_STEPS) / sizeof(BOOT_CYCLE_STEPS[0]);
}

void LightController::begin() {
    pinMode(PIN_LIGHT_RED, OUTPUT);
    pinMode(PIN_LIGHT_YELLOW, OUTPUT);
    pinMode(PIN_LIGHT_GREEN, OUTPUT);

    allOff();
}

void LightController::allOff() {
    writeRelay(PIN_LIGHT_RED, false);
    writeRelay(PIN_LIGHT_YELLOW, false);
    writeRelay(PIN_LIGHT_GREEN, false);
}

void LightController::allOn() {
    writeRelay(PIN_LIGHT_RED, true);
    writeRelay(PIN_LIGHT_YELLOW, true);
    writeRelay(PIN_LIGHT_GREEN, true);
}

void LightController::writeColor(SignalColor color, bool enabled) {
    writeRelay(PIN_LIGHT_RED, color == SignalColor::RED && enabled);
    writeRelay(PIN_LIGHT_YELLOW, color == SignalColor::YELLOW && enabled);
    writeRelay(PIN_LIGHT_GREEN, color == SignalColor::GREEN && enabled);
}

void LightController::applyOutputs(bool enabled) {
    allOff();

    if (enabled) {
        writeColor(state_.color, true);
    }
}

void LightController::setState(const SignalState& state) {
    // A real status update always wins over whatever bench test happens
    // to be running.
    bootSequenceActive_ = false;
    selfTestActive_ = false;
    allOnTestActive_ = false;
    forcedOffActive_ = false;
    colorTestActive_ = false;

    state_ = state;
    flashOn_ = true;
    lastToggleMs_ = millis();

    applyOutputs(true);
}

SignalState LightController::currentState() const {
    return state_;
}

void LightController::startSelfTest() {
    bootSequenceActive_ = false;
    allOnTestActive_ = false;

    if (!selfTestActive_) {
        preSelfTestState_ = state_;
        // Remember whether we were holding a forced-off or a single
        // color test (e.g. from /reset, /test/all-off, or clicking a
        // lamp) so it can be properly restored, not just the bare light
        // pattern -- otherwise finishing self-test drops the protection
        // and whatever the "real" computed status is (which could be
        // the loud OFFLINE alarm pattern) shows through immediately.
        preSelfTestForcedOff_ = forcedOffActive_;
        preSelfTestColorTestActive_ = colorTestActive_;
    }

    forcedOffActive_ = false;
    colorTestActive_ = false;
    selfTestActive_ = true;
    selfTestStep_ = 0;
    selfTestStepStartMs_ = millis();
    flashOn_ = true;
    lastToggleMs_ = selfTestStepStartMs_;
}

bool LightController::selfTestActive() const {
    return selfTestActive_;
}

unsigned long LightController::selfTestTotalMs() const {
    return selfTestTotalDurationMs();
}

namespace {
    bool channelOn(TestChannelMode mode, bool flashOn) {
        return mode == TestChannelMode::ON || (mode == TestChannelMode::FLASH && flashOn);
    }
}

void LightController::updateSelfTest(unsigned long now) {
    if (now - selfTestStepStartMs_ >= SELF_TEST_PHASES[selfTestStep_].durationMs) {
        selfTestStep_++;
        selfTestStepStartMs_ = now;
    }

    if (selfTestStep_ >= SELF_TEST_PHASE_COUNT) {
        selfTestActive_ = false;
        setState(preSelfTestState_);
        // setState() unconditionally clears these overrides -- put back
        // whichever one we're genuinely returning to.
        if (preSelfTestForcedOff_) {
            forcedOffActive_ = true;
        }
        if (preSelfTestColorTestActive_) {
            colorTestActive_ = true;
        }
        return;
    }

    // Shared flash clock for this run -- every FLASHing channel in the
    // current phase toggles together, in sync.
    if (now - lastToggleMs_ >= SLOW_FLASH_MS) {
        flashOn_ = !flashOn_;
        lastToggleMs_ = now;
    }

    const SelfTestPhase& phase = SELF_TEST_PHASES[selfTestStep_];
    writeRelay(PIN_LIGHT_RED, channelOn(phase.red, flashOn_));
    writeRelay(PIN_LIGHT_YELLOW, channelOn(phase.yellow, flashOn_));
    writeRelay(PIN_LIGHT_GREEN, channelOn(phase.green, flashOn_));
}

void LightController::startAllOnTest() {
    bootSequenceActive_ = false;
    selfTestActive_ = false;
    forcedOffActive_ = false;
    colorTestActive_ = false;

    allOnTestActive_ = true;
    allOn();
}

void LightController::forceOff() {
    // Route through setState() (not a bare allOff()) so this is a real,
    // persistent "off" -- otherwise whatever pattern was active before
    // just resumes on the very next update() tick, since a one-shot
    // allOff() never touches state_.
    // setState() clears forcedOffActive_ (it clears every override), so
    // set it back to true after -- that flag is what keeps this "off"
    // from being immediately re-overridden itself, e.g. by a periodic
    // heartbeat-staleness recompute right after.
    setState({ SignalColor::NONE, SignalPattern::OFF, SignalPriority::NORMAL });
    forcedOffActive_ = true;
}

void LightController::stopAllOnTest() {
    forceOff();
}

bool LightController::forcedOffActive() const {
    return forcedOffActive_;
}

void LightController::startColorTest(SignalColor color) {
    // Same pattern as forceOff(): setState() clears every override, so
    // set colorTestActive_ back to true after.
    setState({ color, SignalPattern::SOLID, SignalPriority::NORMAL });
    colorTestActive_ = true;
}

bool LightController::colorTestActive() const {
    return colorTestActive_;
}

bool LightController::allOnTestActive() const {
    return allOnTestActive_;
}

void LightController::startBootSequence() {
    selfTestActive_ = false;
    allOnTestActive_ = false;
    forcedOffActive_ = false;
    colorTestActive_ = false;

    bootSequenceActive_ = true;
    bootSequencePhase_ = BootPhase::ALL_ON;
    bootSequenceStep_ = 0;
    bootSequenceStepStartMs_ = millis();
    allOn();
}

void LightController::stopBootSequence() {
    bootSequenceActive_ = false;
}

bool LightController::bootSequenceActive() const {
    return bootSequenceActive_;
}

void LightController::updateBootSequence(unsigned long now) {
    if (bootSequencePhase_ == BootPhase::ALL_ON) {
        if (now - bootSequenceStepStartMs_ < BOOT_ALL_ON_MS) {
            allOn();
            return;
        }

        bootSequencePhase_ = BootPhase::CYCLE;
        bootSequenceStep_ = 0;
        bootSequenceStepStartMs_ = now;
    }

    // CYCLE phase: loops red -> yellow -> green indefinitely until
    // stopBootSequence() is called from outside (main.cpp, once the
    // device is on the network).
    if (now - bootSequenceStepStartMs_ >= BOOT_CYCLE_STEP_MS) {
        bootSequenceStep_ = (bootSequenceStep_ + 1) % BOOT_CYCLE_STEP_COUNT;
        bootSequenceStepStartMs_ = now;
    }

    allOff();
    writeColor(BOOT_CYCLE_STEPS[bootSequenceStep_], true);
}

void LightController::update() {
    unsigned long now = millis();

    if (bootSequenceActive_) {
        updateBootSequence(now);
        return;
    }

    if (selfTestActive_) {
        updateSelfTest(now);
        return;
    }

    if (allOnTestActive_) {
        allOn();
        return;
    }

    switch (state_.pattern) {
        case SignalPattern::OFF:
            allOff();
            break;

        case SignalPattern::SOLID:
            applyOutputs(true);
            break;

        case SignalPattern::SLOW_FLASH:
            if (now - lastToggleMs_ >= SLOW_FLASH_MS) {
                flashOn_ = !flashOn_;
                lastToggleMs_ = now;
                applyOutputs(flashOn_);
            }
            break;

        case SignalPattern::FAST_FLASH:
            if (now - lastToggleMs_ >= FAST_FLASH_MS) {
                flashOn_ = !flashOn_;
                lastToggleMs_ = now;
                applyOutputs(flashOn_);
            }
            break;

        case SignalPattern::ALTERNATING:
            // Ignores state_.color -- alternates red/yellow regardless of
            // what color was set, for the highest-urgency conditions.
            if (now - lastToggleMs_ >= FAST_FLASH_MS) {
                flashOn_ = !flashOn_;
                lastToggleMs_ = now;
            }

            allOff();
            writeColor(flashOn_ ? SignalColor::RED : SignalColor::YELLOW, true);
            break;
    }
}