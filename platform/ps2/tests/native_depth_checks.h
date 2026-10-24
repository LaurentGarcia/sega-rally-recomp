#pragma once
#include "native_display.h"
#include <bit>
#include <cstdint>

namespace ps2::test {
struct DepthChecks { bool pass=true; unsigned count=0; std::uint64_t hash=1469598103934665603ull; };
inline DepthChecks depth_checks() {
    DepthChecks result;
    const auto check=[&](std::uint32_t bits) {
        if (bits>0x3f800000u) return;
        const float input=std::bit_cast<float>(bits);
        // Binary64 has enough significand bits for exact binary32*65535
        // plus0.5 here. This oracle does not reuse integer production logic.
        const auto expected=static_cast<std::uint32_t>(double(input)*65535.+.5);
        const auto actual=ps2::native::reverse_depth16(input);
        result.pass &= actual==expected;
        result.hash=(result.hash^actual)*1099511628211ull; ++result.count;
    };
    for (unsigned i=0;i<=65536;++i) {
        const auto bits=std::bit_cast<std::uint32_t>(float(i)/65536.f);
        if (bits) check(bits-1);
        check(bits); check(bits+1);
    }
    std::uint32_t random=0x91ad73e5u;
    for (unsigned i=0;i<10000;++i) {
        random^=random<<13; random^=random>>17; random^=random<<5;
        check(random%0x3f800001u);
    }
    return result;
}
}
