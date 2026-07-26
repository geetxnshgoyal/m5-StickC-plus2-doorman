#pragma once
#include <Arduino.h>

namespace portal {

enum State : uint8_t {
  IDLE,        // no upstream link yet
  CHECKING,    // probing whether traffic actually reaches the internet
  LOGGING_IN,
  ONLINE,
  FAILED,      // login attempted and rejected; will retry with backoff
};

void begin();

// Drive from loop(). Handles the periodic reachability probe and re-login when
// the hotspot session lapses (RouterOS caps sessions, typically at 24h).
void tick();

State state();
const char *stateName();
String lastError();
uint32_t loginCount();

// Forces an immediate re-login attempt, ignoring backoff.
void forceLogin();

// True when a probe to a known-204 endpoint came back clean.
bool online();

}  // namespace portal
