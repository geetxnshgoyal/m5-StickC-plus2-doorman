#pragma once
#include <Arduino.h>

// Makes phones offer their own "Sign in to network" prompt instead of quietly
// giving up.
//
// Every phone and laptop decides whether a network is usable by fetching one
// well-known URL right after it joins. A redirect means "there's a portal
// here" and the sign-in sheet appears. A timeout means "this network is
// broken", and crucially the device then stops asking: it will not probe again
// on its own, so a portal that comes back later is never noticed. Forgetting
// the network and rejoining is the only cure, which is a miserable thing to ask
// of anyone whose session expires daily.
//
// So Doorman answers those probes itself. It runs as the DNS server for its own
// clients, forwarding everything upstream as normal, except that while there is
// no internet it points the handful of known probe hostnames at itself and
// redirects them to the real login page.
namespace captive {

void begin();
void tick();  // drive from loop()

// True when a hostname belongs to a connectivity probe and we are currently
// answering it ourselves. webui uses this to decide whether to redirect.
bool intercepting();

}  // namespace captive
