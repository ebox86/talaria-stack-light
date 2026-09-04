#include <Arduino.h>
#include <esp_task_wdt.h>

#include "network.h"
#include "light_controller.h"
#include "api_server.h"
#include "ota.h"
#include "config.h"

LightController lights;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Talaria Stack Light");
    Serial.println("===================");

    // Watchdog: if loop() ever stops coming back around (a wedged library
    // call, a runaway handler) within WATCHDOG_TIMEOUT_S, force-reboot
    // rather than leaving the light frozen on whatever it last showed.
    // There's no physical reset access once this is installed, so this is
    // the only thing that can recover it.
    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    lights.begin();

    // Safe startup state.
    lights.allOff();

    // Boot indicator: lamp-test all three lights, then cycle
    // red -> yellow -> green until the device is on the network.
    lights.startBootSequence();

    setupEthernet();
    setupApiServer(lights);
    setupOta(lights);
}

void loop() {
    // Once the device has a network link, drop out of the boot cycle and
    // go quiet -- not straight into the loud "no contact yet" OFFLINE
    // alarm pattern. That pattern is reserved for losing contact after
    // it was actually established (see the heartbeat monitor); a fresh
    // boot that hasn't heard from anyone yet isn't that.
    if (lights.bootSequenceActive() && ethernetReady()) {
        lights.stopBootSequence();
        lights.forceOff();
    }

    lights.update();
    handleApiServer();
    apiServerTick();
    handleOta();

    // Last-resort safety net: a controlled reboot on critically low heap
    // beats whatever undefined-behavior crash is coming otherwise.
    if (ESP.getFreeHeap() < LOW_HEAP_REBOOT_THRESHOLD_BYTES) {
        Serial.println("[SAFETY] Free heap critically low -- rebooting");
        Serial.flush();
        delay(50);
        ESP.restart();
    }

    esp_task_wdt_reset();
    delay(10);
}
