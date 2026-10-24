#include "orientation.h"
#include "orientation_cases.h"
#include <bit>
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
using orientation_cases::Bits;
ps2::native::OrientationAngles value(Bits bits) {
    return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}
Bits bits(ps2::native::OrientationAngles value) {
    return {std::bit_cast<std::uint32_t>(value.first), std::bit_cast<std::uint32_t>(value.middle),
            std::bit_cast<std::uint32_t>(value.third)};
}
#ifndef _EE
std::uint32_t expected(std::uint32_t input, bool outer) {
    // Independent numerical reference; EE fixture uses bounded original
    // boundary results below because EE's native FPU is not IEEE binary64.
    double angle = std::bit_cast<float>(input);
    constexpr double pi = 0x1.921fb54442d18p+1;
    if (angle > pi) angle -= 2*pi;
    else if (angle < -pi) angle += 2*pi;
    float rounded = float(angle);
    if (outer && double(rounded) > pi/8) rounded = float(pi/8);
    if (outer && double(rounded) < -pi/8) rounded = -float(pi/8);
    return std::bit_cast<std::uint32_t>(rounded);
}
#endif
unsigned checks = 0;
bool check(Bits input, Bits want) {
    auto angles = value(input);
    if (!ps2::native::finalize_orientation(angles) || bits(angles) != want) {
        const auto got = bits(angles);
        std::printf("ORIENTATION MISMATCH input=%08x,%08x,%08x output=%08x,%08x,%08x\n",
                    input[0],input[1],input[2],got[0],got[1],got[2]);
        return false;
    }
    ++checks;
    return true;
}
bool run() {
    // Independently calculated mathematical endpoints, usable on EE without
    // depending on its floating-point arithmetic. Columns: input/fold/bound.
    constexpr Bits endpoints[] = {
        {0x00000000,0x00000000,0x00000000}, {0x00000001,0x00000001,0x00000001},
        {0x007fffff,0x007fffff,0x007fffff}, {0x00800000,0x00800000,0x00800000},
        {0x3ec90fd9,0x3ec90fd9,0x3ec90fd9}, {0x3ec90fda,0x3ec90fda,0x3ec90fda},
        {0x3ec90fdb,0x3ec90fdb,0x3ec90fdb}, {0x3ec90fdc,0x3ec90fdc,0x3ec90fdb},
        {0x3f800000,0x3f800000,0x3ec90fdb}, {0x40490fd9,0x40490fd9,0x3ec90fdb},
        {0x40490fda,0x40490fda,0x3ec90fdb}, {0x40490fdb,0xc0490fda,0xbec90fdb},
        {0x40490fdc,0xc0490fd9,0xbec90fdb}, {0x40c90fda,0xb4a22169,0xb4a22169},
        {0x40c90fdb,0x343bbd2e,0x343bbd2e}
    };
    for (auto a : endpoints) for (auto b : endpoints) for (auto c : endpoints)
        for (unsigned signs = 0; signs < 8; ++signs) {
            const std::uint32_t x = (signs & 1) ? 0x80000000u : 0;
            const std::uint32_t y = (signs & 2) ? 0x80000000u : 0;
            const std::uint32_t z = (signs & 4) ? 0x80000000u : 0;
            if (!check({a[0]^x,b[0]^y,c[0]^z},{a[2]^x,b[1]^y,c[2]^z})) return false;
        }
    // Channel selection, signed zero, exact clamp neighbors and fold-before-
    // bound behavior: folding +pi's nearest value changes its sign.
    if (!check({0x80000000,0,1},{0x80000000,0,1}) ||
        !check({0x3ec90fda,0x3f800000,0xbec90fdc},{0x3ec90fda,0x3f800000,0xbec90fdb}) ||
        !check({0x40490fdb,0x40490fda,0xc0490fdb},{0xbec90fdb,0x40490fda,0x3ec90fdb}) ||
        !check({0x3f800000,0xbf800000,0xbf800000},{0x3ec90fdb,0xbf800000,0xbec90fdb})) return false;
#ifndef _EE
    if (!orientation_cases::accepted([](Bits input) {
        return check(input,{expected(input[0],true),expected(input[1],false),expected(input[2],true)});
    })) return false;
#endif
    for (auto bad : {0x40c90fdcu,0xc0c90fdcu,0x7f7fffffu,0xff7fffffu,
                     0x7f800000u,0xff800000u,0x7fc00001u,0x7f800001u,0xffc54321u})
        for (unsigned channel = 0; channel < 3; ++channel) {
            Bits input = {0x40490fdb,0xc0490fdb,0x3f800000};
            input[channel] = bad;
            auto angles = value(input);
            if (ps2::native::finalize_orientation(angles) || bits(angles) != input) return false;
            ++checks;
        }
    return true;
}
} // namespace
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    const bool ok = run();
    std::printf("NATIVE_ORIENTATION %s checks=%u\n",ok ? "PASS" : "FAIL",checks);
#ifdef _EE
    SleepThread();
#endif
    return ok ? 0 : 1;
}
