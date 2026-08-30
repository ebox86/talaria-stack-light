#include <Arduino.h>
#include <ETH.h>

#include "network.h"

#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_POWER 16
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN

static bool connected = false;

static void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("[ETH] Starting");
            ETH.setHostname("talaria-stack-01");
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