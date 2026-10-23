#pragma once
#include <array>
#include <cstdint>

namespace ps2::native {

// Raw value bits at the game/render boundary. This component copies values;
// it does not interpret coordinate units, angle order or floating-point data.
struct CarPoseWords {
    std::array<std::uint32_t, 3> position{}, motion{}, orientation{};
    std::array<std::uint32_t, 11> attributes{};
    bool operator==(const CarPoseWords &) const = default;
};
struct CarPoseState {
    CarPoseWords words;
    std::uint32_t flags = 0, progress = 0;
    bool operator==(const CarPoseState &) const = default;
};
struct RenderPose {
    CarPoseWords words;
    std::uint32_t flags = 0, progress = 0;
    bool operator==(const RenderPose &) const = default;
};

// Publish into a distinct render pose, retaining its two persistent flag bits
// in the game state and setting the observed publication flag in the output.
void publish_car_pose(CarPoseState &state, RenderPose &pose) noexcept;

} // namespace ps2::native
