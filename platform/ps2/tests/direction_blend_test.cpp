#include "direction_blend.h"
#include "direction_blend_cases.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#ifndef _EE
#include <vector>
#endif

namespace {
using ps2::native::PlanarPair;
using direction_blend_cases::Bits;
std::array<std::uint32_t,2> bits(const PlanarPair &pair) {
    return {std::bit_cast<std::uint32_t>(pair.first),std::bit_cast<std::uint32_t>(pair.third)};
}
#ifndef _EE
// Independent host oracle: exact binary64 products of binary32 inputs, then
// explicit binary32 rounding at each mathematical boundary. No integer
// significand implementation is shared with the production function.
PlanarPair reference(const PlanarPair &base, const PlanarPair &target) {
    const auto round=[](double value) { return static_cast<float>(value); };
    const float p=round(double(base.first)*target.first);
    const float q=round(double(base.third)*target.third);
    const float dot=round(double(p)+q);
    const float weight=round(std::min(1.0,1.0-double(dot)));
    const float first_difference=round(double(target.first)-base.first);
    const float third_difference=round(double(target.third)-base.third);
    const float first_step=round(double(first_difference)*weight);
    const float third_step=round(double(third_difference)*weight);
    return {round(double(base.first)+first_step),round(double(base.third)+third_step)};
}
#endif
}
int main() {
    unsigned count=0,rejections=0;
    std::uint64_t fingerprint=14695981039346656037ull;
#ifndef _EE
    std::vector<Bits> inputs;
#endif
    const auto test=[&](Bits input) {
        const PlanarPair base{std::bit_cast<float>(input[0]),std::bit_cast<float>(input[1])};
        const PlanarPair target{std::bit_cast<float>(input[2]),std::bit_cast<float>(input[3])};
        PlanarPair result{};
        if (!ps2::native::blend_planar_direction(base,target,result)) return false;
        const auto output=bits(result);
#ifndef _EE
        if (output!=bits(reference(base,target))) {
            std::printf("DIRECTION MISMATCH %08x %08x %08x %08x got=%08x,%08x\n",
                        input[0],input[1],input[2],input[3],output[0],output[1]);
            return false;
        }
        inputs.push_back(input);
#endif
        auto first_alias=base,third_alias=target;
        if (!ps2::native::blend_planar_direction(first_alias,target,first_alias) ||
            !ps2::native::blend_planar_direction(base,third_alias,third_alias) ||
            bits(first_alias)!=output || bits(third_alias)!=output) return false;
        for (auto value:input) fingerprint=(fingerprint ^ value)*1099511628211ull;
        for (auto value:output) fingerprint=(fingerprint ^ value)*1099511628211ull;
        ++count;
        return true;
    };
    if (!direction_blend_cases::probe(test)) return 1;
    // Same original inputs/hash on host and EE; EE deliberately has no host
    // floating arithmetic oracle. Compare its fingerprint with the host run.
    std::printf("NATIVE_DIRECTION_PROBE PASS cases=%u hash=%016llx\n",count,
                static_cast<unsigned long long>(fingerprint));
#ifndef _EE
    if (!direction_blend_cases::extended(test)) return 1;
    std::sort(inputs.begin(),inputs.end());
    const auto unique=std::unique(inputs.begin(),inputs.end())-inputs.begin();
    std::printf("NATIVE_DIRECTION_HOST PASS cases=%u unique_quadruples=%llu hash=%016llx\n",
                count,static_cast<unsigned long long>(unique),static_cast<unsigned long long>(fingerprint));
#endif
    constexpr std::uint32_t invalid[]{0x3f800001u,0xbf800001u,0x7f7fffffu,0xff7fffffu,
                                      0x7f800000u,0xff800000u,0x7fc00000u,0x7f800001u,0xffc12345u};
    for (auto value:invalid) for (unsigned channel=0; channel<4; ++channel) {
        Bits input{0x3f000000u,0xbf000000u,0x3f800000u,0}; input[channel]=value;
        PlanarPair base{std::bit_cast<float>(input[0]),std::bit_cast<float>(input[1])};
        PlanarPair target{std::bit_cast<float>(input[2]),std::bit_cast<float>(input[3])};
        PlanarPair output{std::bit_cast<float>(0x80000000u),std::bit_cast<float>(0x7fc12345u)};
        const auto saved=bits(output),base_saved=bits(base),target_saved=bits(target);
        if (ps2::native::blend_planar_direction(base,target,output) || bits(output)!=saved ||
            ps2::native::blend_planar_direction(base,target,base) || bits(base)!=base_saved ||
            ps2::native::blend_planar_direction(base,target,target) || bits(target)!=target_saved) return 1;
        ++rejections;
    }
    std::printf("NATIVE_DIRECTION_REJECTION PASS cases=%u aliases_per_case=2\n",rejections);
    return 0;
}
