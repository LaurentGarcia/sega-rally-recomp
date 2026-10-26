#pragma once
#include <array>
#include <cstdint>

namespace ps2::native {
struct VehicleParameterRecord { std::array<std::uint32_t,13> words{}; };
struct VehicleParameterSelection { VehicleParameterRecord record{}; unsigned index=0; };
// Highest set bit among9..1 selects that record; otherwise select record0.
// Bit0 and bits10..31 do not affect selection. All13 words are copied raw.
// The caller owns this immutable data; no original table is embedded here.
[[nodiscard]] VehicleParameterSelection select_vehicle_parameters(std::uint32_t flags,
    const std::array<VehicleParameterRecord,10>& records) noexcept;
} // namespace ps2::native
