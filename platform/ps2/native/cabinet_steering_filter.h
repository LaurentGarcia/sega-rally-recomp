#pragma once
#include <cstdint>

namespace ps2::native {
struct CabinetSteeringUpdate {
    float history;
    float steering;
};

// Apply the recovered cabinet sample filter and sign-flipped output.
// History must be finite in [-1,1], including signed zero and subnormals.
// Every byte sample is supported. Invalid history preserves both output
// fields; a successful history stays within [-1,1] for subsequent updates.
// This is input conditioning, not wheel calibration or a vehicle model.
[[nodiscard]] bool filter_cabinet_steering(float history, std::uint8_t sample,
                                          CabinetSteeringUpdate &output) noexcept;
} // namespace ps2::native
