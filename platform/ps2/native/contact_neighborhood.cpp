#include "contact_neighborhood.h"

namespace ps2::native {

bool contact_neighborhood(std::uint32_t segment_count, std::int16_t current,
                          ContactNeighborhood &output) noexcept {
    if (!segment_count || segment_count > max_contact_segments || current < 0)
        return false;
    const auto index = static_cast<std::uint32_t>(current);
    if (index >= segment_count) return false;
    const auto previous = index ? index - 1 : segment_count - 1;
    const auto next = index + 1 == segment_count ? 0 : index + 1;
    output = {static_cast<std::uint16_t>(previous),
              static_cast<std::uint16_t>(index),
              static_cast<std::uint16_t>(next)};
    return true;
}

} // namespace ps2::native
