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
  g_cfg.apSsid    = getStr("apSsid", g_cfg.apSsid);
  g_cfg.apPass    = getStr("apPass", g_cfg.apPass);
  g_cfg.adminPass = getStr("adminPass", "");
  g_cfg.upSsid    = getStr("upSsid", "");
  g_cfg.upMode    = getU8("upMode", UP_PSK);
  g_cfg.upPass    = getStr("upPass", "");
  g_cfg.eapIdent  = getStr("eapIdent", "");
  g_cfg.eapUser   = getStr("eapUser", "");
  g_cfg.eapPass   = getStr("eapPass", "");
  g_cfg.eapCa     = getStr("eapCa", "");
  prefs.end();
}

void settings::save() {
  prefs.begin(NS, false);
  prefs.putString("apSsid", g_cfg.apSsid);
  prefs.putString("apPass", g_cfg.apPass);
  prefs.putString("adminPass", g_cfg.adminPass);
  prefs.putString("upSsid", g_cfg.upSsid);
  prefs.putUChar("upMode", g_cfg.upMode);
  prefs.putString("upPass", g_cfg.upPass);
  prefs.putString("eapIdent", g_cfg.eapIdent);
  prefs.putString("eapUser", g_cfg.eapUser);
  prefs.putString("eapPass", g_cfg.eapPass);
  prefs.putString("eapCa", g_cfg.eapCa);
  prefs.end();
}

void settings::factoryReset() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
}
