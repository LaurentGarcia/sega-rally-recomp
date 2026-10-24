#include "controls.h"
#include <algorithm>

namespace ps2 {
rt::Model2AIO::Inputs CabinetControls::sample(const PadSnapshot &pad) {
    rt::Model2AIO::Inputs result;
    constexpr uint8_t gears[] = {0, 0x20, 0x10, 0x60, 0x50};
    const uint16_t held = pad.connected ? pad.buttons : 0;
    const uint16_t pressed = held & ~previous_;
    previous_ = held;
    if (pressed & R1) gear_ = std::min(4, gear_ + 1);
    if (pressed & L1) gear_ = std::max(0, gear_ - 1);
    result.in1 = uint8_t(0x8f | gears[gear_]);
    if (!pad.connected) return result;
    const int axis = pad.analog ? int(pad.left_x) - 128 : 0;
    constexpr int deadzone = 16;
    int steer = 128;
    if (axis < -deadzone) steer += (axis + deadzone) * 128 / (128 - deadzone);
    if (axis > deadzone) steer += (axis - deadzone) * 127 / (127 - deadzone);
    const bool left = held & Left, right = held & Right;
    if (left != right) steer = left ? 0 : 255;
    result.analog[0] = uint8_t(steer);
    result.analog[1] = (held & Cross) ? 255 : 0;
    result.analog[2] = (held & Square) ? 255 : 0;
    if (held & Select) result.in0 &= uint8_t(~0x01u);
    if (held & Start) result.in0 &= uint8_t(~0x40u);
    if (held & Triangle) result.in0 &= uint8_t(~0x20u);
    result.in2 = (held & Circle) ? 255 : 0;
    return result;
}
} // namespace ps2
