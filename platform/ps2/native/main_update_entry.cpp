#include "main_update_entry.h"
#include "cabinet_steering_filter.h"

namespace ps2::native {
bool begin_main_update(const MainEntryInput& input,MainEntryState& output) noexcept {
    if(input.surrounding_car_update)return false;
    auto state=output;
    state.flags=(state.flags&~0x80u)|((std::uint32_t(input.flag_sample)&1u)<<7);
    if(input.screen==9)state.steering=0;
    else {
        CabinetSteeringUpdate filtered;
        if(!filter_cabinet_steering(state.steering_history,input.steering_sample,filtered))return false;
        state.steering_history=filtered.history;state.steering=filtered.steering;
    }
    SegmentSelection selected;
    if(!select_course_segment(input.vertices,input.segments,input.position,state.progression,selected))return false;
    state.segment=selected.current<0?0:selected.current;
    state.progression=selected.progression;
    state.published_segment=state.segment;state.published_progression=state.progression;
    output=state;return true;
}
} // namespace ps2::native
