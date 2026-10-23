#include "car_pose.h"

namespace ps2::native {

void publish_car_pose(CarPoseState &state, RenderPose &pose) noexcept {
    constexpr std::uint32_t retained_flags = 0x0000c000;
    state.flags = (state.flags & ~retained_flags) | (pose.flags & retained_flags);
    pose.words = state.words;
    pose.flags = state.flags | 0x80;
    pose.progress = state.progress;
}

} // namespace ps2::native
