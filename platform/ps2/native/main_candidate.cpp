#include "main_candidate.h"
#include "recovered_scalar_metric.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>

namespace ps2::native {
namespace {
// Exact bounded dyadic arithmetic, independently written in this project.
// No host floating operation participates in the recovered rounding contract.
constexpr std::uint32_t sign_bit = 0x80000000u, magnitude_mask = 0x7fffffffu;
struct Dyadic { std::uint32_t numerator; int power; };

Dyadic parts(std::uint32_t bits) noexcept {
    const auto exponent = (bits >> 23) & 255u;
    return {(bits & 0x7fffffu) | (exponent ? 0x800000u : 0u),
            exponent ? int(exponent) - 150 : -149};
}

// Round an exact unsigned dyadic to binary32, retaining signed underflow zero.
// Only bounded finite results enter this private arithmetic: no infinities,
// exceptions, processor registers or general instruction semantics are modeled.
std::uint32_t rounded(std::uint64_t numerator, int power, std::uint32_t sign) noexcept {
    if (!numerator) return sign;
    const int top = int(std::bit_width(numerator)) - 1;
    const int shift = (top + power < -126) ? -149 - power : top - 23;
    if (shift > 0) {
        if (shift >= 64) return sign; // Numerators here have at most 56 bits.
        const auto discarded = numerator & ((std::uint64_t(1) << shift) - 1);
        const auto half = std::uint64_t(1) << (shift - 1);
        numerator >>= shift;
        if (discarded > half || (discarded == half && (numerator & 1))) ++numerator;
    } else {
        numerator <<= -shift;
    }
    power += shift;
    if (numerator >= 0x1000000u) { numerator >>= 1; ++power; }
    if (numerator < 0x800000u) return sign | std::uint32_t(numerator);
    return sign | (std::uint32_t(power + 150) << 23) | (std::uint32_t(numerator) & 0x7fffffu);
}

std::uint32_t product(std::uint32_t left, std::uint32_t right) noexcept {
    const auto a = parts(left), b = parts(right);
    return rounded(std::uint64_t(a.numerator) * b.numerator, a.power + b.power,
                   (left ^ right) & sign_bit);
}

std::uint32_t sum(std::uint32_t left, std::uint32_t right) noexcept {
    auto a = parts(left), b = parts(right);
    if (!a.numerator && !b.numerator) return (left & right) & sign_bit;
    if (!a.numerator) return right;
    if (!b.numerator) return left;
    if (a.power < b.power) {
        const auto saved = left; left = right; right = saved;
        const auto value = a; a = b; b = value;
    }
    const unsigned gap = unsigned(a.power - b.power);
    // Both operands are already binary32. Beyond 32 exponent steps, the smaller
    // cannot reach even half the narrower neighboring spacing of the larger.
    if (gap > 32) return left;
    const std::uint64_t first = std::uint64_t(a.numerator) << gap;
    const std::uint64_t second = b.numerator;
    if ((left ^ right) & sign_bit) {
        if (first == second) return 0; // Exact cancellation at nearest/even.
        return first > second ? rounded(first - second,b.power,left & sign_bit)
                              : rounded(second - first,b.power,right & sign_bit);
    }
    return rounded(first + second,b.power,left & sign_bit);
}


std::uint32_t bits(float v) noexcept { return std::bit_cast<std::uint32_t>(v); }
float value(std::uint32_t b) noexcept { return std::bit_cast<float>(b); }
} // namespace

bool prepare_main_candidate(MainCandidateState& state) noexcept {
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::digits==24);
    for(const auto* group:{&state.position,&state.pending_position_delta,&state.motion,
                           &state.orientation,&state.angular_motion})
        for(float component:*group)if((bits(component)&magnitude_mask)>0x49800000u)return false;
    MainCandidateState result;
    std::memcpy(&result,&state,sizeof result);
    auto& delta=result.pending_position_delta;
    const auto middle=bits(delta[1]);
    // Binary64 0.1 lies strictly between these adjacent binary32 values.
    if((middle&magnitude_mask)>=0x3dcccccdu)delta[1]=value((middle&sign_bit)|0x3dcccccdu);
    float metric;
    if(!recovered_scalar_metric({0,0,delta[1]},{0,delta[0],delta[2]},metric))return false;
    // Binary64 0.002 lies just below its binary32 rounding. Positive binary32
    // comparison against its ceiling therefore preserves the original branch.
    if(bits(metric)>=0x3b03126fu) {
        result.response_marker=15;
        for(unsigned axis:{0u,2u}) {
            result.motion[axis]=value(sum(bits(result.motion[axis]),product(bits(delta[axis]),0x3f19999au)));
            // The original binary64 constant is exactly the binary32 value
            // 3ecccccc. This product has at most48 significant bits, so R64 is
            // exact before its separate binary32 rounding (including underflow).
            delta[axis]=value(product(bits(delta[axis]),0x3eccccccu));
        }
        for(unsigned axis=0;axis<3;++axis) {
            result.position[axis]=value(sum(bits(result.position[axis]),bits(delta[axis])));
            delta[axis]=0;
        }
    }
    for(unsigned axis=0;axis<3;++axis) {
        result.candidate_position[axis]=value(sum(bits(result.position[axis]),bits(result.motion[axis])));
        result.candidate_orientation[axis]=value(sum(bits(result.orientation[axis]),bits(result.angular_motion[axis])));
    }
    std::memcpy(&state,&result,sizeof result);return true;
}
} // namespace ps2::native
