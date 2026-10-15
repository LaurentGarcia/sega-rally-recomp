// Optional diagnostic artifact: exact normalized stereo samples as float WAV.
#pragma once
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>

namespace rally {
class AudioCapture {
public:
    explicit AudioCapture(const std::filesystem::path &path,unsigned rate=44100,unsigned channels=2)
        : channels_(valid_channels(channels)), rate_(valid_rate(rate)), file_(path, std::ios::binary | std::ios::trunc) {
        if (!file_) throw std::runtime_error("Cannot open SCSP WAV capture");
        file_.write("RIFF",4); word(36,4); file_.write("WAVEfmt ",8); word(16,4);
        word(3,2); word(channels_,2); word(rate_,4); word(rate_*channels_*4,4); word(channels_*4,2); word(32,2);
        file_.write("data",4); word(0,4);
    }
    ~AudioCapture() { if (!finished_) { try { finish(); } catch (...) {} } }
    void push(std::span<const float> samples) {
        if (finished_) throw std::logic_error("SCSP WAV capture already finished");
        if (samples.size()%channels_) throw std::invalid_argument("Audio WAV requires complete channel frames");
        if (samples.size() > (std::numeric_limits<uint32_t>::max()-36-bytes_)/4)
            throw std::runtime_error("SCSP WAV capture exceeds RIFF size limit");
        for (float value : samples) word(std::bit_cast<uint32_t>(value),4);
        bytes_ += uint32_t(samples.size()*4);
        if (!file_) throw std::runtime_error("Cannot write SCSP WAV capture");
    }
    void finish() {
        if (finished_) return;
        file_.seekp(4); word(bytes_+36,4); file_.seekp(40); word(bytes_,4); file_.flush();
        if (!file_) throw std::runtime_error("Cannot finalize SCSP WAV capture");
        finished_ = true;
    }
    uint64_t frames() const { return bytes_/(channels_*4); }
private:
    static unsigned valid_channels(unsigned value) {
        if(!value||value>8) throw std::invalid_argument("Audio WAV supports 1..8 channels");return value;
    }
    static unsigned valid_rate(unsigned value) {
        if(value<8000||value>192000) throw std::invalid_argument("Audio WAV supports 8000..192000Hz");return value;
    }
    void word(uint32_t value,unsigned count) {
        for(unsigned i=0;i<count;++i) { file_.put(char(value&255));value>>=8; }
    }
    unsigned channels_,rate_;
    std::ofstream file_;
    uint32_t bytes_=0;
    bool finished_=false;
};
} // namespace rally
