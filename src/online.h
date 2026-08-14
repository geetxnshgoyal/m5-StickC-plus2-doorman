#pragma once
#include <Arduino.h>

// A reachability monitor, and nothing more.
//
// Doorman does not log in to captive portals. You do that yourself, once, from
// any device behind it, and because everything shares one upstream MAC the
// whole private network is covered by that single login. This module only
// answers "is traffic actually getting out", so the screen can say something
// useful when it isn't.
namespace online {

void begin();
void tick();  // drive from loop()

bool ok();             // last probe genuinely reached the internet
const char *status();  // short label for the screen
void recheck();        // probe now rather than waiting for the next interval

#ifdef DOORMAN_SPEEDTEST
// Diagnostic only, built with -DDOORMAN_SPEEDTEST. Measures how fast the device
// itself can pull bytes down, which separates a slow uplink from slow NAT
// forwarding when a client behind the AP reports poor throughput.
void benchmark();
#endif

}  // namespace online
