// Original bit-pattern cases. No ROM values or guest instruction encodings.
#include "car_pose.h"
#include <cstdio>
#ifdef _EE
#include <kernel.h>
#endif

int main() {
    using namespace ps2::native;
    unsigned checks = 0;
    CarPoseState state;
    state.words.position = {0x80000000, 0, 0x7fc12345};
    state.words.motion = {1, 0x7f800000, 0xff800000};
    state.words.orientation = {0x3f800000, 0xbf800000, 0xffffffff};
    for (unsigned i = 0; i < state.words.attributes.size(); ++i)
        state.words.attributes[i] = 0x31415926u ^ (i * 0x12345u);
    state.progress = 0x98765432;
    const auto original_words = state.words;
    for (unsigned source_byte = 0; source_byte < 256; ++source_byte)
        for (unsigned old_byte = 0; old_byte < 256; ++old_byte) {
            RenderPose pose;
            state.flags = 0xabcd0055u | (source_byte << 8);
            pose.flags = 0x12340023u | (old_byte << 8);
            const auto expected_state_flags = 0xabcd0055u |
                (((source_byte & 63u) | (old_byte & 192u)) << 8);
            publish_car_pose(state, pose);
            if (state.words != original_words || state.flags != expected_state_flags ||
                state.progress != 0x98765432 || pose.words != original_words ||
                pose.flags != (expected_state_flags | 128u) || pose.progress != state.progress) return 1;
            // Re-publication is idempotent for state and output.
            const auto once = pose;
            publish_car_pose(state, pose);
            if (pose != once || state.flags != expected_state_flags) return 1;
            ++checks;
        }
    std::printf("Native car pose PASS: %u flag pairs, raw value bits and repeated publication\n", checks);
    std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
