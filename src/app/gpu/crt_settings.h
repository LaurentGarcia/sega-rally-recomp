#pragma once

namespace app {
// Presentation only. Disabled by default; all raster/image comparisons bypass
// the effect. Strengths are finite and clamped again by CrtRenderer.
struct CrtSettings {
    bool enabled = false;
    float scanlines = 0.25f; // 0..1
    float mask = 0.12f;      // 0..1
    float glow = 0.10f;      // 0..0.5
    float curvature = 0.035f; // 0..0.15
    float chromatic = 0.25f; // 0..1 native source pixels
};
} // namespace app
