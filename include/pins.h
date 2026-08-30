#pragma once

// Placeholder relay GPIOs.
// Verify against WT32-ETH01 Ethernet PHY pin usage before wiring hardware.
constexpr int PIN_LIGHT_RED = 14;
constexpr int PIN_LIGHT_YELLOW = 15;
constexpr int PIN_LIGHT_GREEN = 4;

// Many relay boards are active-low.
// Change this once you select the actual board.
constexpr bool RELAY_ACTIVE_LOW = true;