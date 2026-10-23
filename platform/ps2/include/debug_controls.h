#pragma once
#include "pad_state.h"

namespace ps2 {
// Once the diagnostic chord is recognized, neither constituent button may
// reach the game until both are released. Ordinary single presses pass through.
class DebugPageToggle {
public:
    bool update(PadSnapshot &pad) {
        constexpr uint16_t mask = Select | Start;
        if (!pad.connected) { latched_ = false; return false; }
        const auto held = pad.buttons & mask;
        const bool toggled = !latched_ && held == mask;
        if (toggled) latched_ = true;
        if (latched_) pad.buttons &= ~mask;
        if (!held) latched_ = false;
        return toggled;
    }
private:
    bool latched_ = false;
};
} // namespace ps2
