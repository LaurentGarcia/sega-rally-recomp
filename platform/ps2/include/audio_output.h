#pragma once
#include <cstdint>
#include <memory>
#include <span>

namespace ps2 {
class AudioOutput {
public:
    struct Stats {
        uint64_t submitted_frames = 0;
        uint32_t queued = 0, consumed = 0, silent_frames = 0, blocks = 0;
        // A delivered DMA completion found the previous completion unserviced.
        // This does not count every possible hardware underrun or recover audio.
        uint32_t late_service_events = 0;
    };
    AudioOutput();
    ~AudioOutput();
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;
    // Call after pad/storage initialization: this never resets the IOP.
    bool initialize();
    // Interleaved normalized stereo at 44,100 Hz. Backpressure is bounded to
    // one second per RPC packet while the IOP responds; the SDK's synchronous
    // RPC itself has no deadline. No game sample is silently discarded.
    bool submit(std::span<const float> stereo);
    bool flush();
    Stats stats();
    const char *error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ps2
