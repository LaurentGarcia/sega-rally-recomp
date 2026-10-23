#pragma once

namespace ps2 {
class GsDisplay;
// Uses the display's existing packet and native GS sprites without allocation.
// The caller supplies actual progress/errors; this adds no readiness claims.
bool show_status(GsDisplay &display, const char *message);
} // namespace ps2
