# Security

## Reporting

Open a private security advisory through the repository's Security tab. Please
don't file a public issue for anything that exposes credentials.

## The security model

Doorman holds real secrets: your Wi-Fi password, possibly your enterprise
credentials, and your captive portal login. It is worth being clear about what
protects them.

**The settings page is reachable only from Doorman's own network.** The web
server binds to every interface, and the device runs as station and access
point at once, so without a check it would answer requests from the upstream
network too. Every handler rejects clients outside the softAP subnet.

**The settings page requires a password.** Being on the private network is not
enough on its own, because the whole purpose of this device is to put IoT
gear you have no reason to trust onto that same subnet. A compromised bulb
should not be able to read your credentials. The settings password defaults to
your AP password, and you can set a separate one.

**Stored passwords are never sent to the browser.** Password fields render
empty and say only whether something is set. Submitting one blank leaves the
stored value alone. Fetching the settings page discloses no credentials.

**Forms carry a per-boot token.** Both state-changing routes are POST and both
verify it, so a page you happen to visit while on Doorman's network cannot
reconfigure the device behind your back.

**Cleartext portal retries are opt-in.** When a portal offers CHAP, the
password is not supposed to cross the wire at all. Earlier versions retried in
cleartext when CHAP failed, which meant a gateway that always rejected CHAP
would collect the password on the second attempt. That retry is now off unless
you turn it on.

## Known limitations

**Credentials are stored unencrypted in NVS.** Anyone with physical access to
the device and a USB cable can dump the flash and read them.

This is deliberate, and worth explaining. The fix is ESP32 flash encryption,
which burns eFuses. That is irreversible, can leave the board unable to be
reflashed over serial, and bricks it outright if done wrong. Enabling it by
default on a hobbyist device that ships as a browser-flashable binary would
break more people than it protects.

If your threat model includes someone picking the device up, enable flash
encryption and NVS encryption yourself. Read
[Espressif's flash encryption guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/security/flash-encryption.html)
first, all of it, including the part about eFuses being permanent.

Practically: treat a Doorman you leave plugged in somewhere shared as a device
that will surrender its stored passwords to anyone who walks off with it. Use a
portal account you don't reuse elsewhere.

**Enterprise networks need a CA certificate.** Without one, the supplicant
offers MSCHAPv2 credentials to any access point broadcasting your SSID, which
is the standard evil-twin credential harvest. Paste your RADIUS server's CA in
the settings page. If you leave it blank it will still work, and the boot log
will warn you, but you are trusting whatever answers.

**The settings page is HTTP, not HTTPS.** It is served over your own WPA2
protected network, so the link is encrypted at layer 2. Adding TLS would mean
shipping a certificate that no browser trusts, which trains people to click
through warnings and buys very little.

**Traffic through Doorman is not private from the upstream network.** It is a
NAT router, not a VPN. Whoever runs the network you joined sees what they would
have seen anyway.
