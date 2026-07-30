#include "webui.h"
#include "settings.h"
#include "net.h"
#include "portal.h"
#include "version.h"

#include <WiFi.h>
#include <WebServer.h>

namespace {

WebServer server(80);
bool s_reboot = false;

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
    "input,select{width:100%;padding:9px 10px;background:var(--felt);"
    "border:1px solid var(--line);color:var(--ivory);border-radius:3px;font-size:15px;"
    "font-family:inherit}"
    "input:focus,select:focus{outline:2px solid var(--brass);outline-offset:1px;"
    "border-color:var(--brass)}"
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

void handleRoot() {
  String h;
  h.reserve(6000);
  h += "<!doctype html><meta charset=utf-8>";
  h += "<meta name=viewport content='width=device-width,initial-scale=1'>";
  h += "<title>" DOORMAN_NAME "</title>";
  h += STYLE;
  h += "<h1>" DOORMAN_NAME " <small>" DOORMAN_VERSION "</small></h1>";
  h += "<div class=s>" + esc(statusBlock()) + "</div>";
  h += "<p><a href='/scan'>scan nearby networks</a> &middot; "
       "<a href='/relogin'>force re-login</a></p>";

  h += "<form method=POST action=/save>";

  // maxlength on every field: NVS values are size limited, and an over-long
  // string fails to save rather than telling anyone about it.
  h += "<h2>Private AP <small>(join your smart devices to this)</small></h2>";
  h += "<label>Network name</label>"
       "<input name=apSsid maxlength=32 value='" + esc(g_cfg.apSsid) + "'>";
  h += "<label>Password <small>8 to 63 characters</small></label>"
       "<input type=password name=apPass maxlength=63 value='" + esc(g_cfg.apPass) + "'>";

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
  h += "<label>Wi-Fi password <small>(WPA2-Personal only)</small></label>"
       "<input type=password name=upPass maxlength=63 value='" + esc(g_cfg.upPass) + "'>";
  h += "<label>EAP identity <small>(Enterprise only; blank = same as username)</small></label>"
       "<input name=eapIdent maxlength=64 value='" + esc(g_cfg.eapIdent) + "'>";
  h += "<label>EAP username</label>"
       "<input name=eapUser maxlength=64 value='" + esc(g_cfg.eapUser) + "'>";
  h += "<label>EAP password</label>"
       "<input type=password name=eapPass maxlength=64 value='" + esc(g_cfg.eapPass) + "'>";

  h += "<h2>Captive portal <small>(the second login)</small></h2>";
  h += "<label>Type</label><select name=portalType>";
  h += opt("None - monitor only, you log in by hand", PORTAL_NONE, g_cfg.portalType);
  h += opt("MikroTik hotspot (RouterOS built-in form)", PORTAL_MIKROTIK, g_cfg.portalType);
  h += opt("Generic form POST", PORTAL_GENERIC, g_cfg.portalType);
  h += "</select>";
  h += "<label>Portal username</label>"
       "<input name=portalUser maxlength=64 value='" + esc(g_cfg.portalUser) + "'>";
  h += "<label>Portal password</label>"
       "<input type=password name=portalPass maxlength=64 value='" +
       esc(g_cfg.portalPass) + "'>";
  h += "<label>POST URL <small>(generic only; leading / = the gateway)</small></label>"
       "<input name=portalUrl maxlength=200 value='" + esc(g_cfg.portalUrl) + "'>";
  h += "<label>POST body <small>(generic only; {user} {pass} {ts})</small></label>"
       "<input name=portalBody maxlength=300 value='" + esc(g_cfg.portalBody) + "'>";

  h += "<button type=submit>Save &amp; reboot</button></form>";
  server.send(200, "text/html", h);
}

void handleScan() {
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

void handleRelogin() {
  portal::forceLogin();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

String arg(const char *name, const String &fallback) {
  return server.hasArg(name) ? server.arg(name) : fallback;
}

void sendError(const String &why) {
  server.send(400, "text/html",
              String("<!doctype html><meta charset=utf-8>") +
                  "<meta name=viewport content='width=device-width,initial-scale=1'>" +
                  STYLE + "<h1>" DOORMAN_NAME "</h1><h2>Not saved</h2><p>" + esc(why) +
                  "</p><p><a href='/'>Go back</a></p>");
}

void handleSave() {
  // Validate before touching g_cfg. A softAP with a password shorter than 8
  // characters silently refuses to start, which strands the user with no way
  // back in except a factory reset.
  String newApSsid = arg("apSsid", g_cfg.apSsid);
  String newApPass = arg("apPass", g_cfg.apPass);
  if (newApSsid.length() < 1 || newApSsid.length() > 32) {
    return sendError("The private network name must be 1 to 32 characters.");
  }
  if (newApPass.length() < 8 || newApPass.length() > 63) {
    return sendError("The private network password must be 8 to 63 characters. "
                     "That's a WPA2 rule, not ours.");
  }

  g_cfg.apSsid = newApSsid;
  g_cfg.apPass = newApPass;
  g_cfg.upSsid = arg("upSsid", g_cfg.upSsid);
  g_cfg.upMode = (uint8_t)arg("upMode", String(g_cfg.upMode)).toInt();
  g_cfg.upPass = arg("upPass", g_cfg.upPass);
  g_cfg.eapIdent = arg("eapIdent", g_cfg.eapIdent);
  g_cfg.eapUser = arg("eapUser", g_cfg.eapUser);
  g_cfg.eapPass = arg("eapPass", g_cfg.eapPass);
  g_cfg.portalType = (uint8_t)arg("portalType", String(g_cfg.portalType)).toInt();
  g_cfg.portalUser = arg("portalUser", g_cfg.portalUser);
  g_cfg.portalPass = arg("portalPass", g_cfg.portalPass);
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
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", handleScan);
  server.on("/relogin", handleRelogin);
  server.onNotFound(handleRoot);
  server.begin();
}

void webui::tick() { server.handleClient(); }
bool webui::rebootRequested() { return s_reboot; }
