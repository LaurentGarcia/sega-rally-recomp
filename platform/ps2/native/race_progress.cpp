#include "race_progress.h"

namespace ps2::native {

bool advance_race_progress(RaceProgress &state, std::size_t first_index,
                           std::size_t second_index) noexcept {
    if (first_index >= state.first_bank.size() || second_index >= state.second_bank.size())
        return false;
    if (state.remaining_ticks > 0) --state.remaining_ticks;
    for (auto &counter : state.counters) ++counter;
    ++state.first_bank[first_index];
    ++state.second_bank[second_index];
    return true;
}

} // namespace ps2::native
