#include "net.h"
#include "settings.h"

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_eap_client.h"

static bool s_routing = false;

void net::startAp() {
  WiFi.mode(WIFI_AP_STA);
  // max_connection 8: a bulb + an Echo leaves room to spare.
  WiFi.softAP(g_cfg.apSsid.c_str(), g_cfg.apPass.c_str(), 1, 0, 8);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
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
  }
  WiFi.scanDelete();
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
    // No CA bundle is shipped, so cert validity (which depends on a clock we
    // don't have yet at association time) must not gate the handshake.
    esp_eap_client_set_disable_time_check(true);
    esp_wifi_sta_enterprise_enable();
    WiFi.begin(g_cfg.upSsid.c_str());
  } else if (g_cfg.upMode == UP_PSK) {
    WiFi.begin(g_cfg.upSsid.c_str(), g_cfg.upPass.c_str());
  } else {
    WiFi.begin(g_cfg.upSsid.c_str());
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
  // address and then fail every lookup. Hand them whatever the hostel handed us.
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
