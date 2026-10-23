#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ps2::native {

// One recovered progress update, not the race state machine or its cadence.
// Counter/bank names remain neutral until their callers are migrated.
struct RaceProgress {
    static constexpr std::size_t bank_size = 64;
    std::int32_t remaining_ticks = 0;
    std::array<std::uint32_t, 2> counters{};
    std::array<std::uint32_t, bank_size> first_bank{};
    std::array<std::uint32_t, bank_size> second_bank{};
    bool operator==(const RaceProgress &) const = default;
};

// Select one entry from each bank. Invalid indices leave all state untouched.
// One successful call consumes one positive remaining tick and increments
// both counters and the two selected entries, with unsigned 32-bit wrapping.
bool advance_race_progress(RaceProgress &state, std::size_t first_index,
                           std::size_t second_index) noexcept;

} // namespace ps2::native
