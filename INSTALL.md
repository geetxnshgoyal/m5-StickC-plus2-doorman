# Installing Doorman

Three ways in, easiest first. You only need one.

---

## 1. Flash from your browser (easiest)

Go to **<https://geetxnshgoyal.github.io/m5-StickC-plus2-doorman/>**, plug the
stick into your computer with a USB-C cable, and click Install.

Needs Chrome or Edge on a desktop, because Safari and Firefox don't support Web
Serial. Nothing to download, no toolchain, no command line.

If the install button says no device was found, see
[Troubleshooting](#troubleshooting) below.

---

## 2. Flash a prebuilt file (no compiler needed)

Grab the right file from the
[latest release](https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman/releases/latest):

| Your board | File |
| --- | --- |
| M5StickC Plus2 | `doorman-stickc-plus2.factory.bin` |
| M5StickC Plus | `doorman-stickc-plus.factory.bin` |

Then install `esptool` and write it:

```bash
pip install esptool
esptool write-flash 0x0 doorman-stickc-plus2.factory.bin
```

These are merged images, so the whole thing goes to offset `0x0` in one
command. `esptool` finds the port on its own. If you have several boards
plugged in, add `--port /dev/cu.usbserial-XXXX` on macOS or Linux, or
`--port COM5` on Windows.

---

## 3. Build it yourself

For changing the code, or for boards other than the two above.

```bash
git clone https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman
cd m5-StickC-plus2-doorman
python3 -m venv .venv && .venv/bin/pip install platformio
.venv/bin/pio run -e stickc-plus2 -t upload
```

Swap `stickc-plus2` for `stickc-plus` if that's your board. Watch the boot log
with `.venv/bin/pio device monitor`.

See [CONTRIBUTING.md](CONTRIBUTING.md) if you plan to change something.

---

## After flashing

1. The screen shows `DOORMAN` and `NOT CONFIGURED`.
2. Join the **`Doorman`** Wi-Fi network from a phone or laptop. Default password
   is `changeme123`.
3. Open **<http://192.168.4.1>**.
4. Enter the SSID and password of the network you want it to join. Change the AP
   password while you're in there.
5. Save. It reboots and connects.
6. If that network has a captive portal, open any `http://` page and log in
   once. Everything behind Doorman is covered from then on.
7. Set your smart devices up against the `Doorman` network.

Keep it plugged into USB. The internal battery is 200 mAh and won't last.

---

## Troubleshooting

**The browser installer doesn't see the stick.**
Use Chrome or Edge on a desktop. Try a different USB-C cable, since plenty of
cheap ones carry power but no data. On macOS and Windows the M5StickC needs the
CH9102 driver, available from
[WCH](https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html).

**Nothing on the screen after flashing.**
Hold the power button (the one on the left side, below the red LED) for two
seconds. If it's on USB it should stay on by itself.

**It says `NOT CONFIGURED` and the AP won't appear.**
Give it about ten seconds after boot. If the `Doorman` network still isn't
listed, reflash and watch the boot log for the detected board id.

**It connects but there's no internet.**
That's normal until you log into the captive portal. Join `Doorman`, open any
`http://` page, and the portal should appear. The screen says
`session expired` when this is what's happening.

**It worked and then stopped after a day.**
Portal sessions expire, often every 24 hours. Log in again the same way. Doorman
deliberately doesn't automate this, and the README explains why.

**Everything drops for a few seconds now and then.**
The ESP32 shares one radio between joining your network and broadcasting its
own. When your network moves you to a different access point, Doorman has to
follow, and clients reconnect. That's hardware, not a bug.

**I want to start over.**
Hold button B for three seconds to factory reset.
