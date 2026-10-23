// Streaming 44.1 -> 48 kHz presentation resampler. Game/SCSP clocks are unchanged.
#pragma once
#include <cstdint>
#include <cmath>

namespace ps2 {
class PcmResampler {
public:
    template<class Emit> bool frame(float left, float right, Emit emit) {
        left = clean(left); right = clean(right);
        if (!started_) {
            previous_[0] = left; previous_[1] = right;
            started_ = true; phase_ = 44100;
            return emit(quantize(left), quantize(right));
        }
        while (phase_ <= 48000) {
            const float fraction = float(phase_) / 48000.0f;
            if (!emit(quantize(previous_[0] + (left - previous_[0]) * fraction),
                      quantize(previous_[1] + (right - previous_[1]) * fraction))) return false;
            phase_ += 44100;
        }
        phase_ -= 48000;
        previous_[0] = left; previous_[1] = right;
        return true;
    }
private:
    static float clean(float sample) {
        if (!std::isfinite(sample)) return 0;
        return sample > 1 ? 1 : sample < -1 ? -1 : sample;
    }
    static int16_t quantize(float sample) {
        const int value = int(sample * 32768.0f);
        return int16_t(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
    }
    float previous_[2]{};
    unsigned phase_ = 0;
    bool started_ = false;
};
} // namespace ps2
