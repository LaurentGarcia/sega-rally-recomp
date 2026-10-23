// Original mathematical boundary vectors; no game instructions or assets.
#include "angle_fold.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
std::uint64_t checks = 0;
bool check(std::uint32_t bits) {
    float actual = std::bit_cast<float>(bits);
    const auto magnitude = bits & 0x7fffffffu;
    if (magnitude > 0x40c90fdb) {
        if (ps2::native::fold_angle_once(actual) || std::bit_cast<std::uint32_t>(actual) != bits)
            return false;
    } else {
        constexpr double pi = 0x1.921fb54442d18p+1;
        constexpr double turn = 0x1.921fb54442d18p+2;
        const double source = actual;
        float expected = actual;
        if (source > pi) expected = float(source - turn);
        else if (source < -pi) expected = float(source + turn);
        if (!ps2::native::fold_angle_once(actual) ||
            std::bit_cast<std::uint32_t>(actual) != std::bit_cast<std::uint32_t>(expected)) {
            std::fprintf(stderr, "angle fold mismatch input=%08x actual=%08x expected=%08x\n",
                         bits, std::bit_cast<std::uint32_t>(actual), std::bit_cast<std::uint32_t>(expected));
            return false;
        }
    }
    ++checks;
    return true;
}
bool both(std::uint32_t magnitude) { return check(magnitude) && check(magnitude | 0x80000000); }
} // namespace

int main(int argc, char **argv) {
    constexpr std::uint32_t boundaries[] = {
        0, 1, 0x007fffff, 0x00800000, 0x3f800000, 0x40490fda,
        0x40490fdb, 0x40800000, 0x40c90fda, 0x40c90fdb,
        0x40c90fdc, 0x7f7fffff, 0x7f800000, 0x7f800001, 0x7fc00001, 0x7fffffff
    };
    for (const auto value : boundaries) if (!both(value)) return 1;
    for (auto center : {0x40490fdbu, 0x40800000u, 0x40c90fdbu})
        for (int delta = -32; delta <= 32; ++delta)
            if (!both(center + delta)) return 1;
    // Exercise every binary exponent and diverse fraction bits, including
    // unchanged subnormals, non-finites and unchanged rejection state.
    for (unsigned exponent = 0; exponent < 256; ++exponent)
        for (unsigned fraction : {0u, 1u, 0x155555u, 0x400000u, 0x7ffffeu, 0x7fffffu})
            if (!both((exponent << 23) | fraction)) return 1;
    std::uint32_t random = 0x1c653a97;
    for (unsigned i = 0; i < 32768; ++i) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        if (!check(random)) return 1;
    }
    const bool exhaustive = argc == 2 && std::strcmp(argv[1], "--exhaustive-folded") == 0;
    if (exhaustive)
        for (std::uint32_t magnitude = 0x40490fdb; magnitude <= 0x40c90fdb; ++magnitude)
            if (!both(magnitude)) return 1;
    std::printf("Native angle fold PASS: %llu checks; folded interval exhaustive=%u\n",
                (unsigned long long)checks, unsigned(exhaustive));
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
