#include <Arduino.h>

#include "light_controller.h"
#include "pins.h"
#include "config.h"

static void writeRelay(int pin, bool enabled) {
    const bool level = RELAY_ACTIVE_LOW ? !enabled : enabled;
    digitalWrite(pin, level ? HIGH : LOW);
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
    state_ = state;
    flashOn_ = true;
    lastToggleMs_ = millis();

    applyOutputs(true);
}

SignalState LightController::currentState() const {
    return state_;
}

void LightController::update() {
    unsigned long now = millis();

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
            // We'll implement multi-color alternating patterns next.
            break;
    }
}