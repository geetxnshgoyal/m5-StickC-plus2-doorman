#pragma once
#include <Arduino.h>

// Upstream authentication styles. Which one a network uses is not always
// obvious from the outside, so net::scanUpstream() reports what the AP actually
// advertises rather than making you guess.
enum UpstreamMode : uint8_t {
  UP_OPEN = 0,  // no Wi-Fi password (a portal does all the gating)
  UP_PSK  = 1,  // one shared Wi-Fi password (WPA2-Personal)
  UP_PEAP = 2,  // WPA2-Enterprise, PEAP-MSCHAPv2
  UP_TTLS = 3,  // WPA2-Enterprise, EAP-TTLS-MSCHAPv2
};

struct Settings {
  // The private network your smart devices will join.
  String apSsid = "Doorman";
  String apPass = "changeme123";

  // Config page login. Blank means "reuse apPass", so there's nothing extra to
  // set up. It is a separate credential because the whole point of this device
  // is putting devices you don't trust onto that same subnet: a compromised
  // bulb should not be able to read your passwords off the config page.
  String adminPass;

  // The network to join.
  String upSsid;
  uint8_t upMode = UP_PSK;
  String upPass;     // UP_PSK only
  String eapIdent;   // outer identity; usually the same as eapUser
  String eapUser;
  String eapPass;
  // PEM of the RADIUS server's CA. Without it the supplicant will hand
  // MSCHAPv2 credentials to any access point claiming the SSID, which is the
  // standard evil-twin harvest. Blank is allowed but warned about loudly.
  String eapCa;

  // There is deliberately nothing here about captive portals. Doorman does not
  // log in to them. You sign in once yourself from any device behind it, and
  // because everything leaves through one MAC that covers the whole private
  // network. Automating it meant a pile of per-vendor guesswork, a credential
  // downgrade risk, and a device ticking terms-of-service boxes on your behalf.

  bool configured() const { return upSsid.length() > 0; }
};

extern Settings g_cfg;

namespace settings {
void load();
void save();
void factoryReset();
}  // namespace settings
