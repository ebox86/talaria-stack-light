#include <Arduino.h>
#include <ETH.h>
#include <ESPmDNS.h>

#include "network.h"
#include "config.h"

#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

static bool connected = false;
static bool mdnsStarted = false;

static void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("[ETH] Starting");
            ETH.setHostname(DEVICE_NAME);
            break;

        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("[ETH] Link connected");
            break;

        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.println("[ETH] Got IP");
            Serial.print("[ETH] IP: ");
            Serial.println(ETH.localIP());

            Serial.print("[ETH] MAC: ");
            Serial.println(ETH.macAddress());

            connected = true;

            // Re-arm mDNS on every GOT_IP, not just the first one: after a
            // link bounce (cable unplugged/replugged, switch reboot) the
            // old mDNS socket may no longer be valid on the new interface
            // state, and doing nothing here would leave the hostname
            // silently unresolvable until the device itself is rebooted --
            // exactly the kind of self-inflicted "unreachable until
            // someone visits it in person" failure this device can't afford.
            if (mdnsStarted) {
                MDNS.end();
            }

            if (MDNS.begin(DEVICE_NAME)) {
                MDNS.addService("http", "tcp", 80);
                Serial.print("[mDNS] Reachable at http://");
                Serial.print(DEVICE_NAME);
                Serial.println(".local");
                mdnsStarted = true;
            } else {
                Serial.println("[mDNS] Failed to start");
                mdnsStarted = false;
            }
            break;

        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("[ETH] Disconnected");
            connected = false;
            break;

        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("[ETH] Stopped");
            connected = false;
            break;

        default:
            break;
    }
}

void setupEthernet() {
    WiFi.onEvent(WiFiEvent);

    ETH.begin(
        ETH_PHY_ADDR,
        ETH_PHY_POWER,
        ETH_PHY_MDC,
        ETH_PHY_MDIO,
        ETH_PHY_TYPE,
        ETH_CLK_MODE
    );
}

bool ethernetReady() {
    return connected;
}

const char* deviceHostname() {
    return DEVICE_NAME;
}