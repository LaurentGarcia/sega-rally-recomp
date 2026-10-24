#pragma once
#include <cstdint>

namespace ps2 {
enum Button : uint16_t {
    Left = 1u << 0, Right = 1u << 1, Cross = 1u << 2, Square = 1u << 3,
    Circle = 1u << 4, Triangle = 1u << 5, L1 = 1u << 6, R1 = 1u << 7,
    Select = 1u << 8, Start = 1u << 9,
};
struct PadSnapshot {
    bool connected = false, analog = false;
    uint16_t buttons = 0;
    uint8_t left_x = 128; // Physical left-stick horizontal axis,0..255; center128.
};
} // namespace ps2
