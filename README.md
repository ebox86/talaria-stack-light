# Talaria Stack Light

Network-connected ESP32 stack/tower light controller for Talaria.

The device uses a **WT32-ETH01** ESP32 Ethernet board to control a three-light traffic signal that will act as a physical status indicator for Talaria.

The long-term goal is to use the signal as an operational status light for conditions such as:

* Store open / closed
* Printer problems
* Network outages
* Talaria service degradation
* Mercury integration issues
* Other warnings or critical operational events

## Hardware

Current controller:

* WT32-ETH01 ESP32 Ethernet board
* LAN8720 Ethernet PHY
* USB-to-UART adapter for flashing
* Three future relay outputs:

  * Red
  * Yellow
  * Green

Planned architecture:

```text
Talaria
   |
   | Ethernet / HTTP
   v
WT32-ETH01
   |
   v
Status Engine
   |
   v
Relay Controller
   |
   +---- Red
   +---- Yellow
   +---- Green
```

## Current Status

Hardware bring-up and the initial HTTP API are complete, and the device
is physically installed in its signal housing and running. It boots,
comes up on the network, serves the status API, and correctly drives all
three real relay outputs.

Confirmed working:

* ESP32 firmware upload
* Serial logging
* LAN8720 Ethernet initialization
* Ethernet link detection
* DHCP
* MAC address detection
* mDNS (`talaria-stack-01.local`)
* HTTP status API, plus a structured multi-condition signals API
  (`/api/v1/signals`) with priority resolution
* Talaria heartbeat timeout (auto-reverts to offline if Talaria goes quiet)
* Per-condition TTL expiry and a hard cap on active conditions
* Task watchdog and a low-heap safety reboot (see Reliability)
* API key auth on every state-changing endpoint, OTA firmware updates
* Relay/light control logic (solid, flashing, and alternating patterns)
* Boot indicator sequence (lamp test, then cycle until on the network)
* Built-in bench self-test and all-on/all-off test for the light outputs

Example boot output:

```text
[ETH] Link connected
[ETH] Got IP
[ETH] IP: 192.168.1.183
[ETH] MAC: 70:4B:CA:83:94:4F
[mDNS] Reachable at http://talaria-stack-01.local
[API] HTTP server listening on port 80
```

**Installed and live** as of 2026-09-04: relay board and lamp wiring are
bench-confirmed against real hardware, `pins.h` matches the actual
installed GPIOs (red=32, amber=33, green=14 -- not the original
placeholder 14/15/4), and `/test/cycle` cleanly lights each color on its
own with the other two off. The mismatch between placeholder and real
wiring was the root cause of a long, confusing round of "why does every
command light the wrong color" debugging -- worth remembering if this
board is ever re-wired: **update `pins.h` before re-testing**, not after.

## Project Structure

```text
talaria-stack-light/
├── include/
│   ├── config.h
│   ├── pins.h
│   ├── secrets.example.h     # copy to secrets.h and fill in real values
│   └── secrets.h             # gitignored -- OTA password + API key
├── src/
│   ├── main.cpp
│   ├── network.cpp
│   ├── network.h
│   ├── api_server.cpp        # HTTP API + built-in test dashboard
│   ├── api_server.h
│   ├── ota.cpp                # ArduinoOTA wiring
│   ├── ota.h
│   ├── signal_types.h
│   ├── status_engine.cpp     # TalariaStatus -> SignalState
│   ├── status_engine.h
│   ├── condition_registry.cpp # multi-condition tracking + priority pick
│   ├── condition_registry.h
│   ├── heartbeat_monitor.cpp  # Talaria staleness timing
│   ├── heartbeat_monitor.h
│   ├── light_controller.cpp
│   └── light_controller.h
├── test/                         # native unit tests (no hardware needed)
│   ├── test_status_engine/
│   ├── test_condition_registry/
│   └── test_heartbeat_monitor/
├── scripts/
│   └── smoke_test.ps1        # hits a running device's API end-to-end
├── api.http                  # REST Client request collection
├── platformio.ini
└── README.md
```

## Development Environment

The project uses:

* PlatformIO
* Arduino framework
* ESP32 platform

**First-time setup:** the build requires `include/secrets.h`, which is
gitignored and won't exist on a fresh clone:

```bash
cp include/secrets.example.h include/secrets.h
```

The placeholder values are enough to build and test with, but set real
ones before flashing a device you intend to actually install -- see
[Authentication](#authentication) and [OTA Updates](#ota-updates).

Build:

```bash
pio run
```

Upload:

```bash
pio run -t upload
```

Serial monitor:

```bash
pio device monitor
```

Serial speed:

```text
115200 baud
```

## WT32-ETH01 Programming Wiring

The WT32-ETH01 does not include onboard USB, so a USB-to-UART adapter is required.

Current wiring:

```text
USB-UART       WT32-ETH01
--------------------------
5V        -->  5V
GND       -->  GND
RX        -->  TX0
TX        -->  RX0
```

TX and RX must be crossed:

```text
USB TX -> WT32 RX0
USB RX -> WT32 TX0
```

### Flash Mode

To enter the ESP32 bootloader:

```text
IO0 -> GND
```

Then reset or power-cycle the board.

Recommended flashing procedure:

1. Disconnect power.
2. Connect `IO0` to `GND`.
3. Apply power.
4. If necessary, briefly connect `EN` to `GND` and release it.
5. Upload firmware.
6. Disconnect power.
7. Remove the `IO0 -> GND` jumper.
8. Apply power again.
9. Open the serial monitor.

Do not leave IO0 permanently grounded during normal operation.

## Ethernet Configuration

The WT32-ETH01 uses the LAN8720 PHY.

Current PHY configuration:

```cpp
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN
```

Ethernet currently uses DHCP.

## Boot Sequence

On power-up the device runs a visual boot indicator before showing real
status, so you can tell "still starting up" apart from "stuck":

1. **Lamp test** -- all three lights on for `BOOT_ALL_ON_MS` (default 1s).
2. **Cycle** -- red -> yellow -> green, repeating every `BOOT_CYCLE_STEP_MS`
   (default 400ms), until the device has an IP address.

Once Ethernet comes up, the cycle stops and the device shows its real
status (`OFFLINE` -- yellow fast flash -- until an API call or Talaria
sets something else). If the link never comes up, the cycle just keeps
running, which doubles as a "no network" indicator. Both durations live
in [`include/config.h`](include/config.h).

## Signal Model

The firmware separates Talaria system state from the physical light outputs.

Examples:

| State           | Light Pattern      |
| --------------- | ------------------ |
| Store Open      | Green solid        |
| Store Closed    | Red solid          |
| Warning         | Yellow slow flash  |
| Service Issue   | Yellow fast flash  |
| Critical Issue  | Red flash          |
| Network Problem | Red/yellow pattern |

The exact mappings will evolve as Talaria integration is implemented.

The goal is for Talaria to send semantic conditions such as:

```json
{
  "source": "printer-front-01",
  "condition": "PAPER_OUT",
  "severity": "warning"
}
```

rather than directly controlling GPIO pins.

## Priority Model

Multiple Talaria conditions may eventually be active simultaneously.

The stack light should display the highest-priority active condition.

Example priority levels:

```text
NORMAL      0
INFO       10
WARNING    20
CRITICAL   30
EMERGENCY  40
```

Example:

```text
Store Open
  -> Green solid

Printer runs out of paper
  -> Yellow flashing

Printer issue clears
  -> Green solid
```

The underlying store state remains active while the higher-priority alert temporarily overrides the displayed pattern. This is implemented: the displayed light is always `max(priority)` across the base status (`/status/*`) and every active condition posted to `/api/v1/signals` below -- clearing the top condition reveals whatever's next, with no need for Talaria to resend it.

## HTTP API

The device serves a plain HTTP API on port 80 (`http://talaria-stack-01.local`
via mDNS, or the DHCP IP printed on the serial monitor). Every `POST` /
`DELETE` below requires the `X-Api-Key` header -- see
[Authentication](#authentication).

```text
GET  /              Built-in test dashboard (buttons + live status)
GET  /health         { status, device, uptimeMs, ethernet, freeHeapBytes }
GET  /status          { status, color, pattern, priority, selfTest }

POST /status/open      -> green solid                       [auth]
POST /status/closed    -> red solid                         [auth]
POST /status/warning   -> yellow slow flash                 [auth]
POST /status/critical  -> red fast flash                    [auth]
POST /status/offline   -> yellow fast flash (manual test     [auth]
                           only -- normally set by the
                           device itself)

POST /test/cycle       Bench self-test: red -> yellow ->     [auth]
                        green (~700ms each), then restores
                        prior status
POST /test/all-on      Hold all three lights on              [auth]
                        until told otherwise
POST /test/all-off     Turn all lights off                   [auth]
                        (cancels /test/all-on)

POST /heartbeat        Talaria "still alive" ping --          [auth]
                        see Heartbeat below
```

All responses are JSON. Unknown routes return `404` with
`{"error": "not found"}` instead of hanging or resetting the connection.

Example:

```bash
curl -X POST http://talaria-stack-01.local/status/warning -H "X-Api-Key: $TALARIA_API_KEY"
# {"status":"warning","color":"yellow","pattern":"slow_flash","priority":20,"selfTest":false}
```

Talaria can also submit structured conditions instead of a single flat
status. Multiple conditions can be active at once, from any number of
sources -- the light always shows the highest-severity one:

```text
GET    /api/v1/signals                        { count, signals: [...] }
POST   /api/v1/signals                        Add/update a condition  [auth]
DELETE /api/v1/signals/{source}/{condition}   Clear one condition     [auth]
```

A condition is keyed by `(source, condition)` -- posting the same pair
again updates it in place rather than creating a duplicate. `severity`
must be one of `info` / `warning` / `critical` / `emergency`
(case-insensitive), mapped to a light pattern the same way `TalariaStatus`
is:

| Severity  | Priority | Light Pattern             |
| --------- | -------- | -------------------------- |
| info      | 10       | Yellow solid                |
| warning   | 20       | Yellow slow flash            |
| critical  | 30       | Red fast flash                |
| emergency | 40       | Red/yellow alternating         |

```bash
curl -X POST http://talaria-stack-01.local/api/v1/signals \
  -H "X-Api-Key: $TALARIA_API_KEY" -H "Content-Type: application/json" \
  -d '{"source":"printer-front-01","condition":"PAPER_JAM","severity":"warning","message":"Front printer is jammed"}'

curl -X DELETE http://talaria-stack-01.local/api/v1/signals/printer-front-01/PAPER_JAM \
  -H "X-Api-Key: $TALARIA_API_KEY"
```

Example: store is open (green solid, priority `NORMAL`); the printer jams
(`warning`, priority `WARNING`) and the light switches to yellow slow
flash; the printer issue is cleared with the `DELETE` call above and the
light drops straight back to green, with no further action from Talaria.

`ttlSeconds` (optional, default `0` = never expires) is enforced: if a
condition isn't refreshed by posting the same `(source, condition)` again
within that many seconds of its last update, it's dropped automatically
and the light falls through to whatever's next -- a Talaria bug that
forgets to `DELETE` a resolved condition self-heals instead of leaving a
stale light forever. Posting the same condition again resets its TTL
clock. Max `ttlSeconds` is `MAX_TTL_SECONDS` (7 days); a larger value is
rejected with `400` rather than silently overflowing the internal timer.

At most `MAX_ACTIVE_CONDITIONS` (32) distinct `(source, condition)` pairs
can be active at once -- refreshing or deleting an existing one is never
blocked by this, only creating a new entry while already full, which
returns `507` with the current limit. This exists so a careless or
malfunctioning client can't grow the device's heap without bound; there's
no physical access to recover a device that's crashed from running out of
memory.

## Heartbeat

Since Talaria might go quiet without ever calling `DELETE` (crash, lost
connectivity, etc.), the device tracks the last time it heard anything
*operational* -- a `/status/*` call, an `/api/v1/signals` POST or DELETE,
or a dedicated ping:

```bash
curl -X POST http://talaria-stack-01.local/heartbeat
```

If nothing arrives within `HEARTBEAT_TIMEOUT_MS` (default 30s, in
[`include/config.h`](include/config.h)), the display forces itself to
`OFFLINE` -- yellow fast flash -- on its own, with no incoming request
needed to trigger it. This is a *display* override, not a reset: the
underlying status and active conditions are left exactly as they were,
so the moment contact resumes (any of the calls above), the real state
reappears immediately without Talaria needing to resend anything. An
active `critical` or `emergency` condition still outranks a stale
`OFFLINE`, the same way it would outrank a real one.

Talaria should call `/heartbeat` at well under the timeout -- a third of
it is a safe rule of thumb (every ~10s for the 30s default) -- so one
missed request doesn't trip it. `GET` requests (dashboard polling,
`/health`, `/status`) never count as contact; only calls that actually
say something on Talaria's behalf do.

## Authentication

Every `POST` and `DELETE` above requires an `X-Api-Key` header matching
`API_KEY` in [`include/secrets.h`](include/secrets.h) (gitignored --
copy it from `secrets.example.h`, see [Development
Environment](#development-environment)). A missing or wrong key gets
`401`:

```json
{"error": "unauthorized", "hint": "send the shared secret as the X-Api-Key header"}
```

`GET` routes (the dashboard, `/health`, `/status`, `/api/v1/signals`)
stay open with no key -- a status light is only useful if its state is
easy to check. The built-in dashboard has its own API key field, stored
in that browser's `localStorage` and sent automatically once set.

This is a single shared secret, not per-caller credentials -- fine for
one Talaria instance talking to one device, but rotate it (edit
`secrets.h`, reflash) if it's ever exposed, and don't reuse the same key
across every device in a fleet if that's ever a thing.

## OTA Updates

Firmware can be pushed over the network instead of the physical
`IO0 -> GND` + USB-UART procedure, once the device is already running a
build that includes ArduinoOTA (i.e. this one or later):

```bash
export TALARIA_OTA_PASSWORD=<value from secrets.h's OTA_PASSWORD>
pio run -e wt32-eth01-ota -t upload
```

(PowerShell: `$env:TALARIA_OTA_PASSWORD = "..."` instead of `export`.)
This targets `talaria-stack-01.local` by default -- override
`upload_port` in [`platformio.ini`](platformio.ini) if mDNS isn't
resolving on your network. The password is a separate secret from the
API key above (`OTA_PASSWORD`, also in `secrets.h`), sent as an MD5
challenge-response by the OTA protocol rather than in cleartext.

While an update is in progress the device holds all three lights on
solid as a "don't power-cycle me" indicator, clearing automatically on
success (it reboots into the new firmware, running the normal boot
sequence) or failure (falls back to whatever it was showing before).
The very first OTA push to a device still has to happen the physical
way, since it's what gets ArduinoOTA onto the device in the first place.

## Reliability

This device has no physical access once installed -- no USB, and even
flashing it back on the bench requires the `IO0 -> GND` jumper dance
above. Everything here exists to fail *safely and recoverably* on its
own, without a hand ever touching it:

* **Watchdog** -- if `loop()` doesn't come back around within
  `WATCHDOG_TIMEOUT_S` (10s), the device assumes something's wedged (a
  hung library call, a runaway handler) and force-reboots rather than
  sitting there showing a frozen light forever.
* **Low-heap safety reboot** -- if free heap ever drops below
  `LOW_HEAP_REBOOT_THRESHOLD_BYTES` (20,000, conservative and unvalidated
  against real hardware -- see below), the device reboots itself in a
  controlled way rather than waiting for an uncontrolled allocation
  failure. `GET /health` exposes `freeHeapBytes` so this threshold can be
  tuned against real steady-state numbers once the device has run for a
  while.
* **Bounded condition storage** -- `MAX_ACTIVE_CONDITIONS` and per-signal
  TTL expiry (above) mean `/api/v1/signals` can't grow the heap without
  bound no matter how Talaria (mis)behaves.
* **mDNS re-arms itself on every reconnect** -- not just the first one --
  so a cable bounce or switch reboot can't leave the hostname silently
  unresolvable until someone visits the device in person.
* **The failure mode is itself visible** -- a crash-triggered reboot runs
  the same boot lamp-test/cycle as a cold boot, so "the device just
  restarted" is visible on the light itself, not just in a log no one is
  watching.
* **~~No OTA firmware updates~~** -- see [OTA Updates](#ota-updates). The
  single biggest gap for a device you can't physically reach is closed:
  a future fix no longer requires the `IO0 -> GND` + USB-UART procedure.
* **~~No authentication~~** -- see [Authentication](#authentication).
  Every state-changing endpoint (and OTA itself) now requires a secret
  from `include/secrets.h`, which is exactly what made turning OTA on
  safe to do at the same time.

**What's deliberately still open, in priority order if you want to keep
going:**

1. **No runtime-configurable settings.** `HEARTBEAT_TIMEOUT_MS`,
   `DEVICE_NAME`, `MAX_ACTIVE_CONDITIONS`, etc. are all compile-time
   constants -- retuning any of them today means a reflash (OTA makes
   this much less painful than it used to be, but it's still a firmware
   push for what should be a config change). A small `GET/POST /config`
   backed by `Preferences` (NVS, persists across reboots) would let you
   fix a misjudged timeout remotely without touching firmware at all.
2. **Unbounded request body size.** A very large POST body gets buffered
   into memory by the WebServer library before our code ever sees it, so
   there's no clean way to reject it early without patching the library
   or moving to an async/streaming server. Acceptable on a trusted LAN
   talking only to Talaria; revisit if this is ever reachable from
   anywhere less trusted than that.
3. **No scheduled preventive reboot.** The low-heap safety net (above)
   catches a real leak, but a cheap belt-and-suspenders addition is a
   reboot on a schedule (e.g. once a week, during a quiet window) so heap
   fragmentation from months of JSON/string churn never gets anywhere
   near that edge in the first place. Optional, and easy to add if
   wanted.
4. **The API key is a single shared secret**, not per-caller credentials
   or anything more sophisticated -- fine for one Talaria instance today,
   worth revisiting if this ever becomes a multi-device fleet with
   different trust levels.
5. **Integration note, not a code gap:** after any reboot (planned,
   crash-recovered, power-loss, or an OTA update), the device comes up in
   `OFFLINE` with no active conditions until Talaria's next update. This
   self-heals automatically as long as Talaria periodically re-sends its
   full authoritative state rather than only sending deltas on change --
   worth confirming that's how the eventual Talaria integration behaves.

## Testing

Three layers, from fastest/no-hardware to full end-to-end:

**1. Logic, no hardware required.** `status_engine`'s status-to-signal
mapping, `condition_registry`'s priority resolution (which condition
wins, what happens when it's cleared), and `heartbeat_monitor`'s staleness
timing are all pure functions, tested natively on your machine:

```bash
pio test -e native
```

Requires a host C/C++ compiler (gcc, clang, or MSVC) on `PATH`; on
Windows without one installed, install MSYS2/MinGW or run this in CI.

**2. Light/relay bench test, no Talaria/API client needed.** Once the
device is flashed and on the network, trigger the built-in self-test to
verify wiring and polarity:

```bash
curl -X POST http://talaria-stack-01.local/test/cycle
```

This cycles red -> yellow -> green (~700ms each) and then restores
whatever status was showing before, so it's safe to run any time.

To check every relay energizes at once (or measure total current draw),
hold all three lights on instead:

```bash
curl -X POST http://talaria-stack-01.local/test/all-on
curl -X POST http://talaria-stack-01.local/test/all-off   # turns them back off
```

To see the heartbeat timeout actually trip: set a status, then stop
calling anything for longer than `HEARTBEAT_TIMEOUT_MS` (30s default) --
`GET /status` alone won't reset it. The light should switch itself to the
offline pattern with no further request from you, and `"stale": true`
should appear in the next `GET /status`.

To see a condition's TTL actually expire: post one with a short
`ttlSeconds`, then leave it alone:

```bash
curl -X POST http://talaria-stack-01.local/api/v1/signals \
  -H "Content-Type: application/json" \
  -d '{"source":"test","condition":"SHORT_LIVED","severity":"warning","ttlSeconds":5}'

sleep 6
curl http://talaria-stack-01.local/api/v1/signals   # should no longer list it
```

**3. Full API smoke test.** Exercises every endpoint against a running
device:

```powershell
./scripts/smoke_test.ps1
# or: ./scripts/smoke_test.ps1 -DeviceHost 192.168.1.183
```

For interactive, one-off requests, open [`api.http`](api.http) in VS Code
with the [REST Client](https://marketplace.visualstudio.com/items?itemName=humao.rest-client)
extension and click "Send Request" above any block. The built-in
dashboard at `http://talaria-stack-01.local/` covers the same ground
from a browser, with live status, a self-test button, and a form to post
and clear `/api/v1/signals` conditions without curl.

## Roadmap

Near-term:

* ~~Add HTTP server~~
* ~~Add `/health`~~
* ~~Add `/status`~~
* ~~Add status-changing endpoints~~
* ~~Confirm GPIO output behavior against real relay hardware~~
* ~~Connect relay board~~
* ~~Bench-test red/yellow/green outputs on the physical signal~~ (installed and live, 2026-09-04)

Later:

* ~~Active condition tracking~~ (`/api/v1/signals`, multiple sources at once)
* ~~Alert priorities~~ (highest-severity active condition always wins)
* ~~Overall Talaria-heartbeat timeout~~ (`POST /heartbeat`, forces the
  display to OFFLINE if nothing's heard for `HEARTBEAT_TIMEOUT_MS`)
* ~~Per-condition TTL expiry~~ (`ttlSeconds` auto-clears a stale condition;
  see [Reliability](#reliability))
* Talaria integration
* ~~Basic device discovery~~ (mDNS hostname, re-armed on reconnect);
  fleet-wide discovery for multiple units still open
* ~~Authentication~~ (`X-Api-Key`; see [Authentication](#authentication))
* ~~OTA firmware updates~~ (see [OTA Updates](#ota-updates))
* Persistent configuration -- see [Reliability](#reliability) #1
* ~~Watchdog and failure handling~~ (task watchdog + low-heap safety
  reboot; see [Reliability](#reliability))

## Safety

The WT32-ETH01 operates at low voltage.

The eventual traffic signal may use mains-voltage lamps. The ESP32 must not directly switch mains voltage.

The final system should use properly rated relays or contactors, appropriate fusing, protected terminals, strain relief, and physical separation between low-voltage control wiring and mains-voltage wiring.
