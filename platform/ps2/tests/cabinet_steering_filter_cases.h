#pragma once
#include <array>
#include <bit>
#include <cstdint>

namespace steering_filter_cases {
struct Input { std::uint32_t history; std::uint8_t sample; };
template<class Visit> bool probe(Visit visit) {
    constexpr std::array<std::uint32_t, 20> histories{
        0,0x80000000u,1,0x80000001u,0x7fffffu,0x807fffffu,0x800000u,0x80800000u,
        0x32000000u,0xb2000000u,0x3d000000u,0xbd000000u,0x3f000000u,0xbf000000u,
        0x3f570a3du,0xbf570a3du,0x3f7fffffu,0xbf7fffffu,0x3f800000u,0xbf800000u};
    for (auto history : histories) for (unsigned sample = 0; sample < 256; ++sample)
        if (!visit(Input{history,std::uint8_t(sample)})) return false;
    return true;
}
template<class Visit> bool extended(Visit visit) {
    std::uint32_t random = 0x7c192d43u;
    const auto next = [&]() { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    for (unsigned i = 0; i < 96; ++i) {
        const auto word = next();
        const auto history = (word % 0x3f800001u) | (word & 0x80000000u);
        for (unsigned sample = 0; sample < 256; ++sample)
            if (!visit(Input{history,std::uint8_t(sample)})) return false;
    }
    for (unsigned i = 0; i < 20000; ++i) {
        const auto word = next();
        if (!visit(Input{(word % 0x3f800001u) | (word & 0x80000000u),std::uint8_t(next())})) return false;
    }
    return true;
}
// Each callback returns the actual next state, so these are stateful sequences.
template<class Step> bool sequences(Step step, unsigned length) {
    for (auto initial : {0u,0x80000000u,0x3f800000u,0xbf800000u}) {
        std::uint32_t state = initial;
        for (unsigned frame = 0; frame < length; ++frame) {
            const auto sample = std::uint8_t(frame < length/3 ? (frame & 255)
                : frame < 2*length/3 ? ((frame & 1) ? 0 : 255) : 128);
            if (!step(Input{state,sample},state)) return false;
        }
    }
    return true;
}
#if !defined(_EE)
inline std::uint32_t mathematical(Input input) {
    const int centered = int(input.sample) - 128;
    int magnitude = centered < 0 ? -centered : centered;
    if (magnitude > 80) magnitude = 80;
    volatile double mapped_product = double(magnitude) * 0.84;
    volatile double mapped_scaled = mapped_product / 80.0;
    const auto mapped_bits = std::bit_cast<std::uint32_t>(float(mapped_scaled)) ^
                             (centered < 0 ? 0x80000000u : 0);
    const auto old = std::bit_cast<float>(input.history);
    volatile float difference = std::bit_cast<float>(mapped_bits) - old;
    volatile double product = double(difference) * 0.9;
    volatile double sum = double(old) + product;
    return std::bit_cast<std::uint32_t>(float(sum));
}
#endif
} // namespace steering_filter_cases
