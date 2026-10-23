// Optional host-owned sound ROM access for memory-constrained machines.
// All offsets are aligned byte offsets in the original big-endian bus image.
#pragma once
#include <cstdint>

namespace snd {
enum class SoundImage { Program, Samples };
class SoundImageSource {
public:
    virtual ~SoundImageSource() = default;
    virtual uint32_t size(SoundImage image) const = 0;
    virtual uint16_t read16_be(SoundImage image, uint32_t offset) = 0;
};
} // namespace snd
