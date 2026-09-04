#pragma once

#include <cstddef>
#include <cstdint>

constexpr char DEVICE_NAME[] = "talaria-stack-01";

constexpr unsigned long SLOW_FLASH_MS = 1000;
constexpr unsigned long FAST_FLASH_MS = 250;

// Boot indicator: all lights on for BOOT_ALL_ON_MS (lamp test), then
// cycles red -> yellow -> green every BOOT_CYCLE_STEP_MS until the
// device is on the network.
constexpr unsigned long BOOT_ALL_ON_MS = 1000;
constexpr unsigned long BOOT_CYCLE_STEP_MS = 400;

// If nothing operational is heard from Talaria (a status change, a
// signal post/delete, or POST /heartbeat) within this window, the
// display forces itself to OFFLINE until contact resumes. Talaria should
// heartbeat at well under this interval -- a third of it is a safe rule
// of thumb -- so a single missed request doesn't trip it.
constexpr unsigned long HEARTBEAT_TIMEOUT_MS = 30000;

// --- Embedded robustness limits ---
// This device has no physical access once installed (no USB, IO0 jumper
// needed to even reflash), so these exist to fail safely on their own
// rather than needing a hand to reset something.

// Hard cap on simultaneously-active /api/v1/signals conditions, so a
// careless or runaway client can't grow the device's heap without bound.
// Refreshing or deleting an existing condition is never blocked by this;
// only creating a brand-new (source, condition) pair beyond the cap is.
constexpr size_t MAX_ACTIVE_CONDITIONS = 32;

// Longest ttlSeconds a signal is allowed to request. Also keeps
// ttlSeconds * 1000 well clear of unsigned long overflow (~49.7 days).
constexpr unsigned long MAX_TTL_SECONDS = 7UL * 24 * 3600; // 7 days

// If loop() doesn't come back around to feed the watchdog within this
// many seconds, the device assumes something is wedged (a hung library
// call, a runaway handler) and force-reboots rather than sitting there
// showing a frozen, possibly-wrong light forever.
constexpr uint32_t WATCHDOG_TIMEOUT_S = 10;

// Last-resort safety net: if free heap ever drops this low, something is
// leaking badly enough that a controlled reboot now is safer than
// whatever crash is coming. Chosen well below normal steady-state free
// heap (see ESP.getFreeHeap() on GET /health) but with enough margin to
// still allocate the reboot path's own bookkeeping.
constexpr uint32_t LOW_HEAP_REBOOT_THRESHOLD_BYTES = 20000;