// Presentation only: the shared machine supplies already-clocked SCSP samples.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace rally {
class AudioOutput {
public:
    static constexpr unsigned kRate = 44100, kChannels = 2;
    static constexpr unsigned kPrimeFrames = kRate * 60 / 1000;
    static constexpr unsigned kMaximumFrames = kPrimeFrames * 4;
    ~AudioOutput() { close(); }
    AudioOutput() = default;
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;
    void open() {
        close();
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) fail();
        initialized_ = true;
        const SDL_AudioSpec format{SDL_AUDIO_F32, kChannels, kRate};
        stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, request, this);
        if (!stream_) fail(); // SDL opens this device paused; prime before playback.
    }
    void close() {
        if (stream_) SDL_DestroyAudioStream(stream_);
        stream_ = nullptr; started_ = false;
        if (initialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }
    void clear() {
        if (!stream_) return;
        if (!SDL_PauseAudioStreamDevice(stream_) || !SDL_ClearAudioStream(stream_) ||
            !SDL_SetAudioStreamFrequencyRatio(stream_, 1.0f)) fail();
        started_ = false;
    }
    void push(std::span<const float> samples) {
        if (!stream_) throw std::logic_error("SCSP audio output is not open");
        if (samples.size() % kChannels) throw std::invalid_argument("SCSP audio requires complete stereo frames");
        if (samples.empty()) return;
        const size_t frames = samples.size() / kChannels;
        received_frames_ += frames;
        const auto queued = queued_frames();
        if (frames > kMaximumFrames || queued > kMaximumFrames - frames) {
            // A debugger, window drag or scheduler stall must not replay a
            // stale backlog. Retain only a bounded tail of newly produced PCM.
            dropped_frames_ += queued;
            clear();
            if (frames > kPrimeFrames) {
                dropped_frames_ += frames - kPrimeFrames;
                samples = samples.last(kPrimeFrames * kChannels);
            }
        }
        if (!SDL_PutAudioStreamData(stream_, samples.data(), int(samples.size_bytes()))) fail();
        submitted_frames_ += samples.size() / kChannels;
        const auto buffered = queued_frames();
        if (!started_ && buffered >= kPrimeFrames) {
            if (!SDL_ResumeAudioStreamDevice(stream_)) fail();
            started_ = true;
        }
        if (started_) {
            // Match Daytona's bounded host-device drift trim. This changes
            // only presentation, never SCSP samples or the shared board clock.
            const float error = std::clamp((float(buffered) - kPrimeFrames) / kPrimeFrames, -1.0f, 1.0f);
            if (!SDL_SetAudioStreamFrequencyRatio(stream_, 1.0f + 0.005f * error)) fail();
        }
    }
    unsigned queued_frames() const {
        if (!stream_) return 0;
        const int bytes = SDL_GetAudioStreamQueued(stream_);
        if (bytes < 0) fail();
        return unsigned(bytes) / (kChannels * sizeof(float));
    }
    bool started() const { return started_; }
    uint64_t received_frames() const { return received_frames_; }
    uint64_t submitted_frames() const { return submitted_frames_; }
    uint64_t dropped_frames() const { return dropped_frames_; }
    // SDL reports estimated extra input bytes required by a playback request;
    // this can slightly overestimate resampler padding. It is a queue-starvation
    // diagnostic, not an exact count of missing hardware output samples.
    uint64_t shortfall_requests() const { return shortfall_requests_.load(std::memory_order_relaxed); }
    uint64_t requested_extra_bytes() const { return requested_extra_bytes_.load(std::memory_order_relaxed); }
private:
    static void SDLCALL request(void *context,SDL_AudioStream *,int additional,int) {
        if(additional<=0) return;
        auto &self=*static_cast<AudioOutput *>(context);
        self.shortfall_requests_.fetch_add(1,std::memory_order_relaxed);
        self.requested_extra_bytes_.fetch_add(uint64_t(additional),std::memory_order_relaxed);
        // Device callbacks observe presentation demand only. They neither run
        // game/sound CPUs nor inject substitute samples into the native output.
    }
    [[noreturn]] static void fail() { throw std::runtime_error(std::string("SDL audio: ") + SDL_GetError()); }
    SDL_AudioStream *stream_ = nullptr;
    bool initialized_ = false, started_ = false;
    uint64_t received_frames_ = 0, submitted_frames_ = 0, dropped_frames_ = 0;
    std::atomic<uint64_t> shortfall_requests_{0}, requested_extra_bytes_{0};
};
} // namespace rally
