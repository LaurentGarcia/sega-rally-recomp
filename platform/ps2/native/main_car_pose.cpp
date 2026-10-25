#include "main_car_pose.h"

namespace ps2::native {

RenderPose make_main_car_pose(const MainCarPoseState &state,
                             const MainCarExternalState &external) noexcept {
    RenderPose result;
    result.words.position = state.position;
    result.words.motion = state.motion;
    result.words.orientation = state.orientation;
    for (std::size_t i = 0; i < state.attributes.size(); ++i)
        result.words.attributes[i] = state.attributes[i];
    for (std::size_t i = 0; i < external.size(); ++i)
        result.words.attributes[state.attributes.size() + i] = external[i];
    result.flags = state.flags;
    result.progress = state.index;
    return result;
}

} // namespace ps2::native
