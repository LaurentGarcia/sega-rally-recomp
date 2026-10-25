#pragma once
#include <cstdint>

namespace ps2::native {

// Map the complete 8-bit cabinet sample domain to the recovered game value.
// Center 128, saturation at 80 counts on either side; exact binary32 result.
// This is the input conversion only, before the separate stateful filter.
float cabinet_steering(std::uint8_t sample) noexcept;

} // namespace ps2::native
