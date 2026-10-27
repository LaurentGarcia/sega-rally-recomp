#pragma once
#include "segment_selection.h"

namespace ps2::native {
struct MainEntryInput {
    std::span<const ContactPointXZ> vertices;
    std::span<const CourseSegmentCorners> segments;
    ContactPointXZ position{};
    std::uint8_t steering_sample=128, flag_sample=0;
    std::int32_t screen=0;
    // The optional surrounding-car gravity calculation is a separate stage.
    // Until supplied by that stage, callers must reject an active request.
    bool surrounding_car_update=false;
};
struct MainEntryState {
    std::uint32_t flags=0;
    float steering_history=0, steering=0;
    std::int16_t segment=0, progression=0;
    std::int32_t published_segment=0, published_progression=0;
};
// Input conditioning and course selection at the start of the main update.
// Negative selected indices are clamped only after the recovered search;
// progression remains signed and unchanged by that clamp. Failure is atomic.
[[nodiscard]] bool begin_main_update(const MainEntryInput& input,
                                      MainEntryState& state) noexcept;
} // namespace ps2::native
