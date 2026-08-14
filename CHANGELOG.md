# Changelog

Notable changes to Doorman. Newest first.

## 1.2.0 (2026-08-14)

The release where automatic portal login was removed and the throughput bug was
found.

### Fixed

- **The station drifted onto weak access points, costing most of the
  throughput.** On a site that broadcasts one SSID from many access points, the
  supplicant chose for itself on every attempt, and a single failed association
  was enough for it to settle on a distant radio on another channel. Because
  one radio serves both interfaces, the softAP was then dragged along with it.
  Association is now pinned to the specific BSSID and channel that the scan
  identified as strongest, and the softAP is moved to that channel first.

  Measured on a hostel network with several access points sharing an SSID:
  **0.61 Mbps associated at -66 dBm on channel 1, and 4.1 Mbps at -48 dBm on
  channel 6** after pinning. Moving the softAP alone was not enough, since the
  station simply wandered back; the BSSID pin is what actually fixed it.
- AP clients are handed the upstream gateway as their DNS server rather than
  the resolver it advertised. On a captive network the gateway is the thing
  that intercepts lookups and redirects them at the login page, which is how a
  portal appears by itself.

### Removed

- **Automatic captive portal login, in its entirety.** The MikroTik and generic
  adapters, all portal configuration, and the credential storage that went with
  them are gone. They needed per-vendor guesswork, they never once succeeded on
  the network they were written against, and making them work in general meant
  a device ticking terms-of-service checkboxes on someone's behalf. You sign in
  once yourself from any device behind Doorman, and because everything leaves
  through one MAC address that covers the whole private network.
- `portal.cpp` and `portal.h`, replaced by `online.cpp`, which only reports
  whether traffic is getting out.

### Changed

- The screen says `sign in: open any http:// page` when traffic isn't getting
  out, so a lapsed portal session is distinguishable from a dropped uplink.
- Transmit power raised to maximum. Throughput on this hardware is largely a
  function of link margin.
- softAP client limit lowered from 8 to 4. Each association reserves buffers
  from a small fixed pool.
- Build debug level lowered from 3 to 2. Warnings and disconnect reason codes
  are kept, since they are genuinely useful when a network refuses you; the
  chatty info logs are not.
- Serial now logs internet up and down transitions, so an outage is
  diagnosable without standing in front of the screen.

### Added

- `-DDOORMAN_SPEEDTEST` build flag, which measures the device's own download
  throughput. This is how you tell a slow radio link apart from slow NAT
  forwarding when a client behind the access point reports poor speed.

## 1.1.1 (unreleased)

### Fixed

- **Signing in to the config page was impossible.** 1.1.0 used digest
  authentication, and Arduino's `WebServer` regenerates its nonce and opaque on
  every `requestAuthentication()` call while requiring them to match on the way
  back. Any extra request from the browser invalidated the credentials it was
  about to send, so the login looped forever. Switched to basic authentication,
  which is appropriate here because the page is reachable only over the
  device's own WPA2 network.

## 1.1.0 (unreleased)

Security hardening, after an audit of the whole attack surface.

### Fixed

- **The config page answered on every interface with no authentication.** The
  web server binds to all interfaces and the device runs as station and access
  point at once, so the settings page was reachable from the upstream network.
  Every handler now rejects clients outside the softAP subnet and requires a
  password. Client isolation on the test network had been masking this.
- **Stored passwords were rendered into the page.** Marking the fields
  `type=password` hid them visually while view-source returned them in
  cleartext. Password fields are now served empty and report only whether
  something is set; submitting one blank keeps the stored value.
- **A failed CHAP login retried in cleartext.** When a portal offers CHAP the
  password is not supposed to cross the wire at all, so a gateway that simply
  always rejected CHAP would harvest it on the second attempt. Made opt-in,
  defaulting to off. (Moot as of 1.2.0, which removed portal login entirely.)
- No CSRF protection. Both state-changing routes are now POST and carry a
  per-boot token; `force re-login` stopped being a link that any page could
  trigger.
- WPA2-Enterprise offered credentials to any access point claiming the SSID.
  Added a RADIUS CA certificate field; the chain is validated when one is set,
  and the boot log warns loudly when one is not.
- `upMode` and `portalType` were taken straight from `toInt()` without
  clamping.

### Added

- `SECURITY.md`, including an honest account of the one finding left unfixed:
  credentials sit in unencrypted NVS. The remedy is flash encryption, which
  burns eFuses irreversibly and can leave a board unflashable, so it is
  documented rather than enabled by default.

## 1.0.0 (2026-08-11)

First release.

### Added

- Station plus access point with NAPT, so devices that only speak WPA2-PSK and
  have no browser can reach a network that demands a captive portal login or
  WPA2-Enterprise.
- DNS handed to access point clients over DHCP, which the softAP does not do by
  default.
- WPA2-Personal and WPA2-Enterprise uplinks (PEAP and TTLS, via the IDF 5
  `esp_eap_client` API).
- Configuration web interface on `192.168.4.1`, backed by NVS.
- Status screen, a reachability probe with exponential backoff, and a runtime
  check that the detected board matches expectations.
- Browser-based installer page and prebuilt firmware images.

### Fixed before release

- Cross-site scripting through unescaped apostrophes in single-quoted
  attributes. An SSID containing one was enough to break the form or inject
  markup.
- The access point password was not length-validated. WPA2 requires 8 to 63
  characters, and anything shorter made `softAP()` fail silently, stranding the
  user with no way back in short of a factory reset.
- A long press of button A triggered the short-press action on the way to the
  long one.
- Connectivity depended on a single probe endpoint, which is blocked outright
  in some countries.
