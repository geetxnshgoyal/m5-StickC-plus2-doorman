#include "settings.h"
#include <Preferences.h>

Settings g_cfg;
static Preferences prefs;
static const char *NS = "router";

// Preferences logs an [E] for every key it can't find, which makes a normal
// first boot look like a fault. isKey() probes NVS directly without logging,
// so gate every read on it and fall back to the struct's default.
static String getStr(const char *key, const String &fallback) {
  return prefs.isKey(key) ? prefs.getString(key, fallback) : fallback;
}
static uint8_t getU8(const char *key, uint8_t fallback) {
  return prefs.isKey(key) ? prefs.getUChar(key, fallback) : fallback;
}

void settings::load() {
  // Read-write rather than read-only so a first boot creates the namespace
  // instead of failing nvs_open outright.
  prefs.begin(NS, false);
  g_cfg.apSsid   = getStr("apSsid", g_cfg.apSsid);
  g_cfg.apPass   = getStr("apPass", g_cfg.apPass);
  g_cfg.upSsid   = getStr("upSsid", "");
  g_cfg.upMode   = getU8("upMode", UP_PSK);
  g_cfg.upPass   = getStr("upPass", "");
  g_cfg.eapIdent = getStr("eapIdent", "");
  g_cfg.eapUser  = getStr("eapUser", "");
  g_cfg.eapPass  = getStr("eapPass", "");
  g_cfg.portalType = getU8("portalType", PORTAL_NONE);
  g_cfg.portalUser = getStr("portalUser", "");
  g_cfg.portalPass = getStr("portalPass", "");
  g_cfg.portalUrl  = getStr("portalUrl", "");
  g_cfg.portalBody = getStr("portalBody", g_cfg.portalBody);
  prefs.end();
}

void settings::save() {
  prefs.begin(NS, false);
  prefs.putString("apSsid", g_cfg.apSsid);
  prefs.putString("apPass", g_cfg.apPass);
  prefs.putString("upSsid", g_cfg.upSsid);
  prefs.putUChar("upMode", g_cfg.upMode);
  prefs.putString("upPass", g_cfg.upPass);
  prefs.putString("eapIdent", g_cfg.eapIdent);
  prefs.putString("eapUser", g_cfg.eapUser);
  prefs.putString("eapPass", g_cfg.eapPass);
  prefs.putUChar("portalType", g_cfg.portalType);
  prefs.putString("portalUser", g_cfg.portalUser);
  prefs.putString("portalPass", g_cfg.portalPass);
  prefs.putString("portalUrl", g_cfg.portalUrl);
  prefs.putString("portalBody", g_cfg.portalBody);
  prefs.end();
}

void settings::factoryReset() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}

static String urlEncode(const String &s) {
  static const char *hex = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

String settings::expand(const String &tmpl, const String &user, const String &pass) {
  String out = tmpl;
  out.replace("{user}", urlEncode(user));
  out.replace("{pass}", urlEncode(pass));
  out.replace("{ts}", String((uint32_t)millis()));
  return out;
}
