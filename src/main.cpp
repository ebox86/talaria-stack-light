#include <Arduino.h>

#include "network.h"
#include "light_controller.h"
#include "status_engine.h"

LightController lights;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Talaria Stack Light");
    Serial.println("===================");

    lights.begin();

    // Safe startup state.
    lights.allOff();

    setupEthernet();

    // Temporary test state.
    lights.setState(
        signalForStatus(TalariaStatus::OPEN)
    );
}

void loop() {
    lights.update();

    delay(10);
}