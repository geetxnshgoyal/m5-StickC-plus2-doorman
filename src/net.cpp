#include "net.h"
#include "settings.h"

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_eap_client.h"

static bool s_routing = false;

// The access point scanUpstream() chose. Association is pinned to it, because
// otherwise the supplicant re-picks on every retry and drifts back to whichever
// radio it feels like, which on a site with one SSID on many access points
// means the distant one.
static net::ScanResult s_pick;

// True when the scan actually identified a specific radio to aim at.
static bool havePick() {
  if (!s_pick.found) return false;
  for (int i = 0; i < 6; i++)
    if (s_pick.bssid[i]) return true;
  return false;
}

void net::startAp() {
  WiFi.mode(WIFI_AP_STA);
  // max_connection 8: a bulb and an Echo leave room to spare.
  WiFi.softAP(g_cfg.apSsid.c_str(), g_cfg.apPass.c_str(), 1, 0, 8);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);

  // Deliberately not calling setTxPower. The chip already defaults to its
  // maximum, and an earlier attempt to "raise" it actually set 19.5 dBm, half
  // a dB below the default.
}

void net::followChannel(uint8_t channel) {
  if (channel < 1 || channel > 14) return;
  if (WiFi.channel() == channel) return;
  Serial.printf("[ap] moving to channel %u so the station can use the best "
                "access point\n", channel);
  WiFi.softAP(g_cfg.apSsid.c_str(), g_cfg.apPass.c_str(), channel, 0, 8);
}

static const char *authName(wifi_auth_mode_t m, bool &enterprise) {
  enterprise = false;
  switch (m) {
    case WIFI_AUTH_OPEN:            return "Open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
    case WIFI_AUTH_ENTERPRISE:      enterprise = true; return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_ENTERPRISE: enterprise = true; return "WPA3-Enterprise";
    default:                        return "Unknown";
  }
}

net::ScanResult net::scanUpstream(const String &ssid) {
  ScanResult r;
  int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  int32_t best = -127;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) != ssid) continue;
    if (WiFi.RSSI(i) <= best) continue;  // keep the strongest BSSID
    best = WiFi.RSSI(i);
    bool ent = false;
    const char *name = authName(WiFi.encryptionType(i), ent);
    r.found = true;
    r.rssi = WiFi.RSSI(i);
    r.channel = WiFi.channel(i);
    r.authName = name;
    r.isEnterprise = ent;
    if (WiFi.BSSID(i)) memcpy(r.bssid, WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();
  s_pick = r;  // connectUpstream() pins the association to this
  return r;
}

void net::connectUpstream() {
  WiFi.disconnect(false, true);
  esp_wifi_sta_enterprise_disable();

  if (g_cfg.upMode == UP_PEAP || g_cfg.upMode == UP_TTLS) {
    const String &ident = g_cfg.eapIdent.length() ? g_cfg.eapIdent : g_cfg.eapUser;
    esp_eap_client_set_identity((const uint8_t *)ident.c_str(), ident.length());
    esp_eap_client_set_username((const uint8_t *)g_cfg.eapUser.c_str(), g_cfg.eapUser.length());
    esp_eap_client_set_password((const uint8_t *)g_cfg.eapPass.c_str(), g_cfg.eapPass.length());
    if (g_cfg.upMode == UP_TTLS) {
      esp_eap_client_set_ttls_phase2_method(ESP_EAP_TTLS_PHASE2_MSCHAPV2);
    }
    // Without a CA the supplicant trusts whatever answers, so any access point
    // broadcasting this SSID can collect an MSCHAPv2 exchange and grind it
    // offline. Validate the chain whenever the user has given us a CA.
    if (g_cfg.eapCa.length()) {
      esp_eap_client_set_ca_cert((const unsigned char *)g_cfg.eapCa.c_str(),
                                 g_cfg.eapCa.length() + 1);
      // Chain is checked, but notBefore/notAfter cannot be: there's no clock
      // yet at association time, and NTP needs the association to succeed.
      esp_eap_client_set_disable_time_check(true);
    } else {
      Serial.println("[eap] WARNING: no CA certificate set. Credentials will be "
                     "offered to any AP claiming this SSID.");
      esp_eap_client_set_disable_time_check(true);
    }
    esp_wifi_sta_enterprise_enable();
  }

  // Pin to the exact access point and channel we scanned. Without the BSSID the
  // supplicant picks for itself, and a failed attempt followed by a retry is
  // enough for it to settle on a far weaker radio on another channel, dragging
  // the softAP along with it.
  const char *pass = (g_cfg.upMode == UP_PSK) ? g_cfg.upPass.c_str() : nullptr;
  if (havePick()) {
    Serial.printf("[up] pinning to %02X:%02X:%02X:%02X:%02X:%02X on ch%u (%d dBm)\n",
                  s_pick.bssid[0], s_pick.bssid[1], s_pick.bssid[2], s_pick.bssid[3],
                  s_pick.bssid[4], s_pick.bssid[5], s_pick.channel, (int)s_pick.rssi);
    WiFi.begin(g_cfg.upSsid.c_str(), pass, s_pick.channel, s_pick.bssid);
  } else {
    WiFi.begin(g_cfg.upSsid.c_str(), pass);
  }
  WiFi.setSleep(false);
}

bool net::staConnected() { return WiFi.status() == WL_CONNECTED; }
bool net::routingActive() { return s_routing; }

bool net::enableRouting() {
  if (s_routing) return true;
  if (!staConnected()) return false;

  esp_netif_t *sta = WiFi.STA.netif();
  esp_netif_t *ap = WiFi.AP.netif();
  if (!sta || !ap) return false;

  // The softAP's DHCP server offers no DNS by default, so clients would get an
  // address and then fail every lookup.
  //
  // Hand them whatever resolver the upstream network handed us. An earlier
  // version substituted the gateway here, on the theory that a captive network
  // wants to intercept lookups. That was speculation, it was never measured,
  // and the portal worked perfectly well without it.
  esp_netif_dns_info_t dns;
  if (esp_netif_get_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
    esp_netif_dhcps_stop(ap);
    uint8_t offer = 2;  // dhcps_offer_t: OFFER_DNS
    esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                           &offer, sizeof(offer));
    esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);
    esp_netif_dhcps_start(ap);
  }

  esp_err_t err = esp_netif_napt_enable(ap);
  if (err != ESP_OK) {
    log_e("NAPT enable failed: %s", esp_err_to_name(err));
    return false;
  }
  s_routing = true;
  return true;
}

uint8_t net::clientCount() { return WiFi.softAPgetStationNum(); }
String net::staIp() { return WiFi.localIP().toString(); }
String net::apIp() { return WiFi.softAPIP().toString(); }
