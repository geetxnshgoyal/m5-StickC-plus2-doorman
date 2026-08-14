#include "online.h"
#include "net.h"

#include <WiFi.h>
#include <HTTPClient.h>

namespace {

bool s_ok = false;
bool s_known = false;  // have we completed a probe since the link came up
uint32_t s_next = 0;

const uint32_t WHEN_OK = 30000;
const uint32_t WHEN_BAD = 10000;

// Two probes run by different operators. A single one is fragile: gstatic is
// blocked outright in some countries and DNS-hijacked on some networks, and
// treating that as "captive" would be wrong.
struct Probe {
  const char *url;
  int code;
  const char *marker;  // optional body check, for endpoints answering 200
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
  bool good = (code == p.code);
  if (good && p.marker) good = http.getString().indexOf(p.marker) >= 0;
  http.end();
  return good;
}

bool reachable() {
  for (const Probe &p : PROBES) {
    if (probeOnce(p)) return true;
  }
  return false;
}

}  // namespace

void online::begin() {
  s_ok = false;
  s_known = false;
  s_next = 0;
}

void online::recheck() { s_next = 0; }

bool online::ok() { return s_ok; }

const char *online::status() {
  if (!net::staConnected()) return "no uplink";
  if (!s_known) return "checking";
  return s_ok ? "online" : "sign in: open any http:// page";
}

#ifdef DOORMAN_SPEEDTEST
void online::benchmark() {
  // Plain HTTP on purpose: TLS on this chip is slow enough to dominate the
  // measurement and we want to know about the radio, not about handshakes.
  // Several hosts, because captive networks block plenty of them outright.
  const char *urls[] = {
      "http://speedtest.tele2.net/10MB.zip",
      "http://cachefly.cachefly.net/10mb.test",
      "http://speedtest.tele2.net/1MB.zip",
  };

  WiFiClient client;
  HTTPClient http;
  uint32_t t0 = 0;
  int code = 0;
  bool started = false;
  for (const char *url : urls) {
    http.setConnectTimeout(8000);
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) continue;
    t0 = millis();
    code = http.GET();
    if (code == 200) {
      Serial.printf("[speed] measuring against %s\n", url);
      started = true;
      break;
    }
    Serial.printf("[speed] %s gave %d, trying another\n", url, code);
    http.end();
  }
  if (!started) {
    Serial.println("[speed] every test host was unreachable");
    return;
  }
  WiFiClient *s = http.getStreamPtr();
  static uint8_t buf[1460];
  size_t total = 0;
  while (http.connected() && (millis() - t0) < 25000 && total < 5UL * 1024 * 1024) {
    size_t avail = s->available();
    if (avail) {
      total += s->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
    } else {
      delay(1);
    }
  }
  uint32_t ms = millis() - t0;
  http.end();
  Serial.printf("[speed] device downloaded %u bytes in %u ms = %.2f Mbps (its own "
                "link, not through NAT)\n",
                (unsigned)total, (unsigned)ms,
                ms ? (total * 8.0) / (ms * 1000.0) : 0.0);
}
#endif

void online::tick() {
  if (!net::staConnected()) {
    s_ok = false;
    s_known = false;
    return;
  }
  if ((int32_t)(millis() - s_next) < 0) return;

  bool was = s_ok, first = !s_known;
  s_ok = reachable();
  s_known = true;
  s_next = millis() + (s_ok ? WHEN_OK : WHEN_BAD);

  // Log transitions so a serial capture explains an outage without anyone
  // having to be standing in front of the screen.
  if (first || was != s_ok) {
    if (s_ok) {
      Serial.println("[net] internet reachable");
    } else {
      Serial.println("[net] no internet. The hotspot session has lapsed: join "
                     "this device's Wi-Fi and open any http:// page to sign in.");
    }
  }
}
