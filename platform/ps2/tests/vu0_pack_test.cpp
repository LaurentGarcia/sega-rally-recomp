// ROM-free numerical fixture. The same source runs on the development host
// and as an EE ELF; only an EE run exercises the real COP2 instructions.
#include "vu0_pack.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>
#ifdef _EE
#include <kernel.h>
#endif

namespace {
using ps2::GsCoordinates;
using ps2::GsFixedCoordinates;
std::uint32_t cases = 0;
std::uint64_t fingerprint = 1469598103934665603ull;

// Independent binary32 oracle: shifting the significand implements floor(x*16)
// for legal nonnegative GS coordinates, including zero and subnormals. It does
// not depend on the EE FPU rounding mode or its denormal handling.
std::uint32_t oracle(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    const auto exponent = (bits >> 23) & 255u;
    if (exponent < 123) return 0;
    return ((bits & 0x7fffffu) | 0x800000u) >> (146u - exponent);
}

bool check_batch(const GsCoordinates *input, std::size_t count) {
    std::array<GsFixedCoordinates, 35> scalar{}, vector{};
    if (count > 33) return false;
    std::memset(scalar.data(), 0xa5, sizeof scalar);
    std::memset(vector.data(), 0xa5, sizeof vector);
    if (!ps2::quantize_gs_xy_checked({input, count}, {scalar.data() + 1, count}, false) ||
        !ps2::quantize_gs_xy_checked({input, count}, {vector.data() + 1, count}, true)) return false;
    if (std::memcmp(scalar.data(), vector.data(), sizeof scalar)) {
        std::printf("VU0 mismatch at case %lu, count %u\n", (unsigned long)cases, unsigned(count));
        for (std::size_t i = 0; i < count; ++i)
            for (unsigned k = 0; k < 4; ++k)
                if (scalar[i + 1].xy[k] != vector[i + 1].xy[k])
                    std::printf("  bits=%08lx scalar=%lu VU=%lu\n",
                        (unsigned long)std::bit_cast<std::uint32_t>(input[i].xy[k]),
                        (unsigned long)scalar[i + 1].xy[k], (unsigned long)vector[i + 1].xy[k]);
        return false;
    }
    for (unsigned k = 0; k < 4; ++k)
        if (vector[0].xy[k] != 0xa5a5a5a5u || vector[count + 1].xy[k] != 0xa5a5a5a5u) return false;
    for (std::size_t i = 0; i < count; ++i) {
        for (unsigned k = 0; k < 4; ++k) {
            if (vector[i + 1].xy[k] != oracle(input[i].xy[k])) return false;
            fingerprint = (fingerprint ^ vector[i + 1].xy[k]) * 1099511628211ull;
            ++cases;
        }
    }
    return true;
}

bool numerical_tests() {
    // Every GS 12.4 boundary, its immediately adjacent binary32 values and
    // the interval midpoint. Batch lengths exercise the unrolled kernel and
    // each tail length, including zero and the first/last valid coordinate.
    std::array<GsCoordinates, 33> input{};
    for (std::uint32_t boundary = 0; boundary < 65536; ++boundary) {
        const float coordinate = float(boundary) / 16.f;
        const auto bits = std::bit_cast<std::uint32_t>(coordinate);
        input[0] = {{coordinate, std::bit_cast<float>(bits + 1),
                     boundary ? std::bit_cast<float>(bits - 1) : -0.f,
                     coordinate + (1.f / 32.f)}};
        if (!check_batch(input.data(), 1)) return false;
    }
    std::uint32_t random = 0x187b45c9u;
    for (unsigned pass = 0; pass < 512; ++pass) {
        for (auto &pair : input) for (float &coordinate : pair.xy) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            // All positive normal exponents below 4096, plus subnormals.
            const auto bits = ((random % 139u) << 23) | ((random >> 8) & 0x7fffffu);
            coordinate = std::bit_cast<float>(bits);
        }
        if (!check_batch(input.data(), pass % 34)) return false;
    }
    const float positions[] = {-2048.f, -1024.f, -640.f, -496.f, -0.0625f,
        -0.05f, -0.03125f, -0.f, 0.f, 0.03125f, 0.05f, 0.0625f, 496.f, 640.f, 2047.9375f};
    for (float screen : positions) {
        // Match the renderer's scalar offset-before-quantization contract.
        const float translated = screen + 2048.f;
        input[0] = {{translated, translated, translated, translated}};
        if (!check_batch(input.data(), 1)) return false;
    }
    input[0] = {{std::numeric_limits<float>::denorm_min(), 0.f, -0.f,
                 std::bit_cast<float>(0x457fffffu)}};
    return check_batch(input.data(), 1);
}

bool invalid_tests() {
    std::array<GsCoordinates, 5> input{};
    std::array<GsFixedCoordinates, 5> output{};
    const float invalid[] = {-1.f, -0.0001f, std::bit_cast<float>(0x80000001u), 4096.f,
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()};
    for (bool use_vu : {false, true}) {
        for (float value : invalid) {
            input = {};
            input.back().xy[3] = value;
            std::memset(output.data(), 0xa5, sizeof output);
            if (ps2::quantize_gs_xy_checked(input, output, use_vu)) return false;
            for (const auto &pair : output) for (auto lane : pair.xy)
                if (lane != 0xa5a5a5a5u) return false;
        }
        input = {};
        if (ps2::quantize_gs_xy_checked(input, {output.data(), 4}, use_vu)) return false;
        if (ps2::quantize_gs_xy_checked(input,
            {reinterpret_cast<GsFixedCoordinates *>(input.data()), input.size()}, use_vu)) return false;
        if (ps2::quantize_gs_xy_checked({input.data(), 4},
            {reinterpret_cast<GsFixedCoordinates *>(input.data() + 1), 4}, use_vu)) return false;
        if (ps2::quantize_gs_xy_checked({input.data() + 1, 4},
            {reinterpret_cast<GsFixedCoordinates *>(input.data()), 4}, use_vu)) return false;
        if (!ps2::quantize_gs_xy_checked({}, {}, use_vu)) return false;
    }
    return true;
}

#ifdef _EE
std::uint32_t count_register() {
    std::uint32_t value;
    asm volatile("sync\n\tmfc0 %0, $9" : "=r"(value) : : "memory");
    return value;
}
void benchmark() {
    std::array<GsCoordinates, 64> input{};
    std::array<GsFixedCoordinates, 64> output{};
    for (unsigned i = 0; i < input.size(); ++i)
        input[i] = {{2048.f + i * 0.31f, 2048.f + i * 0.71f, 2048.f + i * 1.25f, 2048.f + i * 0.37f}};
    volatile std::uint32_t checksum = 0;
    for (unsigned count : {1u, 2u, 4u, 16u, 64u}) {
        std::uint32_t best[2] = {0xffffffffu, 0xffffffffu};
        for (unsigned trial = 0; trial < 5; ++trial) for (unsigned vector = 0; vector < 2; ++vector) {
            const auto begin = count_register();
            for (unsigned repeat = 0; repeat < 256; ++repeat) {
                if (vector) ps2::quantize_gs_xy_vu0(input.data(), output.data(), count);
                else ps2::quantize_gs_xy_scalar(input.data(), output.data(), count);
            }
            const auto elapsed = count_register() - begin;
            if (elapsed < best[vector]) best[vector] = elapsed;
            checksum = output[count - 1].xy[3];
        }
        std::printf("VU0 BENCH pairs=%u repeats=256 scalar_count=%lu vu0_count=%lu\n",
                    count, (unsigned long)best[0], (unsigned long)best[1]);
    }
    std::printf("VU0 BENCH checksum=%lu (CP0 Count; emulator counts are not hardware timing)\n",
                (unsigned long)checksum);
}
#endif
} // namespace

int main() {
    std::printf("VU0 PACK BEGIN backend=%s\n", ps2::vu0_pack_available() ? "EE COP2" : "host scalar");
    std::fflush(stdout);
    const bool passed = numerical_tests() && invalid_tests();
    std::printf("VU0 PACK %s components=%lu hash=%08lx%08lx\n", passed ? "PASS" : "FAIL",
        (unsigned long)cases, (unsigned long)(fingerprint >> 32), (unsigned long)(fingerprint & 0xffffffffu));
    std::fflush(stdout);
#ifdef _EE
    if (passed) benchmark();
    std::fflush(stdout);
    SleepThread();
#endif
    return passed ? 0 : 1;
}
