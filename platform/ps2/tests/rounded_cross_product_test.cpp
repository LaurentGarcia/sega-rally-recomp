#include "rounded_cross_product_cases.h"
#include <cstdio>
#ifndef _EE
#include <cfenv>
#else
#include <kernel.h>
#endif

namespace {
using namespace ps2::native;
namespace cases = rounded_cross_product_cases;
#ifndef _EE
cases::Words reference(const cases::Case &input) {
    cases::Words result;
    for (unsigned i = 0; i < 3; ++i) {
        const auto j = (i + 1) % 3, k = (i + 2) % 3;
        // Volatile binary32 temporaries force all three original rounding
        // boundaries; the production function uses no floating arithmetic.
        const volatile float first = std::bit_cast<float>(input[j]) * std::bit_cast<float>(input[3+k]);
        const volatile float second = std::bit_cast<float>(input[k]) * std::bit_cast<float>(input[3+j]);
        const float difference = first - second;
        result[i] = std::bit_cast<std::uint32_t>(difference);
    }
    return result;
}
#endif
bool run(bool extended) {
    unsigned count = 0;
    std::uint64_t hash = 14695981039346656037ull;
    const auto test = [&](const cases::Case &c) {
        const auto a = cases::vector(c, 0), b = cases::vector(c, 3);
        CrossVector3 out;
        if (!rounded_cross_product(a,b,out)) return false;
        const auto result = cases::bits(out);
#ifndef _EE
        if (result != reference(c)) {
            std::printf("CROSS MISMATCH case=%u\n",count); return false;
        }
#endif
        auto left_alias = a, right_alias = b, all_alias = a;
        if (!rounded_cross_product(left_alias,b,left_alias) || cases::bits(left_alias) != result ||
            !rounded_cross_product(a,right_alias,right_alias) || cases::bits(right_alias) != result ||
            !rounded_cross_product(all_alias,all_alias,all_alias) || cases::bits(all_alias) != cases::Words{}) return false;
        cases::hash(hash,c,result); ++count; return true;
    };
    if (!cases::each(test,extended)) return false;
    if (count != (extended ? 110048u : 6208u) ||
        hash != (extended ? 0x428bbf5526cd241bull : 0x408f01a4e8d06ea5ull)) return false;
    std::printf("ROUNDED_CROSS_%s PASS cases=%u hash=%016llx aliases=3\n",
                extended ? "HOST" : "PROBE",count,static_cast<unsigned long long>(hash));
    return true;
}
} // namespace
int main() {
#ifndef _EE
    if (std::fegetround() != FE_TONEAREST) return 1;
#endif
    if (!run(false)) return 1;
#ifndef _EE
    if (!run(true)) return 1;
#endif
    // One product rounds to exactly one before subtraction. A fused product
    // and subtraction would produce a nonzero value instead.
    CrossVector3 fused_guard;
    if (!rounded_cross_product({0,std::bit_cast<float>(0x3f800001u),1},
                               {0,1,std::bit_cast<float>(0x3f7ffffeu)},fused_guard) ||
        cases::bits(fused_guard)[0] != 0) return 1;
    const cases::Words sentinel_bits{0x7fc12345u,0x80000000u,0xffffffffu};
    const CrossVector3 sentinel{std::bit_cast<float>(sentinel_bits[0]),std::bit_cast<float>(sentinel_bits[1]),
                                std::bit_cast<float>(sentinel_bits[2])};
    unsigned rejected=0;
    for (auto bad : {0x49800001u,0xc9800001u,0x7f800000u,0xff800000u,0x7fc00000u,
                     0xffc00000u,0x7f800001u,0xff800001u,0x7f7fffffu,0xff7fffffu})
        for (unsigned lane=0;lane<6;++lane) {
            cases::Case c{}; c[lane]=bad;
            auto a=cases::vector(c,0),b=cases::vector(c,3),out=sentinel;
            const auto a_bits=cases::bits(a),b_bits=cases::bits(b);
            if (rounded_cross_product(a,b,out) || cases::bits(out)!=sentinel_bits ||
                rounded_cross_product(a,b,a) || cases::bits(a)!=a_bits ||
                rounded_cross_product(a,b,b) || cases::bits(b)!=b_bits) return 1;
            rejected+=3;
        }
    std::printf("ROUNDED_CROSS_REJECTION PASS cases=%u fused_guard=1\n",rejected);
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
