#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

#include "ota.h"
#include "config.h"
#include "secrets.h"

namespace {
    LightController* lights = nullptr;

    void logOtaError(ota_error_t error) {
        Serial.print("[OTA] Error: ");
        switch (error) {
            case OTA_AUTH_ERROR:    Serial.println("auth failed");    break;
            case OTA_BEGIN_ERROR:   Serial.println("begin failed");   break;
            case OTA_CONNECT_ERROR: Serial.println("connect failed"); break;
            case OTA_RECEIVE_ERROR: Serial.println("receive failed"); break;
            case OTA_END_ERROR:     Serial.println("end failed");     break;
            default:                Serial.println("unknown error"); break;
        }
    }
}

void setupOta(LightController& lightController) {
    lights = &lightController;

    ArduinoOTA.setHostname(DEVICE_NAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    // network.cpp already owns mDNS (and re-arms it on every reconnect);
    // don't let ArduinoOTA run a second, competing registration.
    ArduinoOTA.setMdnsEnabled(false);

    ArduinoOTA.onStart([]() {
        const char* kind = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.print("[OTA] Update starting: ");
        Serial.println(kind);

        // Visible "busy, don't power-cycle me" indicator during the
        // flash; onEnd/onError both clear it one way or another.
        lights->startAllOnTest();
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // A full-image write can run long enough to approach the task
        // watchdog timeout on its own -- keep feeding it here so a slow
        // but healthy transfer is never force-rebooted mid-write.
        esp_task_wdt_reset();

        static unsigned int lastPercent = 101;
        const unsigned int percent = (total > 0) ? (progress * 100) / total : 0;
        if (percent != lastPercent) {
            lastPercent = percent;
            Serial.printf("[OTA] Progress: %u%%\n", percent);
        }
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] Update complete, rebooting");
    });

    ArduinoOTA.onError([](ota_error_t error) {
        logOtaError(error);
        // Don't leave the light stuck on "updating" after a failed attempt.
        lights->stopAllOnTest();
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

void handleOta() {
    ArduinoOTA.handle();
}
