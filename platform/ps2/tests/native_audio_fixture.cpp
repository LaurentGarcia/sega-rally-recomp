// ROM-free SPU2 transport execution check. This diagnostic waveform is not
// game audio and its successful playback does not establish arcade fidelity.
#include "audio_output.h"
#include <kernel.h>
#include <timer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

int main() {
    std::puts("PS2 audio fixture: initializing open IOP/SPU2 transport");
    std::fflush(stdout);
    ps2::AudioOutput output;
    if (!output.initialize()) {
        std::printf("PS2 audio fixture FAIL initialize: %s\n", output.error());
        return 1;
    }
    std::array<float, 256> pcm;
    constexpr unsigned frames = 44101;
    for (unsigned begin = 0; begin < frames; begin += 128) {
        const unsigned count = std::min(128u, frames - begin);
        for (unsigned i = 0; i < count; ++i) {
            const double phase = double(begin + i) / 44100;
            pcm[i * 2] = float(0.125 * std::sin(2 * 3.14159265358979323846 * 440 * phase));
            pcm[i * 2 + 1] = float(0.125 * std::sin(2 * 3.14159265358979323846 * 660 * phase));
        }
        if (!output.submit({pcm.data(), count * 2})) {
            std::printf("PS2 audio fixture FAIL submit: %s\n", output.error());
            return 2;
        }
    }
    if (!output.flush()) {
        std::printf("PS2 audio fixture FAIL flush: %s\n", output.error());
        return 3;
    }
    const uint64_t start = GetTimerSystemTime();
    ps2::AudioOutput::Stats stats{};
    do {
        stats = output.stats();
        if (*output.error()) {
            std::printf("PS2 audio fixture FAIL stats: %s\n", output.error());
            return 4;
        }
        if (!stats.queued && stats.consumed == stats.submitted_frames) break;
    } while (GetTimerSystemTime() - start < uint64_t(kBUSCLK) * 3);
    const bool valid = stats.submitted_frames == 48001 && stats.consumed == 48001 && !stats.queued;
    std::printf("PS2 audio fixture %s submitted=%llu consumed=%u queued=%u silent=%u blocks=%u\n",
                valid ? "PASS" : "FAIL", (unsigned long long)stats.submitted_frames,
                unsigned(stats.consumed), unsigned(stats.queued), unsigned(stats.silent_frames), unsigned(stats.blocks));
    std::fflush(stdout);
    return valid ? 0 : 5;
}
