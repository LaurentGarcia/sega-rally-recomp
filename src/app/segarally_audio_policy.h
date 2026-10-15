// Presentation queue policy; never changes generated PCM or the board clock.
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace rally {
struct AudioQueuePolicy {
    static constexpr unsigned kRate=44100;
    static constexpr unsigned kBaseFrames=kRate*60/1000;
    static constexpr unsigned kMaximumFrames=kBaseFrames*4;
    unsigned target_frames=kBaseFrames,burst_frames=0;

    void configure(int output_rate,int output_frames) {
        if(output_rate<=0 || output_frames<=0)
            throw std::runtime_error("SDL audio returned an invalid device period");
        // Convert one actual output callback period to nominal source frames,
        // rounding upward. uint64_t safely holds every positive int input.
        const uint64_t burst=(uint64_t(output_frames)*kRate+unsigned(output_rate)-1)/unsigned(output_rate);
        // Keep at least one complete target's room below the fixed backlog cap.
        // Otherwise a push crossing the prime threshold could first trigger
        // backlog clearing, preventing startup with sufficiently large blocks.
        if(burst>kMaximumFrames/2-kBaseFrames)
            throw std::runtime_error("SDL audio device period exceeds bounded playback queue support");
        burst_frames=unsigned(burst);target_frames=kBaseFrames+burst_frames;
    }
    float ratio(unsigned buffered) const {
        const float error=std::clamp((float(buffered)-target_frames)/target_frames,-1.0f,1.0f);
        return 1.0f+0.005f*error;
    }
};
} // namespace rally
