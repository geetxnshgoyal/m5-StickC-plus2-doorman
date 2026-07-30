#pragma once

namespace webui {
void begin();
void tick();
// Set when the user saves; main() reboots on the next loop so the new
// credentials are applied from a clean radio state.
bool rebootRequested();
}  // namespace webui
