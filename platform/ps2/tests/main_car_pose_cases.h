// Original synthetic raw-bit cases, independent of game assets.
#pragma once
#include "main_car_pose.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace main_car_pose_cases {
using namespace ps2::native;
using Words = std::array<std::uint32_t, 22>;
struct Case {
    MainCarPoseState state;
    MainCarExternalState external;
    RenderPose previous;
    Words expected;
};
inline std::uint32_t next(std::uint32_t &state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
inline Words words(const RenderPose &pose) {
    Words result;
    std::copy(pose.words.position.begin(), pose.words.position.end(), result.begin());
    std::copy(pose.words.motion.begin(), pose.words.motion.end(), result.begin() + 3);
    std::copy(pose.words.orientation.begin(), pose.words.orientation.end(), result.begin() + 6);
    std::copy(pose.words.attributes.begin(), pose.words.attributes.end(), result.begin() + 9);
    result[20] = pose.flags; result[21] = pose.progress;
    return result;
}
inline Case make(const Words &input, std::uint32_t &random) {
    Case result{};
    std::copy_n(input.begin(), 3, result.state.position.begin());
    std::copy_n(input.begin() + 3, 3, result.state.motion.begin());
    std::copy_n(input.begin() + 6, 3, result.state.orientation.begin());
    std::copy_n(input.begin() + 9, 7, result.state.attributes.begin());
    std::copy_n(input.begin() + 16, 4, result.external.begin());
    result.state.flags = input[20]; result.state.index = input[21];
    for (auto &word : result.previous.words.position) word = next(random);
    for (auto &word : result.previous.words.motion) word = next(random);
    for (auto &word : result.previous.words.orientation) word = next(random);
    for (auto &word : result.previous.words.attributes) word = next(random);
    result.previous.flags = next(random); result.previous.progress = next(random);
    result.expected = input;
    return result;
}
template<class Test> bool each(Test test, bool extended) {
    std::uint32_t random = 0x527bad19u;
    Words input;
    // Exhaust every source low halfword on the host. The bounded EE subset
    // covers every byte, replicated into both halves of that halfword.
    for (unsigned i = 0; i < (extended ? 65536u : 256u); ++i) {
        for (auto &word : input) word = next(random);
        input[20] = (input[20] & 0xffff0000u) | (extended ? i : i * 257u);
        if (!test(make(input, random))) return false;
    }
    // Set and clear every bit independently at every one of the 22 inputs.
    for (unsigned lane = 0; lane < input.size(); ++lane)
        for (unsigned bit = 0; bit < 32; ++bit)
            for (unsigned complement = 0; complement < 2; ++complement) {
                input.fill(complement ? 0xffffffffu : 0u);
                input[lane] ^= std::uint32_t(1) << bit;
                if (!test(make(input, random))) return false;
            }
    constexpr std::array<std::uint32_t, 12> boundaries{
        0u, 0x80000000u, 1u, 0x80000001u, 0x007fffffu, 0x00800000u,
        0x7f800000u, 0xff800000u, 0x7f800001u, 0x7fc12345u, 0xffffffffu, 0xaaaaaaaau};
    for (unsigned lane = 0; lane < input.size(); ++lane)
        for (const auto value : boundaries) {
            for (auto &word : input) word = next(random);
            input[lane] = value;
            if (!test(make(input, random))) return false;
        }
    for (unsigned i = 0; i < (extended ? 10000u : 256u); ++i) {
        for (auto &word : input) word = next(random);
        if (!test(make(input, random))) return false;
    }
    return true;
}
inline void hash_words(std::uint64_t &hash, const Words &input) {
    for (auto word : input) hash = (hash ^ word) * 1099511628211ull;
}
} // namespace main_car_pose_cases
