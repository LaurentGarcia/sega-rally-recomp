#include "recovered_scalar_metric.h"
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace ps2::native {
namespace {
// Local reuse of our independently written bounded dyadic arithmetic. This
// numerical operation has no processor state, data transport or asset table.
constexpr std::uint32_t sign_bit = 0x80000000u;
struct Dyadic { std::uint32_t numerator; int power; };
Dyadic parts(std::uint32_t bits) noexcept {
    const auto exponent = (bits >> 23) & 255u;
    return {(bits & 0x7fffffu) | (exponent ? 0x800000u : 0u),
            exponent ? int(exponent) - 150 : -149};
}
std::uint32_t rounded(std::uint64_t numerator, int power, std::uint32_t sign) noexcept {
    if (!numerator) return sign;
    const int top = int(std::bit_width(numerator)) - 1;
    const int shift = (top + power < -126) ? -149 - power : top - 23;
    if (shift > 0) {
        if (shift >= 64) return sign; // At most 56 numerator bits enter here.
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
    // The smaller binary32 operand cannot reach half the narrower adjacent
    // spacing once the scale gap exceeds 32. The larger is already rounded.
    if (gap > 32) return left;
    const std::uint64_t first = std::uint64_t(a.numerator) << gap;
    const std::uint64_t second = b.numerator;
    if ((left ^ right) & sign_bit) {
        if (first == second) return 0;
        return first > second ? rounded(first - second, b.power, left & sign_bit)
                              : rounded(second - first, b.power, right & sign_bit);
    }
    return rounded(first + second, b.power, left & sign_bit);
}
std::uint32_t difference(std::uint32_t left, std::uint32_t right) noexcept {
    return sum(left, right ^ sign_bit);
}
std::uint32_t quotient(std::uint32_t left,std::uint32_t right) noexcept {
    const auto a=parts(left),b=parts(right);
    const auto sign=(left^right)&sign_bit;
    if(!a.numerator)return sign;
    int ratio_top=int(std::bit_width(a.numerator))-int(std::bit_width(b.numerator));
    if(ratio_top>=0){if(std::uint64_t(a.numerator)<(std::uint64_t(b.numerator)<<ratio_top))--ratio_top;}
    else if((std::uint64_t(a.numerator)<<-ratio_top)<b.numerator)--ratio_top;
    const int exponent=ratio_top+a.power-b.power;
    const int quantum=exponent < -126 ? -149 : exponent-23;
    const unsigned shift=unsigned(a.power-b.power-quantum);
    // Here both operands are positive normal values in [0.5,2].
    // Their 24-bit significands need at most 48 scaled numerator bits.
    const auto numerator=std::uint64_t(a.numerator)<<shift;
    auto significand=numerator/b.numerator;
    const auto remainder=numerator%b.numerator;
    const auto twice=remainder*2;
    if(twice>b.numerator||(twice==b.numerator&&(significand&1)))++significand;
    return rounded(significand,quantum,sign);
}
using Words = std::array<std::uint32_t,3>;
Words bits(const MetricPoint3 &value) noexcept {
    return {std::bit_cast<std::uint32_t>(value.x), std::bit_cast<std::uint32_t>(value.y),
            std::bit_cast<std::uint32_t>(value.z)};
}
} // namespace
bool recovered_scalar_metric(const MetricPoint3 &first, const MetricPoint3 &second,
                             float &output) noexcept {
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::radix==2 &&
                  std::numeric_limits<float>::digits==24 &&
                  std::bit_cast<std::uint32_t>(1.0f)==0x3f800000u);
    const auto a=bits(first), b=bits(second);
    for(unsigned i=0;i<3;++i)
        if((a[i]&0x7fffffffu)>0x49800000u || (b[i]&0x7fffffffu)>0x49800000u) return false;
    Words squares;
    for(unsigned i=0;i<3;++i) {
        const auto delta=difference(b[i],a[i]);
        squares[i]=product(delta,delta);
    }
    const auto squared=sum(sum(squares[2],squares[1]),squares[0]);
    if(squared && squared<0x00800000u) return false;
    std::uint32_t result=0;
    if(squared) {
        constexpr std::uint32_t one=0x3f800000u, half=0x3f000000u;
        constexpr std::uint32_t slope=0x3f1714bau, intercept=0x3ed5a9a8u, odd_scale=0x3f3504f4u;
        const auto mantissa=(squared&0x007fffffu)|half;
        const auto seed=sum(intercept,product(mantissa,slope));
        // The first correction contains seed/seed = exactly one. Replacing
        // it with mantissa/seed would change the established reference value.
        const auto halfway=product(sum(one,seed),half);
        result=product(sum(quotient(mantissa,halfway),halfway),half);
        int adjustment=int(squared>>23)-126;
        if(adjustment%2) { result=product(result,odd_scale); ++adjustment; }
        const int exponent=int(result>>23)+adjustment/2;
        result=(result&0x007fffffu)|(std::uint32_t(exponent)<<23);
    }
    output=std::bit_cast<float>(result);
    return true;
}
} // namespace ps2::native
