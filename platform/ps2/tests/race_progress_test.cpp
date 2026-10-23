// Original boundary vectors; this fixture contains no game instructions/data.
#include "race_progress.h"
#include <cstdio>
#include <limits>
#ifdef _EE
#include <kernel.h>
#endif

int main() {
    using ps2::native::RaceProgress;
    using ps2::native::advance_race_progress;
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    constexpr std::int32_t remaining[] = {
        std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 2,
        std::numeric_limits<std::int32_t>::max()
    };
    unsigned checks = 0;
    for (auto left : remaining) for (std::size_t a : {0u, 1u, 62u, 63u})
        for (std::size_t b : {0u, 1u, 62u, 63u}) {
            RaceProgress state;
            state.remaining_ticks = left;
            state.counters = {maximum, 41};
            state.first_bank.fill(123);
            state.second_bank.fill(456);
            state.first_bank[a] = maximum;
            state.second_bank[b] = maximum;
            if (!advance_race_progress(state, a, b) ||
                state.remaining_ticks != (left > 0 ? left - 1 : left) ||
                state.counters != std::array<std::uint32_t, 2>{0, 42}) return 1;
            for (std::size_t i = 0; i < RaceProgress::bank_size; ++i)
                if (state.first_bank[i] != (i == a ? 0u : 123u) ||
                    state.second_bank[i] != (i == b ? 0u : 456u)) return 1;
            ++checks;
        }
    RaceProgress state;
    state.remaining_ticks = 5;
    state.counters = {15, 19};
    state.first_bank.fill(72);
    state.second_bank.fill(18);
    const auto before = state;
    for (std::size_t invalid : {std::size_t(64), std::size_t(65),
                               std::numeric_limits<std::size_t>::max()}) {
        if (advance_race_progress(state, invalid, 0) || state != before ||
            advance_race_progress(state, 0, invalid) || state != before ||
            advance_race_progress(state, invalid, invalid) || state != before) return 1;
        checks += 3;
    }
    for (unsigned i = 0; i < 10; ++i)
        if (!advance_race_progress(state, 63, 0)) return 1;
    if (state.remaining_ticks != 0 || state.counters[0] != 25 || state.counters[1] != 29 ||
        state.first_bank[63] != 82 || state.second_bank[0] != 28) return 1;
    std::printf("Native race progress PASS: %u boundary/rejection checks and repeated updates\n", checks);
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
