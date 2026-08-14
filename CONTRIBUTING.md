# Contributing

## Building

```bash
python3 -m venv .venv && .venv/bin/pip install platformio
.venv/bin/pio run -e stickc-plus2
```

Flash with `-t upload`, watch with `.venv/bin/pio device monitor`.

To measure the device's own uplink throughput, which is how you tell a slow
radio link apart from slow NAT forwarding:

```bash
PLATFORMIO_BUILD_FLAGS="-DDOORMAN_SPEEDTEST" .venv/bin/pio run -e stickc-plus2 -t upload
```

## What's most useful

**More boards.** This runs on an M5StickC Plus2. The networking in `net.cpp`,
`online.cpp`, `settings.cpp` and `webui.cpp` is board agnostic already; only
`main.cpp` depends on M5Unified for the screen, buttons and power latch.
Pulling that behind a small board interface would open up bare ESP32 devkits,
the C3, S3 and C6, and anything else with a radio. That's the single highest
value change anyone could make.

**Board reports.** The M5StickC Plus target compiles but nobody has run it on
hardware. If you have one, say whether it works.

**Throughput.** The ceiling is the prebuilt WiFi buffer counts
(`DYNAMIC_TX_BUFFER_NUM=32`, `RX_BA_WIN=6`) baked into the Arduino libs. Raising
them means rebuilding ESP-IDF. If you find a workable way to do that from
PlatformIO, that's worth a lot. Note that lwIP's TCP window is *not* the
limiter: NAPT forwards at the IP layer and never terminates TCP, so it only
affects the device's own connections.

## House rules

**Verify, don't assume.** Connectivity is confirmed with a real reachability
probe, never by trusting an HTTP 200. A captive network returning 200 while
still intercepting you is the normal case, not the edge case.

**Report failures specifically.** "no internet" helps nobody. "sign in: open any
http:// page" tells someone what to do. People using this are debugging a
network they don't control, often with only a 135 pixel screen to go on.

**Don't automate consent.** Doorman used to log in to captive portals for you.
That was removed on purpose, not merely disabled. Portals frequently sit behind
a terms-of-service checkbox, and a device that ticks it for you every day has
not obtained anyone's consent to anything. Pull requests that add automatic
portal login will be declined, however well written.

**Credentials stay put.** They live in NVS. They never go into the repo, into
logs, into build flags, or into a page served to a browser. The config page
shows password fields empty and says only whether something is set.

## Testing a change

There's no host test suite. This is firmware whose whole job is talking to
hardware and to networks that don't cooperate. Test on a real device and say in
the pull request what you actually saw, including the boot log.

Pure functions can be exercised on the host. `esc()` in `webui.cpp`, for
instance, compiles unmodified against `std::string` and can be run against XSS
payloads without a board attached.
