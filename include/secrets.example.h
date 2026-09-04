#pragma once

// Copy this file to secrets.h (same directory) and set real values before
// flashing a device you intend to actually deploy. secrets.h is
// gitignored on purpose -- real credentials should never end up in git
// history. If secrets.h doesn't exist, the build fails on a missing
// header rather than silently shipping with these placeholders, which is
// the point: you have to consciously set real values.

// Password required to push a firmware update over the network (see
// "OTA Updates" in the README). Sent as an MD5 challenge-response by the
// OTA protocol, not in cleartext -- but pick something real anyway.
constexpr char OTA_PASSWORD[] = "change-me";

// Shared secret every state-changing HTTP request must send as the
// "X-Api-Key" header (see "Authentication" in the README). GET requests
// (dashboard, /health, /status, /api/v1/signals) don't require it.
constexpr char API_KEY[] = "change-me";
