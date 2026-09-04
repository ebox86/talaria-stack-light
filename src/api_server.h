#pragma once

#include "light_controller.h"

// Starts the HTTP API + test dashboard. Call once from setup(), after
// lights.begin() -- it needs a LightController to drive.
void setupApiServer(LightController& lights);

// Pumps pending HTTP requests. Call every loop() iteration.
void handleApiServer();

// Periodic housekeeping that doesn't depend on an incoming request --
// currently just re-checking Talaria heartbeat staleness, since the
// display needs to drop to OFFLINE on a timeout even with no new HTTP
// call to trigger it. Call every loop() iteration, after handleApiServer().
void apiServerTick();
