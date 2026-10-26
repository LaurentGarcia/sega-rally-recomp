#include "vehicle_parameter_selection.h"

namespace ps2::native {
VehicleParameterSelection select_vehicle_parameters(std::uint32_t flags,
    const std::array<VehicleParameterRecord,10>& records) noexcept {
    unsigned index=9;
    while(index&&!(flags&(std::uint32_t(1)<<index)))--index;
    return {records[index],index};
}
} // namespace ps2::native
