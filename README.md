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

Initial hardware bring-up is complete.

Confirmed working:

* ESP32 firmware upload
* Serial logging
* LAN8720 Ethernet initialization
* Ethernet link detection
* DHCP
* MAC address detection

Example boot output:

```text
[ETH] Link connected
[ETH] Got IP
[ETH] IP: 192.168.1.183
[ETH] MAC: 70:4B:CA:83:94:4F
```

## Project Structure

```text
talaria-stack-light/
├── include/
│   ├── config.h
│   └── pins.h
├── src/
│   ├── main.cpp
│   ├── network.cpp
│   ├── network.h
│   ├── signal_types.h
│   ├── status_engine.cpp
│   ├── status_engine.h
│   ├── light_controller.cpp
│   └── light_controller.h
├── test/
├── platformio.ini
└── README.md
```

## Development Environment

The project uses:

* PlatformIO
* Arduino framework
* ESP32 platform

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

The underlying store state remains active while the higher-priority alert temporarily overrides the displayed pattern.

## Planned HTTP API

Initial API direction:

```text
GET  /health
GET  /status

POST /status/open
POST /status/closed
POST /status/warning
POST /status/critical
```

Longer term, Talaria should submit structured conditions:

```text
POST /api/v1/signals
DELETE /api/v1/signals/{source}/{condition}
```

Example:

```json
{
  "source": "printer-front-01",
  "condition": "PAPER_JAM",
  "severity": "warning",
  "message": "Front printer is jammed",
  "ttlSeconds": 300
}
```

## Roadmap

Near-term:

* Add HTTP server
* Add `/health`
* Add `/status`
* Add status-changing endpoints
* Confirm GPIO output behavior
* Connect relay board
* Bench-test red/yellow/green outputs

Later:

* Active condition tracking
* Alert priorities
* TTL / stale-alert handling
* Talaria integration
* Device discovery
* Authentication
* OTA firmware updates
* Persistent configuration
* Watchdog and failure handling

## Safety

The WT32-ETH01 operates at low voltage.

The eventual traffic signal may use mains-voltage lamps. The ESP32 must not directly switch mains voltage.

The final system should use properly rated relays or contactors, appropriate fusing, protected terminals, strain relief, and physical separation between low-voltage control wiring and mains-voltage wiring.
