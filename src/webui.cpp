#include "webui.h"
#include "settings.h"
#include "net.h"
#include "portal.h"
#include "version.h"

#include <WiFi.h>
#include <WebServer.h>
#include <esp_random.h>

namespace {

WebServer server(80);
bool s_reboot = false;
String s_csrf;  // regenerated every boot

void makeCsrf() {
  static const char *hex = "0123456789abcdef";
  s_csrf = "";
  for (int i = 0; i < 32; i++) s_csrf += hex[esp_random() & 0xF];
}

// Every handler goes through this. Two gates:
//
//  1. Interface. WebServer binds to every interface, and with WIFI_AP_STA that
//     includes the upstream network, which we do not control. This page shows
//     and edits credentials, so it has no business answering anyone out there.
//  2. Password. Being on the private AP is not sufficient authorisation: that
//     subnet is deliberately full of IoT devices we don't trust, which is the
//     entire point of the product.
bool allowed() {
  IPAddress from = server.client().remoteIP();
  IPAddress ap = WiFi.softAPIP();
  if (from[0] != ap[0] || from[1] != ap[1] || from[2] != ap[2]) {
    server.send(403, "text/plain",
                "Doorman's settings are reachable only from its own network.\n");
    return false;
  }
  const String &pass = g_cfg.adminPass.length() ? g_cfg.adminPass : g_cfg.apPass;
  if (!server.authenticate("admin", pass.c_str())) {
    server.requestAuthentication(DIGEST_AUTH, "Doorman",
                                 "Sign in with user 'admin'.");
    return false;
  }
  return true;
}

String esc(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    // Attribute values below are single-quoted, so an apostrophe would close
    // the attribute early. An SSID or password containing one is enough to
    // break the form, and enough to inject markup into it.
    else if (c == '\'') o += "&#39;";
    else o += c;
  }
  return o;
}

String opt(const char *label, int value, int current) {
  String s = "<option value=\"" + String(value) + "\"";
  if (value == current) s += " selected";
  s += ">" + String(label) + "</option>";
  return s;
}

// Same navy-and-brass identity as the project page. No webfonts and no external
// assets: this is served from the AP side, where there is no internet.
const char *STYLE =
    "<style>:root{--ink:#070F1E;--felt:#0E2244;--line:#1C3C6E;--brass:#C9A227;"
    "--ivory:#F1EDE2;--sage:#8FA6C6}"
    "body{background-image:linear-gradient(#102A52,#070F1E 220px)}"
    "*{box-sizing:border-box}body{font:15px/1.6 ui-sans-serif,system-ui,sans-serif;"
    "max-width:620px;margin:0 auto;padding:22px;background:var(--ink);color:var(--ivory)}"
    "h1{font-size:19px;font-weight:600;letter-spacing:.24em;text-transform:uppercase;"
    "color:var(--brass);margin:0 0 18px}"
    "h1 small{letter-spacing:.08em;color:var(--sage);font-size:11px}"
    "h2{font-size:11px;letter-spacing:.18em;text-transform:uppercase;color:var(--brass);"
    "font-weight:600;margin:30px 0 10px;border-bottom:1px solid var(--line);padding-bottom:7px}"
    "h2 small{letter-spacing:.04em;text-transform:none;color:var(--sage);font-size:11px}"
    "label{display:block;margin:12px 0 4px;font-size:12.5px;color:var(--sage)}"
    "input,select,textarea{width:100%;padding:9px 10px;background:var(--felt);"
    "border:1px solid var(--line);color:var(--ivory);border-radius:3px;font-size:15px;"
    "font-family:inherit}"
    "textarea{font-family:ui-monospace,Menlo,monospace;font-size:12px;resize:vertical}"
    "input:focus,select:focus,textarea:focus{outline:2px solid var(--brass);"
    "outline-offset:1px;border-color:var(--brass)}"
    "button{margin-top:24px;padding:12px 24px;border:1px solid #7A5F16;color:#1A1405;"
    "background:linear-gradient(178deg,#E8CC72,#C9A227 55%,#B08C1C);font-weight:650;"
    "border-radius:3px;font-size:15px;cursor:pointer;font-family:inherit;letter-spacing:.02em}"
    ".s{background:var(--felt);border:1px solid var(--line);border-left:2px solid var(--brass);"
    "padding:14px 16px;border-radius:3px;font-size:13px;white-space:pre-wrap;"
    "font-family:ui-monospace,Menlo,monospace;color:var(--ivory)}"
    "small{color:var(--sage)}a{color:var(--brass);text-underline-offset:3px}</style>";

String statusBlock() {
  String s;
  s += "uplink   : " + (net::staConnected()
                            ? g_cfg.upSsid + "  " + String(WiFi.RSSI()) + " dBm  ch" +
                                  String(WiFi.channel())
                            : String("not connected")) + "\n";
  s += "ip       : " + net::staIp() + "\n";
  s += "gateway  : " + WiFi.gatewayIP().toString() + "\n";
  s += "nat      : " + String(net::routingActive() ? "active" : "off") + "\n";
  s += "portal   : " + String(portal::stateName());
  if (portal::lastError().length()) s += "  (" + portal::lastError() + ")";
  s += "\n";
  s += "logins   : " + String(portal::loginCount()) + "\n";
  s += "clients  : " + String(net::clientCount()) + "\n";
  s += "uptime   : " + String(millis() / 60000) + " min\n";
  s += "free heap: " + String(ESP.getFreeHeap() / 1024) + " KB";
  return s;
}

// Password fields are never populated with the stored secret: the page shows an
// empty box and says whether something is set. Submitting it empty keeps what's
// there. So a GET of this page discloses no credentials, even to someone who
// has already got past the two gates in allowed().
String pwField(const char *name, const String &stored, int maxlen) {
  String s = "<input type=password autocomplete=new-password name=";
  s += name;
  s += " maxlength=" + String(maxlen);
  s += " placeholder='";
  s += stored.length() ? "unchanged, type to replace" : "not set";
  s += "'>";
  return s;
}

void handleRoot() {
  if (!allowed()) return;
  String h;
  h.reserve(6000);
  h += "<!doctype html><meta charset=utf-8>";
  h += "<meta name=viewport content='width=device-width,initial-scale=1'>";
  h += "<title>" DOORMAN_NAME "</title>";
  h += STYLE;
  h += "<h1>" DOORMAN_NAME " <small>" DOORMAN_VERSION "</small></h1>";
  h += "<div class=s>" + esc(statusBlock()) + "</div>";
  // force re-login changes state, so it's a POST carrying the token, not a link
  // some other page can trigger on your behalf.
  h += "<p><a href='/scan'>scan nearby networks</a> &middot; "
       "<form method=POST action=/relogin style='display:inline'>"
       "<input type=hidden name=csrf value='" + s_csrf + "'>"
       "<button type=submit style='margin:0;padding:2px 10px;font-size:13px'>"
       "force re-login</button></form></p>";

  h += "<form method=POST action=/save>";
  h += "<input type=hidden name=csrf value='" + s_csrf + "'>";

  // maxlength on every field: NVS values are size limited, and an over-long
  // string fails to save rather than telling anyone about it.
  h += "<h2>Private AP <small>(join your smart devices to this)</small></h2>";
  h += "<label>Network name</label>"
       "<input name=apSsid maxlength=32 value='" + esc(g_cfg.apSsid) + "'>";
  h += "<label>Password <small>8 to 63 characters</small></label>" +
       pwField("apPass", g_cfg.apPass, 63);
  h += "<label>Settings password <small>for this page; blank reuses the one "
       "above</small></label>" +
       pwField("adminPass", g_cfg.adminPass, 63);

  h += "<h2>Upstream Wi-Fi <small>(the network to join)</small></h2>";
  h += "<label>Network name</label>"
       "<input name=upSsid maxlength=32 placeholder='the network to join' value='" +
       esc(g_cfg.upSsid) + "'>";
  h += "<label>Security</label><select name=upMode>";
  h += opt("Open (no Wi-Fi password)", UP_OPEN, g_cfg.upMode);
  h += opt("WPA2-Personal (one shared password)", UP_PSK, g_cfg.upMode);
  h += opt("WPA2-Enterprise PEAP-MSCHAPv2", UP_PEAP, g_cfg.upMode);
  h += opt("WPA2-Enterprise TTLS-MSCHAPv2", UP_TTLS, g_cfg.upMode);
  h += "</select>";
  h += "<label>Wi-Fi password <small>(WPA2-Personal only)</small></label>" +
       pwField("upPass", g_cfg.upPass, 63);
  h += "<label>EAP identity <small>(Enterprise only; blank = same as username)</small></label>"
       "<input name=eapIdent maxlength=64 value='" + esc(g_cfg.eapIdent) + "'>";
  h += "<label>EAP username</label>"
       "<input name=eapUser maxlength=64 value='" + esc(g_cfg.eapUser) + "'>";
  h += "<label>EAP password</label>" + pwField("eapPass", g_cfg.eapPass, 64);
  h += "<label>RADIUS CA certificate <small>(PEM; blank means any access point "
       "claiming this network can collect your credentials)</small></label>"
       "<textarea name=eapCa rows=3 maxlength=2000 placeholder='"
       "-----BEGIN CERTIFICATE-----'>" + esc(g_cfg.eapCa) + "</textarea>";

  h += "<h2>Captive portal <small>(the second login)</small></h2>";
  h += "<label>Type</label><select name=portalType>";
  h += opt("None - monitor only, you log in by hand", PORTAL_NONE, g_cfg.portalType);
  h += opt("MikroTik hotspot (RouterOS built-in form)", PORTAL_MIKROTIK, g_cfg.portalType);
  h += opt("Generic form POST", PORTAL_GENERIC, g_cfg.portalType);
  h += "</select>";
  h += "<label>Portal username</label>"
       "<input name=portalUser maxlength=64 value='" + esc(g_cfg.portalUser) + "'>";
  h += "<label>Portal password</label>" +
       pwField("portalPass", g_cfg.portalPass, 64);
  h += "<label><input type=checkbox name=papFallback value=1 style='width:auto'";
  h += g_cfg.allowPapFallback ? " checked" : "";
  h += "> Retry in cleartext if CHAP is rejected <small>(off by default: a "
       "gateway that always rejects CHAP would capture your password)</small>"
       "</label>";
  h += "<label>POST URL <small>(generic only; leading / = the gateway)</small></label>"
       "<input name=portalUrl maxlength=200 value='" + esc(g_cfg.portalUrl) + "'>";
  h += "<label>POST body <small>(generic only; {user} {pass} {ts})</small></label>"
       "<input name=portalBody maxlength=300 value='" + esc(g_cfg.portalBody) + "'>";

  h += "<button type=submit>Save &amp; reboot</button></form>";
  server.send(200, "text/html", h);
}

void handleScan() {
  if (!allowed()) return;
  int n = WiFi.scanNetworks(false, true);
  String h = "<!doctype html><meta charset=utf-8>"
             "<meta name=viewport content='width=device-width,initial-scale=1'>";
  h += String(STYLE) + "<h1>Nearby networks</h1><div class=s>";
  if (n <= 0) {
    h += "nothing found";
  } else {
    for (int i = 0; i < n; i++) {
      char line[128];
      bool ent = WiFi.encryptionType(i) == WIFI_AUTH_ENTERPRISE ||
                 WiFi.encryptionType(i) == WIFI_AUTH_WPA3_ENTERPRISE;
      snprintf(line, sizeof(line), "%-24s ch%-3d %4d dBm  %s\n",
               WiFi.SSID(i).substring(0, 24).c_str(), WiFi.channel(i), (int)WiFi.RSSI(i),
               ent ? "ENTERPRISE" : (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "PSK"));
      h += esc(String(line));
    }
  }
  WiFi.scanDelete();
  h += "</div><p><a href='/'>back</a></p>";
  server.send(200, "text/html", h);
}

String arg(const char *name, const String &fallback) {
  return server.hasArg(name) ? server.arg(name) : fallback;
}

uint8_t clampU8(long v, uint8_t hi, uint8_t fallback) {
  return (v < 0 || v > hi) ? fallback : (uint8_t)v;
}

// Blank means "leave the stored secret alone". Combined with pwField(), the
// browser is never sent a password and never has to send one back unless the
// user is actually changing it.
String argPw(const char *name, const String &current) {
  String v = server.hasArg(name) ? server.arg(name) : String();
  return v.length() ? v : current;
}

// The token is per boot and lives only in pages we served, so a form on some
// other site cannot supply it. Without this, any page you visit while on
// Doorman's network could silently reconfigure the device.
bool csrfOk() {
  if (server.hasArg("csrf") && server.arg("csrf") == s_csrf) return true;
  server.send(403, "text/plain", "Stale or missing form token. Reload the page.\n");
  return false;
}

void handleRelogin() {
  if (!allowed()) return;
  if (!csrfOk()) return;
  portal::forceLogin();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void sendError(const String &why) {
  server.send(400, "text/html",
              String("<!doctype html><meta charset=utf-8>") +
                  "<meta name=viewport content='width=device-width,initial-scale=1'>" +
                  STYLE + "<h1>" DOORMAN_NAME "</h1><h2>Not saved</h2><p>" + esc(why) +
                  "</p><p><a href='/'>Go back</a></p>");
}

void handleSave() {
  if (!allowed()) return;
  if (!csrfOk()) return;

  // Validate before touching g_cfg. A softAP with a password shorter than 8
  // characters silently refuses to start, which strands the user with no way
  // back in except a factory reset.
  String newApSsid = arg("apSsid", g_cfg.apSsid);
  String newApPass = argPw("apPass", g_cfg.apPass);
  if (newApSsid.length() < 1 || newApSsid.length() > 32) {
    return sendError("The private network name must be 1 to 32 characters.");
  }
  if (newApPass.length() < 8 || newApPass.length() > 63) {
    return sendError("The private network password must be 8 to 63 characters. "
                     "That's a WPA2 rule, not ours.");
  }

  g_cfg.apSsid = newApSsid;
  g_cfg.apPass = newApPass;
  g_cfg.adminPass = argPw("adminPass", g_cfg.adminPass);
  g_cfg.upSsid = arg("upSsid", g_cfg.upSsid);
  // Clamp rather than trust: these index switch statements, and the form is
  // not the only thing that can POST here.
  g_cfg.upMode = clampU8(arg("upMode", String(g_cfg.upMode)).toInt(), UP_TTLS, g_cfg.upMode);
  g_cfg.upPass = argPw("upPass", g_cfg.upPass);
  g_cfg.eapIdent = arg("eapIdent", g_cfg.eapIdent);
  g_cfg.eapUser = arg("eapUser", g_cfg.eapUser);
  g_cfg.eapPass = argPw("eapPass", g_cfg.eapPass);
  g_cfg.eapCa = arg("eapCa", g_cfg.eapCa);
  g_cfg.portalType =
      clampU8(arg("portalType", String(g_cfg.portalType)).toInt(), PORTAL_GENERIC,
              g_cfg.portalType);
  g_cfg.portalUser = arg("portalUser", g_cfg.portalUser);
  g_cfg.portalPass = argPw("portalPass", g_cfg.portalPass);
  // An unchecked checkbox submits nothing at all, so absence means false.
  g_cfg.allowPapFallback = server.hasArg("papFallback");
  g_cfg.portalUrl = arg("portalUrl", g_cfg.portalUrl);
  g_cfg.portalBody = arg("portalBody", g_cfg.portalBody);
  settings::save();

  server.send(200, "text/html",
              String("<!doctype html><meta charset=utf-8>") + STYLE +
                  "<h1>Saved</h1><p>Rebooting. Rejoin <b>" + esc(g_cfg.apSsid) +
                  "</b> in a few seconds.</p>");
  s_reboot = true;
}

}  // namespace

void webui::begin() {
  makeCsrf();
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", handleScan);
  server.on("/relogin", HTTP_POST, handleRelogin);
  server.onNotFound(handleRoot);
  server.begin();
}

void webui::tick() { server.handleClient(); }
bool webui::rebootRequested() { return s_reboot; }
