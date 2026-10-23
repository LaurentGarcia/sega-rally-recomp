#pragma once
#include "runtime/model2a_io.h"
#include "pad_state.h"

namespace ps2 {
class CabinetControls {
public:
    rt::Model2AIO::Inputs sample(const PadSnapshot &pad);
    int gear() const { return gear_; }
private:
    int gear_ = 0;
    uint16_t previous_ = 0;
};
} // namespace ps2
