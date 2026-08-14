<div align="center">

# Doorman

**A Wi-Fi doorman for devices that can't sign themselves in.**

Connect Alexa, Echo, smart bulbs, printers and consoles to hotel, hostel or
campus Wi-Fi that demands a captive portal login or WPA2-Enterprise.

Your smart bulb doesn't have a browser, so it will never get past the captive
portal. Doorman turns a cheap M5StickC into the doorman that signs it in.

[![build](https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman/actions/workflows/build.yml/badge.svg)](https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman/actions/workflows/build.yml)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![platform](https://img.shields.io/badge/platform-ESP32-black.svg)](https://www.espressif.com/)

### [Install it from your browser →](https://geetxnshgoyal.github.io/m5-StickC-plus2-doorman/)

[Full install guide](INSTALL.md) &nbsp;·&nbsp;
[Download firmware](https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman/releases/latest) &nbsp;·&nbsp;
[Contributing](CONTRIBUTING.md)

</div>

---

## Does this solve your problem?

If you came here from a search, you probably have one of these. Doorman fixes
all of them the same way.

**"My Alexa / Echo Dot won't connect to hotel Wi-Fi with a login page."**
The Echo has no browser, so it can't complete the login. Put it behind Doorman
and log in once yourself.

**"My smart bulb can't join campus Wi-Fi that asks for a username and
password."**
Most bulbs only support WPA2-Personal. Doorman gives them a WPA2-Personal
network that leads out to the WPA2-Enterprise one.

**"My hostel Wi-Fi asks for a second login after the Wi-Fi password."**
That second login is a captive portal. One login through Doorman covers every
device behind it.

**"How do I connect a device with no browser to a captive portal?"**
You don't. You put something in front of it that does have a browser, and let
the device sit behind that. This is that something.

**"My Chromecast / PS5 / printer won't join a network with a web login."**
Same problem, same fix. Nothing here is Alexa specific.

**"Can I use one Wi-Fi login for several devices?"**
Behind Doorman they all share one MAC address upstream, so the portal sees a
single client. Check your network's rules first, see
[Before you deploy this](#before-you-deploy-this).

**"My IoT device only supports 2.4 GHz and my network is WPA2-Enterprise."**
Doorman handles the enterprise association (PEAP or TTLS) and hands your device
a plain 2.4 GHz WPA2-PSK network.

## The problem

Hotel, hostel and campus Wi-Fi almost never hands you a plain password. You get
a captive portal, or WPA2-Enterprise, or a portal sitting behind a shared
password. Then you try to set up a smart bulb, a speaker or a printer and find
that it supports exactly one thing: WPA2-Personal, with a password, and no
browser.

There's no setting you can change. The device simply cannot complete the login
your network asks for.

It gets worse. Most portals tie their session to a MAC address, so even if you
could log in on the device's behalf, every gadget you own would need its own
session. Plenty of accounts cap how many you get.

## The fix

Doorman joins the awkward network as a normal client, then re-broadcasts it as
a plain WPA2-PSK network with NAT. Your devices join that one instead.

```
   Wi-Fi with a captive portal / WPA2-Enterprise
                    |
                    |  station interface
             [  Doorman  ]
                    |  NAT, so everything behind here
                    |  shares ONE upstream MAC
                    |
        private WPA2-PSK network, no portal
                    |
        +-----------+-----------+
        |           |           |
      bulb       speaker     printer
```

Since everything leaves through one MAC address, one login covers every device
behind it. Sign in once from a phone joined to Doorman's network and your bulb,
speaker and printer come online with you.

They also share one flat subnet, which is what local discovery (mDNS, SSDP)
expects, so your speaker can still find your bulb.

## Who this is for

- You live somewhere with a captive portal (student housing, a hostel, a
  long-stay rental) and own things that can't deal with one.
- You travel and would rather not log in eleven separate times to get your own
  devices onto hotel Wi-Fi.
- You're on a network where you hold a valid account, and you just want the
  devices you already own to use the access you already pay for.

Not for your employer's network. See [Before you deploy this](#before-you-deploy-this).

## What this actually is

Doorman is a NAT router. That's the entire mechanism. It's the same job your
home router does, and the same job your phone does when you switch on a
personal hotspot.

It is not a bypass, and it doesn't circumvent anything:

- It doesn't crack, weaken or evade authentication. It authenticates the normal
  way, with credentials you supply.
- It doesn't defeat the captive portal. A person logs in through the portal, in
  a browser, the way the portal intends.
- It doesn't spoof, hide or rotate MAC addresses, and it doesn't disguise
  traffic. Your network sees one completely ordinary client.
- It keeps no credentials it wasn't given, and sends them nowhere except the
  network you pointed it at.

The only thing it changes is how many of your devices fit behind a single
login. That's a policy question for whoever runs the network, not a technical
circumvention.

## What you need

- An M5StickC Plus2, or an M5StickC Plus (that target compiles, but nobody has
  reported running it yet). Any ESP32 with a screen will do with small changes.
- USB power. Keep it plugged in. The internal battery is 200 mAh and the radio
  runs with power save turned off.

## Install

**Easiest:** open <https://geetxnshgoyal.github.io/m5-StickC-plus2-doorman/>,
plug the stick in, click Install. Chrome or Edge on a desktop, nothing to
download.

**Prebuilt file:** grab `doorman-stickc-plus2.factory.bin` from the
[latest release](https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman/releases/latest)
and flash it in one command:

```bash
pip install esptool
esptool write-flash 0x0 doorman-stickc-plus2.factory.bin
```

**From source:**

```bash
git clone https://github.com/geetxnshgoyal/m5-StickC-plus2-doorman && cd m5-StickC-plus2-doorman
python3 -m venv .venv && .venv/bin/pip install platformio
.venv/bin/pio run -e stickc-plus2 -t upload
```

Full instructions and troubleshooting live in [INSTALL.md](INSTALL.md).

Then:

1. Join the `Doorman` Wi-Fi network. Default password is `changeme123`.
2. Open <http://192.168.4.1>.
3. Enter the upstream network's SSID and security details. Change the AP
   password while you're in there.
4. Save. It reboots and connects.
5. If the upstream has a captive portal, open any `http://` page and log in
   once. Everything behind Doorman is covered from then on.
6. Set your smart devices up against the `Doorman` network.

## The screen

```
DOORMAN     17 min
ap   Doorman  4 cli
up   CampusWiFi -54dBm
ip   10.0.4.62
nat  active
net  online
```

- **A** forces an immediate connectivity re-check.
- **A**, held for 2s, rescans and reconnects the uplink.
- **B**, held for 3s, factory resets.

The `net` line reads `sign in: open any http:// page` when traffic isn't
getting out, so you can tell a lapsed portal session apart from a dropped
uplink at a glance.

When a portal session lapses the screen says so in plain language instead of
failing quietly.

## On automating consent

Doorman won't tick "I accept the Terms and Conditions" for you. That's on
purpose.

Lots of portals put a terms checkbox next to the login fields, and automating it
would be easy. But a box your device quietly ticks for you every day isn't
really consent, and the person bound by those terms is you, not your firmware.

So Doorman only watches the connection. When the session expires it says so, and
you sign in. One login from any device covers everything behind it. That costs
you a few seconds a day and keeps you the one who agreed to things.

That principle is why the automated login adapters were removed rather than
merely defaulted to off. See below.

## Signing in to a portal

You do it, once, from any device behind Doorman. There is no portal
configuration and nothing to set up.

Join Doorman's network from a phone or laptop and open any `http://` page. The
portal appears and you log in as normal. Because every device behind Doorman
leaves through one MAC address, that single login covers all of them until the
session expires, typically 24 hours.

The screen says `sign in: open any http:// page` whenever traffic isn't getting
out, so you can tell that case apart from a dropped uplink.

**If the portal doesn't appear**, two things usually explain it:

- **Browsers default to HTTPS**, and an HTTPS request cannot be intercepted, so
  it hangs instead of redirecting. Type a plain `http://` address.
  `http://neverssl.com` exists for exactly this.
- **Your phone has already given up on the network.** Once a device decides a
  network has no internet, it stops probing and won't offer the sign-in sheet
  again. Forget the network and rejoin it, which forces a fresh captive check
  and brings the portal straight up.

Doorman used to have adapters that logged in automatically for MikroTik and
generic form portals. They were removed. They needed per-vendor guesswork, they
risked downgrading a CHAP login to a cleartext one when they failed, and making
them work meant a device ticking terms-of-service checkboxes on your behalf.
One manual login a day is a better trade than any of that.

## Honest limits

- **Measured around 4 Mbps** on a congested 2.4 GHz channel in a hostel, on a
  network where a laptop got 33 Mbps over 5 GHz. Two reasons for the gap, and
  neither is fixable in software: the ESP32 has no 5 GHz radio at all, and it
  has one radio serving both the network it joins and the network it
  broadcasts, so every forwarded packet occupies the same channel twice.
  Expect a few Mbps, depending on how busy 2.4 GHz is around you and how good
  your signal to the upstream access point is.
- **Signal to the upstream matters enormously.** The same device measured
  0.6 Mbps associated at -66 dBm and 4 Mbps at -48 dBm. If throughput is poor,
  check what the screen reports before assuming the hardware is at fault.
- That is fine for what this is for. A smart bulb exchanges a few hundred
  bytes at a time and an Echo streams music at around 0.3 Mbps. It is not fine
  as your laptop's connection and it never will be.
- One radio, shared. The ESP32 can't run its AP and station on different
  channels. When the upstream roams you to another access point, Doorman's
  network gets dragged onto the new channel and every client drops for a few
  seconds. That's hardware, not a bug.
- 2.4 GHz only. The ESP32 has no 5 GHz radio.

See also [Before you deploy this](#before-you-deploy-this).

## Before you deploy this

Doorman is ordinary networking software, and building, publishing and using it
is lawful. What varies is whether a particular network allows what you're doing
with it. That's usually a contract question about the terms you agreed to,
rather than a criminal one.

Read your network's acceptable use policy first. The clauses that tend to
matter:

- Limits on how many devices one account may use.
- Rules against sharing an account.
- Rules against attaching your own network equipment or running an access point.

Breaking those usually means a suspended account. That's a real cost, so go in
knowing about it.

Don't put this on an employer's network, a university network, or any
organisation's network. An unregistered access point on a managed network is a
rogue AP, security teams treat that as a genuine incident, and the fallout is
disciplinary regardless of how harmless your intent was. If you want IoT devices
on a network like that, ask the people who run it. Most have a process for it,
often a device registration VLAN built for exactly this.

Use your own credentials on an account you hold. Doorman gives you no way to get
access you don't already have, and that's deliberate.

None of this is legal advice, and the rules vary by country and by contract. If
the stakes matter to you, ask someone qualified.

## Hardware notes

The Plus2 dropped the AXP192 PMU in favour of a bare power latch on G4. If G4
isn't driven high, the board cuts its own power the moment USB is unplugged.
`setup()` asserts it before anything else, so don't reuse G4.

There's no compile-time board hint for the Plus2, so M5Unified detects it at
runtime. The boot log prints what it found and warns on a mismatch, because
that's an assumption worth seeing rather than trusting.

## Layout

| File | Role |
| --- | --- |
| `src/main.cpp` | boot, phase machine, display, buttons |
| `src/net.cpp` | softAP, station connect (PSK and EAP), NAPT, DHCP DNS |
| `src/online.cpp` | reachability probe and status, no login logic |
| `src/webui.cpp` | config page on `http://192.168.4.1` |
| `src/settings.cpp` | NVS-backed config |

Credentials live in NVS. They never go into the repo, into build flags, or into
logs.

## Build notes

This uses the [pioarduino](https://github.com/pioarduino/platform-espressif32)
platform (Arduino core 3.3.11, ESP-IDF 5.5.5) rather than stock `espressif32`.
NAT (`esp_netif_napt_enable`) needs `CONFIG_LWIP_IPV4_NAPT` compiled into the
prebuilt lwIP, and the enterprise path uses IDF 5's `esp_eap_client`.

`board_build.variant` is overridden per env because the platform ships one
StickC board file and it names a variant that Arduino 3.x renamed.

## Alternatives

Worth knowing about, because sometimes one of these is the better answer:

- **A travel router.** GL.iNet and similar boxes running OpenWrt do this same
  job with far more throughput and a nicer interface, for more money. If you
  need real bandwidth behind it, buy one of those instead. Doorman exists
  because an M5StickC is cheap, pocket sized, and you might already own one.
- **Your phone's hotspot.** Fine if your phone can join Wi-Fi and tether at the
  same time. Many can't, and it drains the battery.
- **Asking whoever runs the network.** On any managed or corporate network this
  is the right answer, not this repo. Most have a process for registering
  devices.

## What's next

Doorman runs on the M5StickC Plus2 today. Support for more boards is the next
piece of work: bare ESP32 devkits, and the C3, S3 and C6. The networking is
already board agnostic, so most of the job is putting the screen, buttons and
power latch behind a small interface. See
[CONTRIBUTING.md](CONTRIBUTING.md) if you'd like to help.

The ESP8266 comes up often, so worth answering here: NAT is possible on it, but
it has no WPA2-Enterprise support at all in its SDK, so that half of the
project could never work. It would be a captive-portal-only build with much
less headroom.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

MIT. See [LICENSE](LICENSE).
