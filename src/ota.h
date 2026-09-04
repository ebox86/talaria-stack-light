#pragma once

#include "light_controller.h"

// Starts the ArduinoOTA service (password from include/secrets.h). Call
// once from setup(), after the light controller is ready.
void setupOta(LightController& lights);

// Pumps pending OTA activity. Call every loop() iteration.
void handleOta();
