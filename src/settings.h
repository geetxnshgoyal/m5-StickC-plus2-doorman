#pragma once
#include <Arduino.h>

// Upstream authentication styles. Which one a network uses is not always
// obvious from the outside, so net::scanUpstream() reports what the AP actually
// advertises, so you don't have to guess.
enum UpstreamMode : uint8_t {
  UP_OPEN = 0,  // no Wi-Fi password (the portal does all the gating)
  UP_PSK  = 1,  // one shared Wi-Fi password (WPA2-Personal)
  UP_PEAP = 2,  // WPA2-Enterprise, PEAP-MSCHAPv2
  UP_TTLS = 3,  // WPA2-Enterprise, EAP-TTLS-MSCHAPv2
};

enum PortalType : uint8_t {
  PORTAL_NONE = 0,
  PORTAL_MIKROTIK = 1,  // RouterOS hotspot; handles both PAP and MD5-CHAP forms
  PORTAL_GENERIC = 2,   // paste your own POST target and body template
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

  // The hostel network.
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

  // The second login. Defaults to monitor-only on purpose: many portals put a
  // terms-acceptance checkbox next to the credentials, and agreeing to terms is
  // a human's job, not a firmware's. See the README.
  uint8_t portalType = PORTAL_NONE;
  String portalUser;
  String portalPass;

  // When a portal offers CHAP, failure must not silently retry in cleartext:
  // a gateway that always rejects CHAP would harvest the password. Opt in only
  // if you know your portal advertises CHAP but genuinely accepts PAP.
  bool allowPapFallback = false;

  // PORTAL_GENERIC only. Blank host in portalUrl means "the default gateway".
  // {user}, {pass} and {ts} are substituted into the body.
  String portalUrl;
  String portalBody = "username={user}&password={pass}";

  bool configured() const { return upSsid.length() > 0; }
};

extern Settings g_cfg;

namespace settings {
void load();
void save();
void factoryReset();
// Replaces {user}/{pass}/{ts} in a body template, URL-encoding the values.
String expand(const String &tmpl, const String &user, const String &pass);
}  // namespace settings
