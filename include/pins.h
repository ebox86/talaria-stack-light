#pragma once

// Verified against the actual installed wiring (2026-09-04):
//   Red   -> GPIO32 -> Relay CH1
//   Amber -> GPIO33 -> Relay CH2
//   Green -> GPIO14 -> Relay CH3
// GPIO32/33 double as CFG/485_EN silkscreen labels on some WT32-ETH01
// boards, but neither is used by anything else in this firmware (no
// RS485), so they're free for plain digital output here.
constexpr int PIN_LIGHT_RED = 32;
constexpr int PIN_LIGHT_YELLOW = 33;
constexpr int PIN_LIGHT_GREEN = 14;

// Many relay boards are active-low, but this one isn't. Corrected
// 2026-09-04: commanding RED-only-solid was physically showing
// red=off, yellow=on, green=on -- the exact signature of every "off"
// command (driven HIGH under the active-low assumption) actually being
// this board's "on". Flipped to false; re-verify with /test/cycle after
// this deploys.
constexpr bool RELAY_ACTIVE_LOW = false;
