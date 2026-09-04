#pragma once

void setupEthernet();
bool ethernetReady();

// Device hostname advertised over mDNS once the link comes up
// (e.g. "talaria-stack-01.local"), so the API can be reached without
// reading the DHCP-assigned IP off the serial monitor each time.
const char* deviceHostname();