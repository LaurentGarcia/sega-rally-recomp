#pragma once
#include "contact_result.h"
#include <array>
#include <cstdint>
#include <span>
namespace ps2::native {
struct MainContactSlotInput {
    std::uint8_t slot=0;
    ContactResult contact{};
    ContactVertex vector{},axis_direction{};
    float reference_middle=0,factor=0.25f,object_parameter=1;
    std::array<float,5> parameters{};
    float control=1;
    std::span<const ContactNormalPair> normal_table;
};
struct MainContactSlotState {
    std::uint16_t packed_types=0;
    std::uint32_t type_mask=0;
    std::array<float,4> history{},published_middle{},gain{},metric{},lateral{};
};
struct MainContactSlotOutput { ContactVertex response{};bool active=false; };
// Recovered four-slot update. Slot is0..3. Finite gap and any used history,
// direction, factor/control component have magnitude<=16; used parameters have
// magnitude<=2^16; reference/vector coordinates have magnitude<=2^20.
// Used table-normalization inputs require squared XZ length zero or >=2^-40 and the
// established8192-pair coefficient domain. Other fields are untouched payloads.
// Signed zeros/subnormals and every explicit binary32/binary64 rounding boundary
// are retained. Unsupported input rejects atomically, preserving both outputs.
// Physical meanings of the slot arrays and parameter names remain unproven.
[[nodiscard]] bool update_main_contact_slot(const MainContactSlotInput& input,
    MainContactSlotState& state,MainContactSlotOutput& output) noexcept;
} // namespace ps2::native
