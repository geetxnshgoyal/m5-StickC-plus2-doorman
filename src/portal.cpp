#include "portal.h"
#include "settings.h"
#include "net.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <MD5Builder.h>

namespace {

portal::State s_state = portal::IDLE;
String s_err;
uint32_t s_logins = 0;
uint32_t s_nextCheck = 0;
uint32_t s_backoff = 5000;
bool s_online = false;

const uint32_t CHECK_OK_INTERVAL = 30000;   // while healthy
const uint32_t BACKOFF_MIN = 5000;
const uint32_t BACKOFF_MAX = 300000;

// ---------------------------------------------------------------- reachability

// Two probes run by different operators. A single probe is fragile: gstatic is
// blocked outright in some countries and DNS-hijacked on some networks, and
// when it fails we'd conclude we were permanently captive and never stop
// retrying a login that was never the problem.
struct Probe {
  const char *url;
  int code;
  const char *marker;  // optional body check, for endpoints that answer 200
};

const Probe PROBES[] = {
    {"http://connectivitycheck.gstatic.com/generate_204", 204, nullptr},
    {"http://captive.apple.com/hotspot-detect.html", 200, "Success"},
};

bool probeOnce(const Probe &p) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(4000);
  // A portal announces itself with a redirect, so following one would turn a
  // captive network into a false positive.
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(client, p.url)) return false;
  int code = http.GET();
  bool ok = (code == p.code);
  if (ok && p.marker) ok = http.getString().indexOf(p.marker) >= 0;
  http.end();
  return ok;
}

// True only when traffic is genuinely leaving the network.
bool probeInternet() {
  for (const Probe &p : PROBES) {
    if (probeOnce(p)) return true;
  }
  return false;
}

// ------------------------------------------------------------ MikroTik helpers

// MikroTik emits chap-id / chap-challenge either as hex in hidden inputs or as
// JavaScript string literals full of \xNN escapes. Normalise both to raw bytes.
String unescapeJs(const String &in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c != '\\' || i + 1 >= in.length()) {
      out += c;
      continue;
    }
    char n = in[++i];
    if (n == 'x' && i + 2 < in.length()) {
      out += (char)strtol(in.substring(i + 1, i + 3).c_str(), nullptr, 16);
      i += 2;
    } else if (n >= '0' && n <= '7') {
      size_t j = i;
      while (j < in.length() && j < i + 3 && in[j] >= '0' && in[j] <= '7') j++;
      out += (char)strtol(in.substring(i, j).c_str(), nullptr, 8);
      i = j - 1;
    } else if (n == 'n') {
      out += '\n';
    } else {
      out += n;  // \\ , \' , \" and friends
    }
  }
  return out;
}

bool isHexString(const String &s) {
  if (s.length() == 0 || s.length() % 2 != 0) return false;
  for (size_t i = 0; i < s.length(); i++) {
    if (!isxdigit((unsigned char)s[i])) return false;
  }
  return true;
}

String hexToBytes(const String &s) {
  String out;
  out.reserve(s.length() / 2);
  for (size_t i = 0; i + 1 < s.length(); i += 2) {
    out += (char)strtol(s.substring(i, i + 2).c_str(), nullptr, 16);
  }
  return out;
}

String decodeChapField(const String &raw) {
  return isHexString(raw) ? hexToBytes(raw) : unescapeJs(raw);
}

// Pulls value="..." out of the hidden input whose name matches.
bool findHiddenInput(const String &html, const char *name, String &out) {
  String needle = String("name=\"") + name + "\"";
  int at = html.indexOf(needle);
  if (at < 0) {
    needle = String("name='") + name + "'";
    at = html.indexOf(needle);
    if (at < 0) return false;
  }
  // The value attribute may sit on either side of name=, so search the whole tag.
  int tagStart = html.lastIndexOf('<', at);
  int tagEnd = html.indexOf('>', at);
  if (tagStart < 0 || tagEnd < 0) return false;
  String tag = html.substring(tagStart, tagEnd);
  int v = tag.indexOf("value=");
  if (v < 0) return false;
  char q = tag[v + 6];
  if (q != '"' && q != '\'') return false;
  int e = tag.indexOf(q, v + 7);
  if (e < 0) return false;
  out = tag.substring(v + 7, e);
  return true;
}

// Fallback: scrape the two literals out of hexMD5('<id>' + ... + '<challenge>').
bool findChapInScript(const String &html, String &id, String &challenge) {
  int at = html.indexOf("hexMD5(");
  if (at < 0) return false;
  int end = html.indexOf(')', at);
  if (end < 0) return false;
  String expr = html.substring(at + 7, end);

  String lits[4];
  int n = 0;
  for (int i = 0; i < (int)expr.length() && n < 4; i++) {
    char q = expr[i];
    if (q != '\'' && q != '"') continue;
    int e = i + 1;
    while (e < (int)expr.length()) {
      if (expr[e] == '\\') { e += 2; continue; }
      if (expr[e] == q) break;
      e++;
    }
    if (e >= (int)expr.length()) break;
    lits[n++] = expr.substring(i + 1, e);
    i = e;
  }
  if (n < 2) return false;
  id = lits[0];
  challenge = lits[n - 1];
  return true;
}

String md5Hex(const String &data) {
  MD5Builder md5;
  md5.begin();
  md5.add((const uint8_t *)data.c_str(), data.length());
  md5.calculate();
  return md5.toString();
}

String gatewayBase() {
  return String("http://") + WiFi.gatewayIP().toString();
}

// Fetches the hotspot login form. Returns false only on transport failure.
bool fetchLoginPage(String &body) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(6000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, gatewayBase() + "/login")) return false;
  int code = http.GET();
  if (code <= 0) { http.end(); return false; }
  body = http.getString();
  http.end();
  return true;
}

bool postLogin(const String &body, String &response) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, gatewayBase() + "/login")) return false;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(body);
  if (code <= 0) { http.end(); return false; }
  response = http.getString();
  http.end();
  return true;
}

bool mikrotikLogin() {
  String page;
  if (!fetchLoginPage(page)) {
    s_err = "gateway unreachable";
    return false;
  }

  String chapId, chapChallenge;
  bool haveChap = findHiddenInput(page, "chap-id", chapId) &&
                  findHiddenInput(page, "chap-challenge", chapChallenge);
  if (!haveChap) haveChap = findChapInScript(page, chapId, chapChallenge);

  String resp;
  if (haveChap) {
    // RouterOS default is login-by=http-chap: the wire carries
    // MD5(chap-id || password || challenge), never the password itself.
    String digest = md5Hex(decodeChapField(chapId) + g_cfg.portalPass +
                           decodeChapField(chapChallenge));
    String body = "username=" + settings::expand("{user}", g_cfg.portalUser, "") +
                  "&password=" + digest + "&dst=&popup=true";
    if (postLogin(body, resp)) {
      if (probeInternet()) return true;
    }
    // Do not quietly retry in cleartext. The portal offered CHAP, so the
    // password is not supposed to cross the wire at all; a gateway that simply
    // always rejects CHAP would otherwise harvest it on the second attempt.
    if (!g_cfg.allowPapFallback) {
      s_err = "CHAP rejected (cleartext retry is off)";
      return false;
    }
    s_err = "CHAP rejected";
  }

  String body = settings::expand("username={user}&password={pass}&dst=&popup=true",
                                 g_cfg.portalUser, g_cfg.portalPass);
  if (!postLogin(body, resp)) {
    s_err = "login POST failed";
    return false;
  }
  if (probeInternet()) return true;

  // RouterOS echoes the reason back in the reloaded form.
  if (resp.indexOf("invalid username or password") >= 0) s_err = "bad username/password";
  else if (resp.indexOf("session limit") >= 0)           s_err = "session limit reached";
  else if (resp.indexOf("user") >= 0 && resp.indexOf("limit") >= 0) s_err = "account limit";
  else if (s_err.length() == 0)                          s_err = "login rejected";
  return false;
}

bool genericLogin() {
  String url = g_cfg.portalUrl;
  if (url.startsWith("/")) url = gatewayBase() + url;
  if (url.length() == 0) { s_err = "no portal URL set"; return false; }

  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) { s_err = "bad portal URL"; return false; }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(settings::expand(g_cfg.portalBody, g_cfg.portalUser, g_cfg.portalPass));
  http.end();
  if (code <= 0) { s_err = "portal POST failed"; return false; }
  if (probeInternet()) return true;
  s_err = "still captive after POST";
  return false;
}

bool doLogin() {
  s_err = "";
  switch (g_cfg.portalType) {
    case PORTAL_MIKROTIK: return mikrotikLogin();
    case PORTAL_GENERIC:  return genericLogin();
    default:              s_err = "portal disabled"; return false;
  }
}

}  // namespace

void portal::begin() {
  s_state = IDLE;
  s_nextCheck = 0;
  s_backoff = BACKOFF_MIN;
}

void portal::forceLogin() {
  s_nextCheck = 0;
  s_backoff = BACKOFF_MIN;
  s_online = false;
}

bool portal::online() { return s_online; }
portal::State portal::state() { return s_state; }
String portal::lastError() { return s_err; }
uint32_t portal::loginCount() { return s_logins; }

const char *portal::stateName() {
  switch (s_state) {
    case IDLE:       return "waiting for uplink";
    case CHECKING:   return "checking";
    case LOGGING_IN: return "logging in";
    case ONLINE:     return "online";
    case FAILED:     return "login failed";
  }
  return "?";
}

void portal::tick() {
  if (!net::staConnected()) {
    s_state = IDLE;
    s_online = false;
    return;
  }
  if ((int32_t)(millis() - s_nextCheck) < 0) return;

  s_state = CHECKING;
  if (probeInternet()) {
    s_online = true;
    s_state = ONLINE;
    s_err = "";  // don't keep reporting a failure we've since recovered from
    s_backoff = BACKOFF_MIN;
    s_nextCheck = millis() + CHECK_OK_INTERVAL;
    return;
  }

  s_online = false;
  if (g_cfg.portalType == PORTAL_NONE) {
    // Monitor-only: the uplink is associated but traffic isn't getting out,
    // which on this network means the hotspot session lapsed. Say what to do
    // about it rather than just reporting a failure.
    s_state = FAILED;
    s_err = "session expired - log in via " + g_cfg.apSsid;
    s_nextCheck = millis() + CHECK_OK_INTERVAL;
    return;
  }

  s_state = LOGGING_IN;
  if (doLogin()) {
    s_logins++;
    s_online = true;
    s_state = ONLINE;
    s_err = "";
    s_backoff = BACKOFF_MIN;
    s_nextCheck = millis() + CHECK_OK_INTERVAL;
  } else {
    s_state = FAILED;
    s_nextCheck = millis() + s_backoff;
    s_backoff = min(s_backoff * 2, BACKOFF_MAX);
  }
}
