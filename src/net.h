#pragma once
#include <Arduino.h>

namespace net {

struct ScanResult {
  bool found = false;
  int32_t rssi = 0;
  uint8_t channel = 0;
  String authName;      // human-readable, e.g. "WPA2-PSK" or "WPA2-Enterprise"
  bool isEnterprise = false;
};

// Brings up the private AP the Echo and bulb will join. Safe to call before
// the upstream link exists, since that's how the config page is reachable on a
// fresh board.
void startAp();

// Scans for one SSID and reports what it actually advertises. This is how you
// find out whether you're dealing with WPA2-Personal or true Enterprise.
ScanResult scanUpstream(const String &ssid);

// Kicks off an association attempt. Non-blocking; poll staConnected().
void connectUpstream();
bool staConnected();

// Turns on NAT once the upstream link is up, and hands the upstream's DNS
// server to AP clients over DHCP. Idempotent.
bool enableRouting();
bool routingActive();

uint8_t clientCount();
String staIp();
String apIp();

}  // namespace net
