#include "cabinet_steering.h"
#include <bit>
#include <cstdio>

int main() {
    using ps2::native::cabinet_steering;
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned sample = 0; sample != 256; ++sample) {
        const float actual = cabinet_steering(std::uint8_t(sample));
        const auto bits = std::bit_cast<std::uint32_t>(actual);
        if (sample && actual < cabinet_steering(std::uint8_t(sample - 1))) return 1;
        if (sample <= 48 && bits != 0xbf570a3du) return 2;
        if (sample >= 208 && bits != 0x3f570a3du) return 3;
        if (sample == 128 && bits != 0) return 4;
        if (sample > 0 && sample < 128 &&
            bits != (std::bit_cast<std::uint32_t>(cabinet_steering(std::uint8_t(256 - sample))) ^ 0x80000000u)) return 5;
#ifndef _EE
        // Independent host arithmetic; the EE fixture uses the complete
        // original-program comparison fingerprint instead of EE floating math.
        const int centered = int(sample) - 128;
        int magnitude = centered < 0 ? -centered : centered;
        if (magnitude > 80) magnitude = 80;
        volatile double product = double(magnitude) * 0.84;
        volatile double scaled = product / 80.0;
        const auto expected = std::bit_cast<std::uint32_t>(float(scaled)) ^
                              (centered < 0 ? 0x80000000u : 0u);
        if (bits != expected) return 6;
#endif
        hash = (hash ^ sample) * 1099511628211ull;
        hash = (hash ^ bits) * 1099511628211ull;
    }
    if (hash != 0x466a329bdf779c3aull) return 7;
    std::printf("NATIVE_CABINET_STEERING PASS cases=256 hash=%016llx\n",
                static_cast<unsigned long long>(hash));
}
