#pragma once
#include "car_pose.h"
#include <array>
#include <cstdint>

namespace ps2::native {

// Raw publication values only. Group names do not establish units or axes.
struct MainCarPoseState {
    std::array<std::uint32_t, 3> position{}, motion{}, orientation{};
    std::array<std::uint32_t, 7> attributes{};
    std::uint32_t flags = 0, index = 0;
    bool operator==(const MainCarPoseState &) const = default;
};
using MainCarExternalState = std::array<std::uint32_t, 4>;

// Snapshot all input bits into a new value. Flags are copied unchanged; there
// is no dependency on an earlier destination and neither input is modified.
// Every uint32_t encoding is valid. References must denote live typed objects;
// guest pointers, partial storage overlap and bus-write order are not an API.
[[nodiscard]] RenderPose make_main_car_pose(const MainCarPoseState &state,
                                           const MainCarExternalState &external) noexcept;

} // namespace ps2::native
