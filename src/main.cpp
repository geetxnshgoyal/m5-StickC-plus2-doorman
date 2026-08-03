// Doorman, a Wi-Fi doorman for devices that can't sign themselves in.
//
// Joins a network that gates access behind a captive portal, WPA2-Enterprise,
// or both, and re-broadcasts it as a plain WPA2-PSK network. Devices that only
// speak WPA2-PSK and have no browser (smart bulbs, speakers, printers) join
// that instead.
//
// Everything behind the NAT shares one upstream MAC, so a single portal session
// covers every device: sign in once, from anything with a browser, and the rest
// of the network comes along.

#include <M5Unified.h>
#include <WiFi.h>

#include "settings.h"
#include "net.h"
#include "portal.h"
#include "webui.h"
#include "version.h"

namespace {

enum Phase : uint8_t { PH_SETUP, PH_CONNECTING, PH_RUNNING };
Phase s_phase = PH_SETUP;

uint32_t s_connectDeadline = 0;
uint32_t s_nextDraw = 0;
uint32_t s_retryAt = 0;
net::ScanResult s_scan;

const uint32_t CONNECT_TIMEOUT = 25000;

void beginConnect() {
  s_scan = net::scanUpstream(g_cfg.upSsid);
  if (s_scan.found) {
    // A mismatch here is the single most common reason these builds fail, so
    // say it out loud rather than just timing out.
    bool cfgEnt = (g_cfg.upMode == UP_PEAP || g_cfg.upMode == UP_TTLS);
    if (s_scan.isEnterprise != cfgEnt) {
      Serial.printf("[warn] %s advertises %s but config says %s\n", g_cfg.upSsid.c_str(),
                    s_scan.authName.c_str(), cfgEnt ? "Enterprise" : "Personal");
    }
    Serial.printf("[scan] %s: %s, ch%u, %d dBm\n", g_cfg.upSsid.c_str(),
                  s_scan.authName.c_str(), s_scan.channel, (int)s_scan.rssi);
  } else {
    Serial.printf("[scan] %s not visible on 2.4 GHz\n", g_cfg.upSsid.c_str());
  }

  net::connectUpstream();
  s_connectDeadline = millis() + CONNECT_TIMEOUT;
  s_phase = PH_CONNECTING;
}

void drawLine(int y, const char *label, const String &value, uint16_t colour) {
  M5.Display.setCursor(4, y);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.print(label);
  M5.Display.setTextColor(colour, TFT_BLACK);
  M5.Display.print(value);
  M5.Display.print("        ");
}

void draw() {
  M5.Display.setTextSize(1);

  M5.Display.setCursor(4, 4);
  // Brass, matching the project's navy-and-brass identity. The status colours
  // below stay semantic (green, amber, red) because on a 135px screen that
  // reads faster than any brand palette would.
  M5.Display.setTextColor(M5.Display.color565(201, 162, 39), TFT_BLACK);
  M5.Display.print("DOORMAN");
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.printf("   %lu min   ", millis() / 60000);

  if (!g_cfg.configured()) {
    drawLine(24, "", String("NOT CONFIGURED"), TFT_YELLOW);
    drawLine(40, "join ", g_cfg.apSsid, TFT_WHITE);
    drawLine(56, "open ", String("http://") + net::apIp(), TFT_WHITE);
    return;
  }

  drawLine(24, "ap  ", g_cfg.apSsid + "  " + String(net::clientCount()) + " cli", TFT_WHITE);

  if (net::staConnected()) {
    drawLine(40, "up  ", g_cfg.upSsid + " " + String(WiFi.RSSI()) + "dBm", TFT_GREEN);
    drawLine(56, "ip  ", net::staIp(), TFT_WHITE);
  } else {
    drawLine(40, "up  ", s_phase == PH_CONNECTING ? String("connecting...")
                                                  : String("DISCONNECTED"),
             s_phase == PH_CONNECTING ? TFT_YELLOW : TFT_RED);
    drawLine(56, "ip  ", String("-"), TFT_DARKGREY);
  }

  drawLine(72, "nat ", net::routingActive() ? String("active") : String("off"),
           net::routingActive() ? TFT_GREEN : TFT_RED);

  uint16_t pc = portal::online() ? TFT_GREEN
                                 : (portal::state() == portal::FAILED ? TFT_RED : TFT_YELLOW);
  drawLine(88, "net ", String(portal::stateName()), pc);

  String note = portal::online() ? String("logins ") + portal::loginCount()
                                 : portal::lastError();
  drawLine(104, "    ", note, TFT_DARKGREY);

  drawLine(120, "", String("A=relogin  B(hold)=reset"), TFT_DARKGREY);
}

void handleButtons() {
  // wasClicked / wasHold rather than wasPressed + pressedFor: wasPressed fires
  // the moment the button goes down, so a long press would always trigger the
  // short-press action on its way to the long one. These two are exclusive.
  if (M5.BtnA.wasHold()) {
    Serial.println("[btn] reconnecting upstream");
    beginConnect();
  } else if (M5.BtnA.wasClicked()) {
    portal::forceLogin();
    Serial.println("[btn] forced re-login");
  }
  if (M5.BtnB.pressedFor(3000)) {
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setCursor(10, 50);
    M5.Display.print("FACTORY RESET");
    settings::factoryReset();
    delay(1200);
    ESP.restart();
  }
}

}  // namespace

void setup() {
  // The Plus2 dropped the AXP192 PMIC in favour of a bare power latch on G4:
  // if it isn't driven high, the board cuts its own power the moment USB is
  // removed. M5Unified asserts it too, but only after runtime autodetect has
  // identified the board, and there is no compile-time hint for the Plus2, so
  // claim it first rather than depending on that detection succeeding.
  pinMode(GPIO_NUM_4, OUTPUT);
  digitalWrite(GPIO_NUM_4, HIGH);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(1);

  // Default hold threshold is 500ms, which is easy to trip by accident on a
  // button that also has a short-press action.
  M5.BtnA.setHoldThresh(2000);

  Serial.begin(115200);
  delay(200);
  Serial.printf("\n[boot] %s %s\n", DOORMAN_NAME, DOORMAN_VERSION);

  int board = (int)M5.getBoard();
  int expect = (int)m5::board_t::board_M5StickCPlus2;
  Serial.printf("[hw] detected board id %d (expect %d = StickC Plus2)\n", board, expect);
  if (board != expect) {
    Serial.println("[hw] WARNING: board autodetect disagrees; display and power "
                   "handling may be wrong");
  }

  settings::load();
  net::startAp();
  webui::begin();
  portal::begin();

  Serial.printf("[ap] %s  ->  http://%s\n", g_cfg.apSsid.c_str(), net::apIp().c_str());

  if (g_cfg.configured()) {
    beginConnect();
  } else {
    Serial.println("[cfg] no upstream configured; join the AP and open the config page");
  }
}

void loop() {
  M5.update();
  webui::tick();
  handleButtons();

  if (webui::rebootRequested()) {
    delay(300);
    ESP.restart();
  }

  switch (s_phase) {
    case PH_SETUP:
      break;

    case PH_CONNECTING:
      if (net::staConnected()) {
        Serial.printf("[up] connected, ip %s gw %s\n", net::staIp().c_str(),
                      WiFi.gatewayIP().toString().c_str());
        if (net::enableRouting()) {
          Serial.println("[nat] routing enabled");
        } else {
          Serial.println("[nat] FAILED to enable routing");
        }
        s_phase = PH_RUNNING;
      } else if ((int32_t)(millis() - s_connectDeadline) > 0) {
        Serial.println("[up] connect timed out, retrying in 10s");
        s_phase = PH_SETUP;
        s_retryAt = millis() + 10000;
      }
      break;

    case PH_RUNNING:
      if (!net::staConnected()) {
        Serial.println("[up] link lost");
        s_phase = PH_SETUP;
        s_retryAt = millis() + 5000;
        break;
      }
      portal::tick();
      break;
  }

  if (s_phase == PH_SETUP && g_cfg.configured() && s_retryAt &&
      (int32_t)(millis() - s_retryAt) > 0) {
    s_retryAt = 0;
    beginConnect();
  }

  if ((int32_t)(millis() - s_nextDraw) > 0) {
    s_nextDraw = millis() + 1000;
    draw();
  }
}
