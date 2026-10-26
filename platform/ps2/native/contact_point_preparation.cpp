#include "contact_point_preparation.h"
#include <bit>
#include <cstdint>
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


using Words=std::array<std::uint32_t,3>;
Words words(const AffinePoint3 &p) noexcept {
    return {std::bit_cast<std::uint32_t>(p.x),std::bit_cast<std::uint32_t>(p.y),std::bit_cast<std::uint32_t>(p.z)};
}
AffinePoint3 point(const Words &p) noexcept {
    return {std::bit_cast<float>(p[0]),std::bit_cast<float>(p[1]),std::bit_cast<float>(p[2])};
}
bool bounded(const Words &p,std::uint32_t bound) noexcept {
    for(auto v:p)if((v&magnitude_mask)>bound)return false;
    return true;
}

// Recovered periodic polynomial. These are semantic numeric constants of the
// formula, not a lookup table or a copy of processor instructions. The two
// reductions and all six terms preserve their separate binary32 boundaries.
std::uint32_t periodic_polynomial(std::uint32_t angle) noexcept {
    constexpr std::uint32_t pi=0x40490fdbu,half_pi=0x3fc90fdbu,reciprocal_pi=0x3ea2f983u;
    constexpr std::array<std::uint32_t,6> coefficient{
        0x3f000000u,0x3d2aaaabu,0x3ab60b61u,0x37d00d01u,0x3493f27eu,0x310f76c7u};
    const auto magnitude=angle&magnitude_mask;
    const auto quotient=parts(product(magnitude,reciprocal_pi));
    const unsigned whole=quotient.power<=-32?0u:
        quotient.power<0?quotient.numerator>>(-quotient.power):quotient.numerator<<quotient.power;
    const auto multiple=product(pi,rounded(whole,0,0));
    const auto centered=sum(sum(magnitude,multiple^sign_bit),half_pi^sign_bit);
    const auto square=product(centered,centered);
    std::array<std::uint32_t,6> powers{square};
    for(unsigned i=1;i<6;++i)powers[i]=product(square,powers[i-1]);
    auto result=product(powers[5],coefficient[5]);
    for(int i=4;i>=0;--i){
        auto term=product(powers[unsigned(i)],coefficient[unsigned(i)]);
        if(!(i&1))term^=sign_bit;
        result=sum(result,term);
    }
    result=sum(result,0x3f800000u);
    const unsigned nonpositive=(angle&sign_bit)||!magnitude;
    return result^(((whole+nonpositive)&1u)?sign_bit:0u);
}
} // namespace

bool prepare_contact_points(const AffinePoint3 &orientation,const AffinePoint3 &position,
                            const std::array<AffinePoint3,4> &local_points,
                            ContactPointPreparation &output) noexcept {
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
    const auto angles=words(orientation),origin=words(position);
    std::array<Words,4> inputs;
    for(unsigned i=0;i<4;++i)inputs[i]=words(local_points[i]);
    if(!bounded(angles,0x41000000u)||!bounded(origin,0x49800000u))return false;
    for(const auto &p:inputs)if(!bounded(p,0x49800000u))return false;
    std::array<Words,3> columns{Words{0x3f800000u,0,0},Words{0,0x3f800000u,0},Words{0,0,0x3f800000u}};
    // Apply the middle, first, then third component to the indicated column
    // pair. Preserve all zero products and their signs even for identity input.
    for(unsigned stage=0;stage<3;++stage){
        const unsigned component=stage==0?1:stage==1?0:2;
        const auto sine=periodic_polynomial(angles[component]);
        const auto cosine=periodic_polynomial(sum(angles[component],0x3fc90fdbu));
        const unsigned first=stage==1?1:0,second=stage==2?1:2;
        const auto a=columns[first],b=columns[second];
        for(unsigned row=0;row<3;++row){
            const auto ca=product(cosine,a[row]),sb=product(sine,b[row]);
            const auto sa=product(sine,a[row]),cb=product(cosine,b[row]);
            columns[first][row]=sum(ca,stage==0?sb:sb^sign_bit);
            columns[second][row]=stage==0?sum(cb,sa^sign_bit):sum(sa,cb);
        }
    }
    ContactPointPreparation result;
    for(unsigned column=0;column<3;++column)result.matrix.columns[column]=point(columns[column]);
    for(unsigned i=0;i<4;++i){
        if(!rounded_affine_transform(result.matrix,point(inputs[i]),result.offsets[i]))return false;
        auto absolute=words(result.offsets[i]);
        for(unsigned axis=0;axis<3;++axis)absolute[axis]=sum(absolute[axis],origin[axis]);
        result.points[i]=point(absolute);
    }
    output=result;
    return true;
}
} // namespace ps2::native
