#pragma once

#include <array>
#include <cstdint>

namespace snd {

// The encoded gain has eight TL bits, four attenuation bits, a channel bit
// and three SDL bits. The channel bit selects which side is attenuated; the
// other side has exactly the same gain as attenuation zero. Store each
// distinct value once on memory-constrained hosts. Calculation and rounding
// remain in Model2Scsp::init; this class only changes the lookup layout.
class ScspPanTable {
public:
    void set(uint16_t encoded, int left, int right) {
#ifdef M2_COMPACT_SOUND
        values_[index(encoded, false)] = left;
        values_[index(encoded, true)] = right;
#else
        left_[encoded] = left;
        right_[encoded] = right;
#endif
    }
    int left(uint16_t encoded) const {
#ifdef M2_COMPACT_SOUND
        return values_[index(encoded, false)];
#else
        return left_[encoded];
#endif
    }
    int right(uint16_t encoded) const {
#ifdef M2_COMPACT_SOUND
        return values_[index(encoded, true)];
#else
        return right_[encoded];
#endif
    }
private:
#ifdef M2_COMPACT_SOUND
    static constexpr unsigned index(uint16_t encoded, bool right) {
        const bool attenuated = bool(encoded & 0x1000) == right;
        return (encoded & 0xff) | (attenuated ? encoded & 0x0f00 : 0)
             | ((encoded & 0xe000) >> 1);
    }
    std::array<int, 0x8000> values_{};
#else
    std::array<int, 0x10000> left_{}, right_{};
#endif
};

} // namespace snd
