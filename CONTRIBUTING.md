# Contributing

## Building

```bash
python3 -m venv .venv && .venv/bin/pip install platformio
.venv/bin/pio run -e stickc-plus2
```

Flash with `-t upload`, watch with `.venv/bin/pio device monitor`.

## What's most useful

**Portal adapters.** The interesting variety in this project is captive portals.
`PORTAL_GENERIC` covers simple form POSTs, but plenty of vendors do something
stranger. If you add one, keep it the same shape as the adapters already in
`src/portal.cpp`: detect, attempt, verify with a real reachability probe, and
report a specific error when it fails.

**Board reports.** The M5StickC Plus target compiles but nobody has run it on
hardware. If you have one, say whether it works.

**HTTPS portals.** `PORTAL_GENERIC` speaks plain HTTP only right now. Portals
hosted off-gateway are usually HTTPS and would need `WiFiClientSecure`.

## House rules

- Verify, don't assume. Every login path ends in a real reachability probe
  rather than trusting an HTTP 200. A portal that returns 200 while still
  intercepting you is the normal case, not the edge case.
- Report failures specifically. "login rejected" helps nobody. "bad
  username/password" does. People using this are debugging a network they don't
  control.
- Don't automate consent. Adapters must not tick terms-of-service checkboxes on
  the user's behalf by default. If a portal wants someone to agree to terms,
  that someone is a human. There's a longer note about this in the README.
- Credentials live in NVS. Keep them out of the repo, out of logs, and out of
  build flags.

## Testing a change

There's no host test suite. This is firmware whose whole job is talking to
hardware and to networks that don't cooperate. Test on a real device and say in
the pull request what you actually saw, including the boot log.
