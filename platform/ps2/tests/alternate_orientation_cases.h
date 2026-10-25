// Original input vectors; no game data or captured state.
#pragma once
#include <array>
#include <bit>
#include <cstdint>

namespace alternate_cases {
using Triple = std::array<std::uint32_t, 3>;
struct Input { Triple current, candidate; };
inline constexpr std::uint32_t limit = 0x40c90fdbu;

template<class Visit> bool probe(Visit visit) {
    // Subnormal/normal edges, step threshold neighbors, pi/turn neighbors,
    // cancellation and signed zero. Every sign pairing is included.
    constexpr std::array<std::uint32_t, 26> magnitudes{
        0,1,2,0x3fffff,0x7fffff,0x800000,0x800001,
        0x32000000,0x33800000,0x3c8f177c,0x3c8f177d,0x3c8f177e,
        0x3d000000,0x3e800000,0x3f000000,0x3f7fffff,0x3f800000,
        0x3f800001,0x40000000,0x40490fd9,0x40490fda,0x40490fdb,
        0x40490fdc,0x40c90fd9,0x40c90fda,0x40c90fdb};
    for (auto a : magnitudes) for (auto b : magnitudes)
        for (unsigned signs = 0; signs < 4; ++signs) {
            a &= 0x7fffffff; b &= 0x7fffffff;
            const auto current = a | ((signs & 1) ? 0x80000000u : 0);
            const auto candidate = b | ((signs & 2) ? 0x80000000u : 0);
            if (!visit(Input{{current,current,current},{candidate,candidate,candidate}})) return false;
        }
    std::uint32_t random = 0x91e8d647u;
    auto next = [&]() { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    for (unsigned i = 0; i < 1024; ++i) {
        Input input{};
        for (auto *triple : {&input.current, &input.candidate})
            for (auto &word : *triple) {
                const auto value = next();
                word = (value % (limit + 1)) | (value & 0x80000000u);
            }
        if (!visit(input)) return false;
    }
    return true;
}

template<class Visit> bool extended(Visit visit) {
    std::uint32_t random = 0x6c01793du;
    auto next = [&]() { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    for (unsigned i = 0; i < 50000; ++i) {
        Input input{};
        for (auto *triple : {&input.current, &input.candidate})
            for (auto &word : *triple) {
                const auto value = next();
                // Alternate broad encodings with ordinary angular magnitudes.
                const auto magnitude = i & 1 ? value % (limit + 1)
                    : 0x3c000000u + value % (limit - 0x3c000000u + 1);
                word = magnitude | (value & 0x80000000u);
            }
        if (!visit(input)) return false;
    }
    // Sweep both signs across each strict comparison and domain endpoint.
    for (auto center : {0x3c8f177du,0x40490fdbu,0x40c90fdbu})
        for (int offset = -2048; offset <= 2048; ++offset)
            for (unsigned signs = 0; signs < 4; ++signs) {
                const auto value = std::uint32_t(std::int64_t(center) + offset);
                const auto current = value | ((signs & 1) ? 0x80000000u : 0);
                const auto candidate = (signs & 2) ? 0x80000000u : 0;
                if (!visit(Input{{current,current,current},{candidate,candidate,candidate}})) return false;
            }
    return true;
}

#if !defined(_EE)
// Independent host arithmetic definition, deliberately separate from the
// native fixed-grid integer representation. Explicit stores prevent fusion.
inline float fold(float value) {
    constexpr double pi = std::bit_cast<double>(0x400921fb54442d18ull);
    constexpr double turn = std::bit_cast<double>(0x401921fb54442d18ull);
    if (double(value) > pi) return float(double(value) - turn);
    if (double(value) < -pi) return float(double(value) + turn);
    return value;
}
inline bool mathematical(const Input &input, Triple &folded, Triple &result) {
    constexpr double scale = std::bit_cast<double>(0x3feccccccccccccdull);
    constexpr double step = std::bit_cast<double>(0x3f91e2efa68c6f10ull);
    for (unsigned i = 0; i < 3; ++i)
        if ((input.current[i] & 0x7fffffffu) > limit ||
            (input.candidate[i] & 0x7fffffffu) > limit) return false;
    for (unsigned i = 0; i < 3; ++i) {
        const auto current = std::bit_cast<float>(input.current[i]);
        const auto candidate = fold(std::bit_cast<float>(input.candidate[i]));
        volatile float raw_difference = current - candidate;
        if ((std::bit_cast<std::uint32_t>(float(raw_difference)) & 0x7fffffffu) > limit) return false;
        const auto delta = fold(raw_difference);
        folded[i] = std::bit_cast<std::uint32_t>(candidate);
        float output;
        if (i != 1) {
            volatile double product = double(delta) * scale;
            volatile double sum = double(candidate) + product;
            output = float(sum);
        } else if (double(delta < 0 ? -delta : delta) < step) {
            output = candidate;
        } else {
            volatile double difference = double(current) - (delta < 0 ? -step : step);
            output = float(difference);
        }
        result[i] = std::bit_cast<std::uint32_t>(output);
    }
    return true;
}
#endif
} // namespace alternate_cases
