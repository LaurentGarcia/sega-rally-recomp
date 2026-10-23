#pragma once
#include "runtime/model2a_io.h"
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
    uint8_t steer = 128;
};
class CabinetControls {
public:
    rt::Model2AIO::Inputs sample(const PadSnapshot &pad);
    int gear() const { return gear_; }
private:
    int gear_ = 0;
    uint16_t previous_ = 0;
};
} // namespace ps2
